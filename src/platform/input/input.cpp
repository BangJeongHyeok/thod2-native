#include <thod2/platform/input.h>

#include <cstring>

namespace thod2::platform::input {
namespace {

// DirectInput keeps returning this until the device is acquired again.
constexpr HRESULT kInputLost = static_cast<HRESULT>(0x8007001E);  // DIERR_INPUTLOST

// Analog stick dead zone of the joystick reader (axis range set at enumeration).
constexpr LONG kStickThreshold = 0x40;

// DIK codes of the two keyboard layouts.
struct KeyBinding {
  std::uint8_t dik;
  std::uint32_t button;
};

constexpr KeyBinding kLayout0[] = {
    {DIK_RIGHT, kButtonRight},   {DIK_LEFT, kButtonLeft},       {DIK_UP, kButtonUp},
    {DIK_DOWN, kButtonDown},     {DIK_RETURN, kButtonStart},    {DIK_SPACE, kButtonTrigger},
    {DIK_ADD, kButtonReload},    {DIK_NUMPAD6, kButtonRight},   {DIK_NUMPAD4, kButtonLeft},
    {DIK_NUMPAD8, kButtonUp},    {DIK_NUMPAD2, kButtonDown},
};

constexpr KeyBinding kLayout1[] = {
    {DIK_D, kButtonRight},      {DIK_A, kButtonLeft},         {DIK_W, kButtonUp},
    {DIK_X, kButtonDown},       {DIK_LSHIFT, kButtonStart}, {DIK_LCONTROL, kButtonReload},
    // DIK_TAB (trigger) is handled in ReadKeyboard: ignored while Alt is held (Alt+Tab).
};

// Modifier bits written to name_entry[0] in text entry mode.
constexpr KeyBinding kNameEntryModifiers[] = {
    {DIK_LSHIFT, 0x02}, {DIK_RSHIFT, 0x20},   {DIK_LMENU, 0x40},
    {DIK_RMENU, 0x04},  {DIK_LCONTROL, 0x01}, {DIK_RCONTROL, 0x10},
};

bool KeyDown(const InputMemory& memory, std::uint8_t dik) {
  return (memory.key_state[dik] & 0x80) != 0;
}

// Fills not_held, pressed and released from held and the previous frame's held.
void UpdateEdges(ButtonRecord* record, std::uint32_t* previous) {
  const std::uint32_t held = record->held;
  record->not_held = ~held;
  record->pressed = (held ^ *previous) & held;
  record->released = (held ^ *previous) & ~held;
  *previous = held;
}

std::uint8_t* SlotState(const InputMemory& memory, std::uint32_t slot) {
  return memory.slot_states + slot * kSlotStateSize;
}

// Sets the record type in the player record and in its slot state, and links them.
void SetPlayerType(const InputMemory& memory, std::uint32_t slot, std::uint32_t type) {
  std::uint8_t* state = SlotState(memory, slot);
  *reinterpret_cast<std::uint32_t*>(state) = type;
  state[0x11] = 2;
  memory.player_records[slot].type = type;
  memory.player_records[slot].slot_state = state;
}

void CopyButtons(ButtonRecord* to, const ButtonRecord& from) {
  to->held = from.held;
  to->not_held = from.not_held;
  to->pressed = from.pressed;
  to->released = from.released;
}

void OrButtons(ButtonRecord* to, const ButtonRecord& a, const ButtonRecord& b) {
  to->held = a.held | b.held;
  to->not_held = a.not_held | b.not_held;
  to->pressed = a.pressed | b.pressed;
  to->released = a.released | b.released;
}

void ReadNameEntry(const InputMemory& memory) {
  std::memset(memory.name_entry, 0, 8);
  for (const KeyBinding& key : kNameEntryModifiers) {
    if (KeyDown(memory, key.dik)) {
      memory.name_entry[0] |= static_cast<std::uint8_t>(key.button);
    }
  }
  std::size_t count = 0;
  for (std::size_t i = 0; i < kKeyEventCount; ++i) {
    const DIDEVICEOBJECTDATA& event = memory.key_events[i];
    if ((event.dwData & 0x80) == 0) {
      continue;
    }
    if (count >= kNameEntryMaxChars) {
      memory.name_entry[2] = 1;  // overflow marker, overwrites the first character
      return;
    }
    const std::uint32_t c = memory.key_chars[event.dwOfs & 0xFF];
    if (c != 0) {
      memory.name_entry[2 + count] = static_cast<std::uint8_t>(c);
      ++count;
    }
  }
}

// Mouse modes: trigger/reload flags of the slot from the mouse's pressed edges,
// and the aim position unless the cursor callback says otherwise.
void ApplyMouseShot(const InputMemory& memory, InputHost& host, std::uint32_t slot,
                    std::uint32_t mouse_pressed) {
  SetPlayerType(memory, slot, kRecordPointer);
  std::uint32_t trigger = (mouse_pressed >> 2) & 1;
  std::uint32_t reload = (mouse_pressed >> 1) & 1;
  if (trigger != 0 && reload != 0) {
    trigger = 0;
  }
  memory.trigger_flags[slot] = trigger;
  memory.reload_flags[slot] = reload;
  if (ConsumeCursorCallback(memory, host) == 1) {
    GetCursorClientPos(memory, host, memory.cursor[slot]);
  }
}

void ApplyPlayerMode(const InputMemory& memory, InputHost& host, std::uint32_t player,
                     std::uint32_t mode, std::uint32_t slot) {
  ButtonRecord* out = &memory.player_records[slot];
  const ButtonRecord& keys = memory.keyboard_records[player];
  const ButtonRecord& mouse = *memory.mouse_record;
  switch (mode & 0xFFFF) {
    case 0:
      out->slot_state = SlotState(memory, slot);
      break;
    case 2:
      SetPlayerType(memory, slot, kRecordNameEntry);
      if (player == 0) {
        ReadNameEntry(memory);
      }
      break;
    case 3:
      SetPlayerType(memory, slot, kRecordDigital);
      out->digital_id = kDigitalId;
      CopyButtons(out, keys);
      break;
    case 6:
      OrButtons(out, keys, mouse);
      ApplyMouseShot(memory, host, slot, mouse.pressed);
      break;
    case 5:
      ApplyMouseShot(memory, host, slot, mouse.pressed);
      break;
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
      if (*memory.joystick_count != 0) {
        if ((mode & 0x80000000) != 0) {
          SetPlayerType(memory, slot, kRecordPointer);
        } else {
          SetPlayerType(memory, slot, kRecordDigital);
          out->digital_id = kDigitalId;
        }
        OrButtons(out, memory.joystick_records[(mode & 0xFFFF) - 7], keys);
      }
      break;
    case 13:
    case 14: {
      SetPlayerType(memory, slot, kRecordPointer);
      host.ReadSegaGun(out, mode - 13, slot);
      const std::uint32_t held = out->held | keys.held;
      out->held = held;
      out->not_held = ~held;
      break;
    }
    default:  // 1, 4 and anything above 14: nothing
      break;
  }
}

}  // namespace

HRESULT InputHost::Acquire(IDirectInputDevice7A* device) {
  return device->Acquire();
}

HRESULT InputHost::Poll(IDirectInputDevice7A* device) {
  return device->Poll();
}

HRESULT InputHost::GetDeviceState(IDirectInputDevice7A* device, DWORD size, void* data) {
  return device->GetDeviceState(size, data);
}

HRESULT InputHost::GetDeviceData(IDirectInputDevice7A* device, DWORD object_size,
                                 DIDEVICEOBJECTDATA* data, DWORD* count, DWORD flags) {
  return device->GetDeviceData(object_size, data, count, flags);
}

BOOL InputHost::GetCursorPos(POINT* point) {
  return ::GetCursorPos(point);
}

BOOL InputHost::ClientToScreen(HWND window, POINT* point) {
  return ::ClientToScreen(window, point);
}

// Reimplements: 0x41ED80 ReadKeyboard
// Status: verified
// Notes: both layouts share keyboard_previous, so the pressed/released edges of
// one layout are relative to the other layout's last result (original quirk).
void ReadKeyboard(const InputMemory& memory, InputHost& host, ButtonRecord* record,
                  std::uint32_t layout) {
  IDirectInputDevice7A* keyboard = *memory.keyboard;
  std::memset(memory.key_state, 0, 256);
  std::memset(memory.key_events, 0, kKeyEventCount * sizeof(DIDEVICEOBJECTDATA));
  HRESULT hr;
  do {
    DWORD count = kKeyEventCount;
    host.GetDeviceData(keyboard, sizeof(DIDEVICEOBJECTDATA), memory.key_events, &count, 0);
    hr = host.GetDeviceState(keyboard, 256, memory.key_state);
    if (FAILED(hr)) {
      std::memset(memory.key_state, 0, 256);
      hr = host.Acquire(keyboard);
      if (FAILED(hr)) {
        return;
      }
    }
  } while (hr == kInputLost);

  record->held = 0;
  if (layout == 0) {
    for (const KeyBinding& key : kLayout0) {
      if (KeyDown(memory, key.dik)) {
        record->held |= key.button;
      }
    }
  } else if (layout == 1) {
    for (const KeyBinding& key : kLayout1) {
      if (KeyDown(memory, key.dik)) {
        record->held |= key.button;
      }
    }
    if (!KeyDown(memory, DIK_LMENU) && !KeyDown(memory, DIK_RMENU) && KeyDown(memory, DIK_TAB)) {
      record->held |= kButtonTrigger;
    }
  }
  UpdateEdges(record, memory.keyboard_previous);
}

// Reimplements: 0x41EF50 ReadMouse
// Status: verified
// Notes: left button = trigger, right = reload, middle = start. The aim
// position comes from the Windows cursor (GetCursorClientPos), not from here.
void ReadMouse(const InputMemory& memory, InputHost& host, ButtonRecord* record) {
  IDirectInputDevice7A* mouse = *memory.mouse;
  DIMOUSESTATE state{};
  HRESULT hr;
  do {
    hr = host.GetDeviceState(mouse, sizeof(state), &state);
    if (FAILED(hr)) {
      hr = host.Acquire(mouse);
      if (FAILED(hr)) {
        return;
      }
    }
  } while (hr == kInputLost);

  std::uint32_t held = 0;
  if (state.rgbButtons[0] & 0x80) {
    held |= kButtonTrigger;
  }
  if (state.rgbButtons[1] & 0x80) {
    held |= kButtonReload;
  }
  if (state.rgbButtons[2] & 0x80) {
    held |= kButtonStart;
  }
  record->held = held;
  UpdateEdges(record, memory.mouse_previous);
}

// Reimplements: 0x41F430 ReadJoystick
// Status: implemented
// Notes: not verified, no joystick on the test PC.
void ReadJoystick(const InputMemory& memory, InputHost& host, ButtonRecord* record,
                  std::uint32_t device) {
  IDirectInputDevice7A* joystick = memory.joysticks[device];
  DIJOYSTATE2 state;
  HRESULT hr;
  do {
    hr = host.Poll(joystick);
    if (hr == kInputLost && FAILED(host.Acquire(joystick))) {
      return;
    }
    std::memset(&state, 0, sizeof(state));
    hr = host.GetDeviceState(joystick, sizeof(state), &state);
    if (FAILED(hr)) {
      hr = host.Acquire(joystick);
      if (FAILED(hr)) {
        return;
      }
    }
  } while (hr == kInputLost);

  record->held = 0;
  if (state.lX > kStickThreshold) {
    record->held = kButtonRight;
  } else if (state.lX < -kStickThreshold + 1) {
    record->held = kButtonLeft;
  }
  if (state.lY > kStickThreshold) {
    record->held |= kButtonDown;
  } else if (state.lY < -kStickThreshold + 1) {
    record->held |= kButtonUp;
  }
  for (std::size_t i = 0; i < 14; ++i) {
    if (state.rgbButtons[i] & 0x80) {
      record->held |= memory.joystick_button_map[i];
    }
  }
  UpdateEdges(record, &memory.joystick_previous[device]);
}

// Reimplements: 0x41E670 GetCursorClientPos
// Status: verified
void GetCursorClientPos(const InputMemory& memory, InputHost& host, std::int32_t* xy) {
  POINT cursor{};
  POINT origin{};
  host.GetCursorPos(&cursor);
  host.ClientToScreen(*memory.window, &origin);
  xy[0] = cursor.x - origin.x;
  xy[1] = cursor.y - origin.y;
  host.MapCursor(xy);
}

// Reimplements: 0x41E7C0 ConsumeCursorCallback
// Status: verified
// Notes: returns 1 when no callback is set; the update reads the cursor only
// when this returns 1.
int ConsumeCursorCallback(const InputMemory& memory, InputHost& host) {
  int result = 1;
  if (*memory.cursor_callback != 0) {
    result = host.CallCursorCallback(*memory.cursor_callback);
    *memory.cursor_callback = 0;
  }
  return result;
}

// Reimplements: 0x41E120 UpdateInput
// Status: verified
// Notes: verified with [debug] input_compare for P1 mode 6 (mouse + keyboard)
// and P2 mode 3 (keyboard), including the cursor callback; modes 0, 2, 5, 7-14
// are not covered yet. The gun readers (0x41F000, 0x41F210) are still the
// original code, called through InputHost.
void UpdateInput(const InputMemory& memory, InputHost& host) {
  // The modes are read once; the joystick loop below re-reads them like the original.
  const std::uint32_t modes[2] = {*memory.player1_mode, *memory.player2_mode};
  const std::uint32_t slots[2] = {kPlayer1Slot, kPlayer2Slot};

  for (std::uint32_t i = 0; i < *memory.joystick_count; ++i) {
    if (memory.joysticks[i] == nullptr) {
      continue;
    }
    ButtonRecord* record = &memory.joystick_records[i];
    std::memset(record, 0, sizeof(*record));
    if ((*memory.player1_mode & 0x80000000) != 0 &&
        static_cast<std::uint32_t>(*memory.player1_index - 3) == i) {
      host.ReadGunJoystick(record, i, kPlayer1Slot);
    } else if ((*memory.player2_mode & 0x80000000) != 0 &&
               static_cast<std::uint32_t>(*memory.player2_index - 3) == i) {
      host.ReadGunJoystick(record, i, kPlayer2Slot);
    } else {
      ReadJoystick(memory, host, record, i);
    }
  }
  if (*memory.mouse != nullptr) {
    std::memset(memory.mouse_record, 0, sizeof(ButtonRecord));
    ReadMouse(memory, host, memory.mouse_record);
  }
  if (*memory.keyboard != nullptr) {
    std::memset(&memory.keyboard_records[0], 0, 2 * sizeof(ButtonRecord));
    ReadKeyboard(memory, host, &memory.keyboard_records[0], 0);
    ReadKeyboard(memory, host, &memory.keyboard_records[1], 1);
  }
  for (std::uint32_t player = 0; player < 2; ++player) {
    ApplyPlayerMode(memory, host, player, modes[player], slots[player]);
  }
}

}  // namespace thod2::platform::input
