#include "patches/input_update.h"

#include "log.h"

#include <thod2/interop/hod2_addresses.h>
#include <thod2/interop/iat_hook.h>
#include <thod2/interop/memory_patch.h>
#include <thod2/interop/vtable_hook.h>
#include <thod2/platform/input.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace thod2::core::patches {
namespace {

namespace hod2 = interop::hod2;
namespace input = platform::input;

template <class T>
T* At(std::uintptr_t address) {
  return reinterpret_cast<T*>(address);
}

input::InputMemory GameInputMemory() {
  input::InputMemory m{};
  m.player1_mode = At<std::uint32_t>(hod2::kInputPlayer1Mode);
  m.player2_mode = At<std::uint32_t>(hod2::kInputPlayer2Mode);
  m.player1_index = At<std::int32_t>(hod2::kInputPlayer1Index);
  m.player2_index = At<std::int32_t>(hod2::kInputPlayer2Index);
  m.joystick_count = At<std::uint32_t>(hod2::kInputJoystickCount);
  m.joysticks = At<IDirectInputDevice7A*>(hod2::kInputJoysticks);
  m.keyboard = At<IDirectInputDevice7A*>(hod2::kInputKeyboard);
  m.mouse = At<IDirectInputDevice7A*>(hod2::kInputMouse);
  m.joystick_records = At<input::ButtonRecord>(hod2::kInputJoystickRecords);
  m.mouse_record = At<input::ButtonRecord>(hod2::kInputMouseRecord);
  m.keyboard_records = At<input::ButtonRecord>(hod2::kInputKeyboardRecords);
  m.player_records = At<input::ButtonRecord>(hod2::kInputPlayerRecords);
  m.slot_states = At<std::uint8_t>(hod2::kInputSlotStates);
  m.cursor = reinterpret_cast<std::int32_t(*)[2]>(hod2::kInputCursor);
  m.trigger_flags = At<std::uint32_t>(hod2::kInputTriggerFlags);
  m.reload_flags = At<std::uint32_t>(hod2::kInputReloadFlags);
  m.key_state = At<std::uint8_t>(hod2::kInputKeyState);
  m.key_events = At<DIDEVICEOBJECTDATA>(hod2::kInputKeyEvents);
  m.key_chars = At<const std::uint32_t>(hod2::kInputKeyChars);
  m.name_entry = At<std::uint8_t>(hod2::kInputNameEntry);
  m.keyboard_previous = At<std::uint32_t>(hod2::kInputKeyboardPrevious);
  m.mouse_previous = At<std::uint32_t>(hod2::kInputMousePrevious);
  m.joystick_previous = At<std::uint32_t>(hod2::kInputJoystickPrevious);
  m.joystick_button_map = At<const std::uint32_t>(hod2::kInputJoystickButtonMap);
  m.cursor_callback = At<std::uintptr_t>(hod2::kInputCursorCallback);
  m.window = At<HWND>(hod2::kMainWindow);
  return m;
}

using UpdateInputFn = void(__cdecl*)();
using ReaderFn = int(__cdecl*)(input::ButtonRecord*, std::uint32_t, std::uint32_t);
using CallbackFn = int(__cdecl*)();

// Calls the game directly: the normal host of the reimplementation.
class GameHost : public input::InputHost {
 public:
  int CallCursorCallback(std::uintptr_t callback) override {
    return reinterpret_cast<CallbackFn>(callback)();
  }
  int ReadGunJoystick(input::ButtonRecord* record, std::uint32_t device,
                      std::uint32_t slot) override {
    return reinterpret_cast<ReaderFn>(hod2::kReadGunJoystick)(record, device, slot);
  }
  int ReadSegaGun(input::ButtonRecord* record, std::uint32_t gun, std::uint32_t slot) override {
    return reinterpret_cast<ReaderFn>(hod2::kReadSegaGun)(record, gun, slot);
  }
};

input::InputMemory g_memory;
GameHost g_game_host;

void __cdecl ReimplementedUpdateInput() {
  input::UpdateInput(g_memory, g_game_host);
}

// --- Compare mode -----------------------------------------------------------

// One call the original update made to DirectInput, Win32 or the game.
struct CallRecord {
  enum class Kind { Acquire, Poll, GetDeviceState, GetDeviceData, GetCursorPos, ClientToScreen, Callback };
  Kind kind;
  const void* target;      // device, window or callback
  DWORD size = 0;          // state size / object size
  DWORD count = 0;         // GetDeviceData: count passed in
  std::int64_t result = 0;  // HRESULT, BOOL or int
  std::vector<std::uint8_t> data;  // what the call wrote
};

const char* KindName(CallRecord::Kind kind) {
  switch (kind) {
    case CallRecord::Kind::Acquire: return "Acquire";
    case CallRecord::Kind::Poll: return "Poll";
    case CallRecord::Kind::GetDeviceState: return "GetDeviceState";
    case CallRecord::Kind::GetDeviceData: return "GetDeviceData";
    case CallRecord::Kind::GetCursorPos: return "GetCursorPos";
    case CallRecord::Kind::ClientToScreen: return "ClientToScreen";
    case CallRecord::Kind::Callback: return "CursorCallback";
  }
  return "?";
}

bool g_recording = false;
std::vector<CallRecord> g_calls;

using AcquireFn = HRESULT(__stdcall*)(IDirectInputDevice7A*);
using GetDeviceStateFn = HRESULT(__stdcall*)(IDirectInputDevice7A*, DWORD, void*);
using GetDeviceDataFn = HRESULT(__stdcall*)(IDirectInputDevice7A*, DWORD, DIDEVICEOBJECTDATA*,
                                            DWORD*, DWORD);
using GetCursorPosFn = BOOL(WINAPI*)(POINT*);
using ClientToScreenFn = BOOL(WINAPI*)(HWND, POINT*);

// IDirectInputDevice7A vtable indices.
constexpr std::size_t kAcquireIndex = 7;
constexpr std::size_t kGetDeviceStateIndex = 9;
constexpr std::size_t kGetDeviceDataIndex = 10;
constexpr std::size_t kPollIndex = 25;

// Original entries of each device vtable we hooked (devices may share one).
struct VtableHooks {
  void** vtable = nullptr;
  AcquireFn acquire = nullptr;
  GetDeviceStateFn get_device_state = nullptr;
  GetDeviceDataFn get_device_data = nullptr;
  AcquireFn poll = nullptr;
};
std::array<VtableHooks, 4> g_vtables;

GetCursorPosFn g_get_cursor_pos = nullptr;
ClientToScreenFn g_client_to_screen = nullptr;

const VtableHooks* FindVtable(IDirectInputDevice7A* device) {
  void** vtable = *reinterpret_cast<void***>(device);
  for (const VtableHooks& hooks : g_vtables) {
    if (hooks.vtable == vtable) {
      return &hooks;
    }
  }
  return nullptr;
}

CallRecord& Record(CallRecord::Kind kind, const void* target, std::int64_t result) {
  CallRecord& call = g_calls.emplace_back();
  call.kind = kind;
  call.target = target;
  call.result = result;
  return call;
}

HRESULT __stdcall RecordAcquire(IDirectInputDevice7A* device) {
  const HRESULT hr = FindVtable(device)->acquire(device);
  if (g_recording) {
    Record(CallRecord::Kind::Acquire, device, hr);
  }
  return hr;
}

HRESULT __stdcall RecordPoll(IDirectInputDevice7A* device) {
  const HRESULT hr = FindVtable(device)->poll(device);
  if (g_recording) {
    Record(CallRecord::Kind::Poll, device, hr);
  }
  return hr;
}

HRESULT __stdcall RecordGetDeviceState(IDirectInputDevice7A* device, DWORD size, void* data) {
  const HRESULT hr = FindVtable(device)->get_device_state(device, size, data);
  if (g_recording) {
    CallRecord& call = Record(CallRecord::Kind::GetDeviceState, device, hr);
    call.size = size;
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    call.data.assign(bytes, bytes + size);
  }
  return hr;
}

HRESULT __stdcall RecordGetDeviceData(IDirectInputDevice7A* device, DWORD object_size,
                                      DIDEVICEOBJECTDATA* data, DWORD* count, DWORD flags) {
  const DWORD count_in = count ? *count : 0;
  const HRESULT hr = FindVtable(device)->get_device_data(device, object_size, data, count, flags);
  if (g_recording) {
    CallRecord& call = Record(CallRecord::Kind::GetDeviceData, device, hr);
    call.size = object_size;
    call.count = count_in;
    // Keep the whole buffer the caller passed: a failing call may still have written to it.
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(data);
    call.data.assign(reinterpret_cast<const std::uint8_t*>(count),
                     reinterpret_cast<const std::uint8_t*>(count) + sizeof(DWORD));
    if (data) {
      call.data.insert(call.data.end(), bytes, bytes + static_cast<std::size_t>(object_size) * count_in);
    }
  }
  return hr;
}

BOOL WINAPI RecordGetCursorPos(POINT* point) {
  const BOOL ok = g_get_cursor_pos(point);
  if (g_recording) {
    CallRecord& call = Record(CallRecord::Kind::GetCursorPos, nullptr, ok);
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(point);
    call.data.assign(bytes, bytes + sizeof(POINT));
  }
  return ok;
}

BOOL WINAPI RecordClientToScreen(HWND window, POINT* point) {
  const BOOL ok = g_client_to_screen(window, point);
  if (g_recording) {
    CallRecord& call = Record(CallRecord::Kind::ClientToScreen, window, ok);
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(point);
    call.data.assign(bytes, bytes + sizeof(POINT));
  }
  return ok;
}

std::uintptr_t g_real_callback = 0;

int __cdecl RecordCursorCallback() {
  const int result = reinterpret_cast<CallbackFn>(g_real_callback)();
  Record(CallRecord::Kind::Callback, reinterpret_cast<const void*>(g_real_callback), result);
  return result;
}

void HookDevice(IDirectInputDevice7A* device) {
  if (device == nullptr || FindVtable(device) != nullptr) {
    return;
  }
  for (VtableHooks& hooks : g_vtables) {
    if (hooks.vtable != nullptr) {
      continue;
    }
    hooks.vtable = *reinterpret_cast<void***>(device);
    hooks.acquire = reinterpret_cast<AcquireFn>(
        interop::HookVtable(device, kAcquireIndex, reinterpret_cast<void*>(&RecordAcquire)));
    hooks.get_device_state = reinterpret_cast<GetDeviceStateFn>(interop::HookVtable(
        device, kGetDeviceStateIndex, reinterpret_cast<void*>(&RecordGetDeviceState)));
    hooks.get_device_data = reinterpret_cast<GetDeviceDataFn>(interop::HookVtable(
        device, kGetDeviceDataIndex, reinterpret_cast<void*>(&RecordGetDeviceData)));
    hooks.poll = reinterpret_cast<AcquireFn>(
        interop::HookVtable(device, kPollIndex, reinterpret_cast<void*>(&RecordPoll)));
    Log(LogLevel::Info, "input_compare: hooked device vtable 0x{:X}",
        reinterpret_cast<std::uintptr_t>(hooks.vtable));
    return;
  }
  Log(LogLevel::Warn, "input_compare: too many device vtables, calls not recorded");
}

// Plays the recorded calls back to our UpdateInput and notes any difference
// in the sequence of calls.
class ReplayHost : public input::InputHost {
 public:
  void Reset() {
    next_ = 0;
    divergence_.clear();
    unsupported_ = false;
  }
  bool AllConsumed() const { return next_ == g_calls.size(); }
  std::size_t Consumed() const { return next_; }
  const std::string& divergence() const { return divergence_; }
  bool unsupported() const { return unsupported_; }

  HRESULT Acquire(IDirectInputDevice7A* device) override {
    const CallRecord* call = Next(CallRecord::Kind::Acquire, device);
    return call ? static_cast<HRESULT>(call->result) : E_FAIL;
  }
  HRESULT Poll(IDirectInputDevice7A* device) override {
    const CallRecord* call = Next(CallRecord::Kind::Poll, device);
    return call ? static_cast<HRESULT>(call->result) : E_FAIL;
  }
  HRESULT GetDeviceState(IDirectInputDevice7A* device, DWORD size, void* data) override {
    const CallRecord* call = Next(CallRecord::Kind::GetDeviceState, device);
    if (!call || !Expect(call->size == size, "GetDeviceState size")) {
      return E_FAIL;
    }
    std::memcpy(data, call->data.data(), size);
    return static_cast<HRESULT>(call->result);
  }
  HRESULT GetDeviceData(IDirectInputDevice7A* device, DWORD object_size, DIDEVICEOBJECTDATA* data,
                        DWORD* count, DWORD /*flags*/) override {
    const CallRecord* call = Next(CallRecord::Kind::GetDeviceData, device);
    if (!call || !Expect(call->size == object_size && call->count == *count, "GetDeviceData arguments")) {
      return E_FAIL;
    }
    std::memcpy(count, call->data.data(), sizeof(DWORD));
    std::memcpy(data, call->data.data() + sizeof(DWORD), call->data.size() - sizeof(DWORD));
    return static_cast<HRESULT>(call->result);
  }
  BOOL GetCursorPos(POINT* point) override {
    const CallRecord* call = Next(CallRecord::Kind::GetCursorPos, nullptr);
    if (!call) {
      return FALSE;
    }
    std::memcpy(point, call->data.data(), sizeof(POINT));
    return static_cast<BOOL>(call->result);
  }
  BOOL ClientToScreen(HWND window, POINT* point) override {
    const CallRecord* call = Next(CallRecord::Kind::ClientToScreen, window);
    if (!call) {
      return FALSE;
    }
    std::memcpy(point, call->data.data(), sizeof(POINT));
    return static_cast<BOOL>(call->result);
  }
  int CallCursorCallback(std::uintptr_t callback) override {
    const CallRecord* call =
        Next(CallRecord::Kind::Callback, reinterpret_cast<const void*>(callback));
    return call ? static_cast<int>(call->result) : 0;
  }
  // The gun readers are original code; replaying them is not supported.
  int ReadGunJoystick(input::ButtonRecord*, std::uint32_t, std::uint32_t) override {
    unsupported_ = true;
    return 0;
  }
  int ReadSegaGun(input::ButtonRecord*, std::uint32_t, std::uint32_t) override {
    unsupported_ = true;
    return 0;
  }

 private:
  const CallRecord* Next(CallRecord::Kind kind, const void* target) {
    if (!divergence_.empty()) {
      return nullptr;
    }
    if (next_ >= g_calls.size()) {
      divergence_ = std::format("extra call {} (original made {} calls)", KindName(kind), g_calls.size());
      return nullptr;
    }
    const CallRecord& call = g_calls[next_];
    if (call.kind != kind || call.target != target) {
      divergence_ = std::format("call {}: ours {} 0x{:X}, original {} 0x{:X}", next_, KindName(kind),
                                reinterpret_cast<std::uintptr_t>(target), KindName(call.kind),
                                reinterpret_cast<std::uintptr_t>(call.target));
      return nullptr;
    }
    ++next_;
    return &call;
  }
  bool Expect(bool condition, const char* what) {
    if (!condition && divergence_.empty()) {
      divergence_ = std::format("call {}: {} differ", next_ - 1, what);
    }
    return condition;
  }

  std::size_t next_ = 0;
  std::string divergence_;
  bool unsupported_ = false;
};

ReplayHost g_replay_host;

struct Range {
  std::uintptr_t begin;
  std::uintptr_t end;
};
constexpr Range kStateRanges[] = {
    {hod2::kInputStateBegin, hod2::kInputStateEnd},
    {hod2::kInputStateExtraBegin, hod2::kInputStateExtraEnd},
};

struct Snapshot {
  std::vector<std::uint8_t> bytes;

  void Take() {
    bytes.clear();
    for (const Range& range : kStateRanges) {
      const auto* p = reinterpret_cast<const std::uint8_t*>(range.begin);
      bytes.insert(bytes.end(), p, p + (range.end - range.begin));
    }
  }
  void Restore() const {
    std::size_t offset = 0;
    for (const Range& range : kStateRanges) {
      const std::size_t size = range.end - range.begin;
      std::memcpy(reinterpret_cast<void*>(range.begin), bytes.data() + offset, size);
      offset += size;
    }
  }
};

Snapshot g_before;
Snapshot g_original;
Snapshot g_ours;

struct CompareStats {
  std::uint64_t frames = 0;
  std::uint64_t equal = 0;
  std::uint64_t memory_mismatch = 0;
  std::uint64_t call_divergence = 0;
  std::uint64_t unsupported = 0;
  std::uint64_t calls = 0;
  std::uint64_t cursor_reads = 0;
  std::uint64_t active = 0;  // frames where the original saw a button held
  // Every button bit the original saw held, to tell which inputs the run covered.
  std::uint32_t seen_mouse = 0;
  std::uint32_t seen_layout0 = 0;
  std::uint32_t seen_layout1 = 0;

  std::uint64_t detailed_logs = 0;
};
CompareStats g_stats;

constexpr std::uint64_t kSummaryEveryFrames = 600;  // 10 s at 60 fps
constexpr std::uint64_t kMaxDetailedLogs = 20;

std::uintptr_t AddressOf(std::size_t offset) {
  for (const Range& range : kStateRanges) {
    const std::size_t size = range.end - range.begin;
    if (offset < size) {
      return range.begin + offset;
    }
    offset -= size;
  }
  return 0;
}

// Logs up to 8 differing byte runs between the original's and our result.
void LogMemoryDiff() {
  std::string text;
  int runs = 0;
  const auto& a = g_original.bytes;
  const auto& b = g_ours.bytes;
  for (std::size_t i = 0; i < a.size() && runs < 8;) {
    if (a[i] == b[i]) {
      ++i;
      continue;
    }
    std::size_t j = i;
    while (j < a.size() && a[j] != b[j] && j - i < 16) {
      ++j;
    }
    std::string orig_hex;
    std::string ours_hex;
    for (std::size_t k = i; k < j; ++k) {
      orig_hex += std::format("{:02X}", a[k]);
      ours_hex += std::format("{:02X}", b[k]);
    }
    text += std::format(" [0x{:X}: original {} ours {}]", AddressOf(i), orig_hex, ours_hex);
    ++runs;
    i = j;
  }
  Log(LogLevel::Warn, "input_compare: frame {} memory differs:{}", g_stats.frames, text);
}

void __cdecl CompareUpdateInput() {
  // Devices are created at startup, before the first update.
  HookDevice(*g_memory.keyboard);
  HookDevice(*g_memory.mouse);
  for (std::uint32_t i = 0; i < *g_memory.joystick_count && i < input::kMaxJoysticks; ++i) {
    HookDevice(g_memory.joysticks[i]);
  }

  g_before.Take();

  // 1. The original, recording its calls. The one-shot cursor callback is
  //    wrapped so that its result is recorded too.
  g_calls.clear();
  g_real_callback = *g_memory.cursor_callback;
  if (g_real_callback != 0) {
    *g_memory.cursor_callback = reinterpret_cast<std::uintptr_t>(&RecordCursorCallback);
  }
  g_recording = true;
  reinterpret_cast<UpdateInputFn>(hod2::kUpdateInput)();
  g_recording = false;
  if (*g_memory.cursor_callback == reinterpret_cast<std::uintptr_t>(&RecordCursorCallback)) {
    *g_memory.cursor_callback = g_real_callback;  // not consumed this frame
  }
  g_original.Take();
  g_stats.seen_mouse |= g_memory.mouse_record->held;
  g_stats.seen_layout0 |= g_memory.keyboard_records[0].held;
  g_stats.seen_layout1 |= g_memory.keyboard_records[1].held;
  const bool active = (g_memory.player_records[input::kPlayer1Slot].held |
                       g_memory.player_records[input::kPlayer2Slot].held |
                       g_memory.mouse_record->held) != 0;

  // 2. Ours, from the same starting state, with the recorded calls.
  g_before.Restore();
  g_replay_host.Reset();
  input::UpdateInput(g_memory, g_replay_host);
  g_ours.Take();

  // 3. The game continues with the original's result.
  g_original.Restore();

  ++g_stats.frames;
  g_stats.active += active ? 1 : 0;
  g_stats.calls += g_calls.size();
  for (const CallRecord& call : g_calls) {
    if (call.kind == CallRecord::Kind::GetCursorPos) {
      ++g_stats.cursor_reads;
    }
  }
  const bool detailed = g_stats.detailed_logs < kMaxDetailedLogs;
  if (g_replay_host.unsupported()) {
    ++g_stats.unsupported;
  } else if (!g_replay_host.divergence().empty() || !g_replay_host.AllConsumed()) {
    ++g_stats.call_divergence;
    if (detailed) {
      ++g_stats.detailed_logs;
      Log(LogLevel::Warn, "input_compare: frame {} calls differ: {}", g_stats.frames,
          g_replay_host.divergence().empty()
              ? std::format("ours made {} of {} calls", g_replay_host.Consumed(), g_calls.size())
              : g_replay_host.divergence());
    }
  } else if (g_original.bytes != g_ours.bytes) {
    ++g_stats.memory_mismatch;
    if (detailed) {
      ++g_stats.detailed_logs;
      LogMemoryDiff();
    }
  } else {
    ++g_stats.equal;
  }

  if (g_stats.frames % kSummaryEveryFrames == 0) {
    Log(LogLevel::Info,
        "input_compare: frames {} equal {} memory_mismatch {} call_divergence {} unsupported {} "
        "(frames with buttons held {}, buttons seen mouse 0x{:X} keys 0x{:X}/0x{:X}, calls {}, "
        "cursor reads {}, modes P1 0x{:X} P2 0x{:X})",
        g_stats.frames, g_stats.equal, g_stats.memory_mismatch, g_stats.call_divergence,
        g_stats.unsupported, g_stats.active, g_stats.seen_mouse, g_stats.seen_layout0,
        g_stats.seen_layout1, g_stats.calls, g_stats.cursor_reads, *g_memory.player1_mode,
        *g_memory.player2_mode);
  }
}

std::array<std::uint8_t, 5> CallBytes(std::uintptr_t from, std::uintptr_t to) {
  const auto rel = static_cast<std::uint32_t>(to - (from + 5));
  return {0xE8, static_cast<std::uint8_t>(rel), static_cast<std::uint8_t>(rel >> 8),
          static_cast<std::uint8_t>(rel >> 16), static_cast<std::uint8_t>(rel >> 24)};
}

}  // namespace

bool InstallInputUpdate(InputUpdateMode mode) {
  g_memory = GameInputMemory();
  const bool compare = mode == InputUpdateMode::Compare;
  const auto ours = reinterpret_cast<std::uintptr_t>(compare ? &CompareUpdateInput
                                                             : &ReimplementedUpdateInput);
  const auto expected = CallBytes(hod2::kUpdateInputCall, hod2::kUpdateInput);
  const auto replacement = CallBytes(hod2::kUpdateInputCall, ours);
  if (!interop::PatchBytes(hod2::kUpdateInputCall, expected, replacement)) {
    Log(LogLevel::Warn, "input_update: call at 0x{:X} does not match, not installed",
        hod2::kUpdateInputCall);
    return false;
  }
  if (compare) {
    HMODULE exe = GetModuleHandleW(nullptr);
    g_get_cursor_pos = reinterpret_cast<GetCursorPosFn>(
        interop::HookImport(exe, "USER32.dll", "GetCursorPos", reinterpret_cast<void*>(&RecordGetCursorPos)));
    g_client_to_screen = reinterpret_cast<ClientToScreenFn>(interop::HookImport(
        exe, "USER32.dll", "ClientToScreen", reinterpret_cast<void*>(&RecordClientToScreen)));
    if (!g_get_cursor_pos || !g_client_to_screen) {
      Log(LogLevel::Warn, "input_compare: cursor imports not found, cursor calls not recorded");
    }
    g_calls.reserve(64);
  }
  Log(LogLevel::Info, "input_update: {} (call at 0x{:X})",
      compare ? "comparing reimplementation with the original" : "reimplementation installed",
      hod2::kUpdateInputCall);
  return true;
}

}  // namespace thod2::core::patches
