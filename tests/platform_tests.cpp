// Tests for the reimplemented input update (thod2/platform/input.h) on test
// buffers with a scripted host. They pin down the behaviour read from the
// original code; the in-game comparison is [debug] input_compare.

#include <thod2/platform/input.h>

#include <cstdio>
#include <cstring>
#include <deque>

namespace {

namespace input = thod2::platform::input;

int g_failures = 0;

void Check(bool condition, const char* what) {
  std::printf("[%s] %s\n", condition ? "PASS" : "FAIL", what);
  if (!condition) {
    ++g_failures;
  }
}

// Fake input globals with the same shapes as the game's.
struct FakeGame {
  std::uint32_t player1_mode = 0;
  std::uint32_t player2_mode = 0;
  std::int32_t player1_index = 0;
  std::int32_t player2_index = 0;
  std::uint32_t joystick_count = 0;
  IDirectInputDevice7A* joysticks[8] = {};
  IDirectInputDevice7A* keyboard = nullptr;
  IDirectInputDevice7A* mouse = nullptr;
  input::ButtonRecord joystick_records[8] = {};
  input::ButtonRecord mouse_record = {};
  input::ButtonRecord keyboard_records[2] = {};
  input::ButtonRecord player_records[24] = {};
  std::uint8_t slot_states[24][0x78] = {};
  std::int32_t cursor[24][2] = {};
  std::uint32_t trigger_flags[24] = {};
  std::uint32_t reload_flags[24] = {};
  std::uint8_t key_state[256] = {};
  DIDEVICEOBJECTDATA key_events[16] = {};
  std::uint32_t key_chars[256] = {};
  std::uint8_t name_entry[8] = {};
  std::uint32_t keyboard_previous = 0;
  std::uint32_t mouse_previous = 0;
  std::uint32_t joystick_previous[8] = {};
  std::uint32_t joystick_button_map[14] = {};
  std::uintptr_t cursor_callback = 0;
  HWND window = nullptr;

  input::InputMemory View() {
    input::InputMemory m{};
    m.player1_mode = &player1_mode;
    m.player2_mode = &player2_mode;
    m.player1_index = &player1_index;
    m.player2_index = &player2_index;
    m.joystick_count = &joystick_count;
    m.joysticks = joysticks;
    m.keyboard = &keyboard;
    m.mouse = &mouse;
    m.joystick_records = joystick_records;
    m.mouse_record = &mouse_record;
    m.keyboard_records = keyboard_records;
    m.player_records = player_records;
    m.slot_states = &slot_states[0][0];
    m.cursor = cursor;
    m.trigger_flags = trigger_flags;
    m.reload_flags = reload_flags;
    m.key_state = key_state;
    m.key_events = key_events;
    m.key_chars = key_chars;
    m.name_entry = name_entry;
    m.keyboard_previous = &keyboard_previous;
    m.mouse_previous = &mouse_previous;
    m.joystick_previous = joystick_previous;
    m.joystick_button_map = joystick_button_map;
    m.cursor_callback = &cursor_callback;
    m.window = &window;
    return m;
  }
};

// Device handles are only compared, never called.
IDirectInputDevice7A* const kKeyboard = reinterpret_cast<IDirectInputDevice7A*>(0x1000);
IDirectInputDevice7A* const kMouse = reinterpret_cast<IDirectInputDevice7A*>(0x2000);
IDirectInputDevice7A* const kJoystick = reinterpret_cast<IDirectInputDevice7A*>(0x3000);

// Answers device reads from fixed states.
class FakeHost : public input::InputHost {
 public:
  std::uint8_t keys[256] = {};
  std::deque<DIDEVICEOBJECTDATA> key_events;
  DIMOUSESTATE mouse{};
  DIJOYSTATE2 joystick{};
  POINT cursor{};
  POINT client_origin{};
  bool keep_events = false;  // true: every GetDeviceData call returns the same events
  int callback_result = 1;
  int callback_calls = 0;

  HRESULT Acquire(IDirectInputDevice7A*) override { return S_OK; }
  HRESULT Poll(IDirectInputDevice7A*) override { return S_OK; }
  HRESULT GetDeviceState(IDirectInputDevice7A* device, DWORD size, void* data) override {
    if (device == kKeyboard && size == 256) {
      std::memcpy(data, keys, 256);
    } else if (device == kMouse && size == sizeof(mouse)) {
      std::memcpy(data, &mouse, size);
    } else if (device == kJoystick && size == sizeof(joystick)) {
      std::memcpy(data, &joystick, size);
    } else {
      return E_FAIL;
    }
    return S_OK;
  }
  HRESULT GetDeviceData(IDirectInputDevice7A*, DWORD, DIDEVICEOBJECTDATA* data, DWORD* count,
                        DWORD) override {
    DWORD n = 0;
    for (const DIDEVICEOBJECTDATA& event : key_events) {
      if (n == *count) {
        break;
      }
      data[n++] = event;
    }
    if (!keep_events) {
      key_events.erase(key_events.begin(), key_events.begin() + n);
    }
    *count = n;
    return S_OK;
  }
  BOOL GetCursorPos(POINT* point) override {
    *point = cursor;
    return TRUE;
  }
  BOOL ClientToScreen(HWND, POINT* point) override {
    point->x += client_origin.x;
    point->y += client_origin.y;
    return TRUE;
  }
  int CallCursorCallback(std::uintptr_t) override {
    ++callback_calls;
    return callback_result;
  }
  int ReadGunJoystick(input::ButtonRecord*, std::uint32_t, std::uint32_t) override { return 0; }
  int ReadSegaGun(input::ButtonRecord*, std::uint32_t, std::uint32_t) override { return 0; }
};

void TestKeyboardLayouts() {
  FakeGame game;
  FakeHost host;
  game.keyboard = kKeyboard;
  input::InputMemory m = game.View();

  host.keys[DIK_LEFT] = 0x80;
  host.keys[DIK_SPACE] = 0x80;
  host.keys[DIK_W] = 0x80;
  host.keys[DIK_TAB] = 0x80;
  input::ReadKeyboard(m, host, &game.keyboard_records[0], 0);
  Check(game.keyboard_records[0].held == (input::kButtonLeft | input::kButtonTrigger),
        "layout 0: LEFT + SPACE -> left + trigger");
  Check(game.keyboard_records[0].pressed == game.keyboard_records[0].held,
        "layout 0: first frame, everything held is pressed");

  input::ReadKeyboard(m, host, &game.keyboard_records[1], 1);
  Check(game.keyboard_records[1].held == (input::kButtonUp | input::kButtonTrigger),
        "layout 1: W + TAB -> up + trigger");
  // Shared previous state: layout 1 compares with layout 0's result.
  Check(game.keyboard_records[1].pressed == input::kButtonUp,
        "layout 1: edges are relative to layout 0 (shared previous state)");
  Check(game.keyboard_records[1].released == input::kButtonLeft,
        "layout 1: layout 0's left shows up as released");

  host.keys[DIK_LMENU] = 0x80;
  input::ReadKeyboard(m, host, &game.keyboard_records[1], 1);
  Check((game.keyboard_records[1].held & input::kButtonTrigger) == 0,
        "layout 1: TAB is ignored while Alt is held");
  Check(game.keyboard_records[1].not_held == ~game.keyboard_records[1].held, "not_held is ~held");
}

void TestMouseButtons() {
  FakeGame game;
  FakeHost host;
  game.mouse = kMouse;
  input::InputMemory m = game.View();

  host.mouse.rgbButtons[0] = 0x80;
  host.mouse.rgbButtons[1] = 0x80;
  host.mouse.rgbButtons[2] = 0x80;
  input::ReadMouse(m, host, &game.mouse_record);
  Check(game.mouse_record.held == (input::kButtonTrigger | input::kButtonReload | input::kButtonStart),
        "mouse: left/right/middle -> trigger/reload/start");
  input::ReadMouse(m, host, &game.mouse_record);
  Check(game.mouse_record.pressed == 0, "mouse: held buttons are not pressed again");
}

void TestMouseAndKeyboardMode() {
  FakeGame game;
  FakeHost host;
  game.keyboard = kKeyboard;
  game.mouse = kMouse;
  game.player1_mode = 6;
  game.player2_mode = 3;
  input::InputMemory m = game.View();

  host.mouse.rgbButtons[0] = 0x80;  // trigger
  host.keys[DIK_RETURN] = 0x80;     // start (layout 0)
  host.keys[DIK_D] = 0x80;          // right (layout 1)
  host.cursor = {420, 300};
  host.client_origin = {100, 50};
  input::UpdateInput(m, host);

  const input::ButtonRecord& p1 = game.player_records[input::kPlayer1Slot];
  Check(p1.type == input::kRecordPointer, "mode 6: player 1 is a pointer record");
  Check(p1.held == (input::kButtonTrigger | input::kButtonStart), "mode 6: mouse OR keyboard");
  Check(game.trigger_flags[0] == 1 && game.reload_flags[0] == 0, "mode 6: trigger flag set");
  Check(game.cursor[0][0] == 320 && game.cursor[0][1] == 250, "mode 6: cursor in client pixels");
  Check(p1.slot_state == &game.slot_states[0][0] && game.slot_states[0][0x11] == 2,
        "mode 6: slot state linked");

  const input::ButtonRecord& p2 = game.player_records[input::kPlayer2Slot];
  Check(p2.type == input::kRecordDigital && p2.digital_id == input::kDigitalId,
        "mode 3: player 2 is a digital record");
  Check(p2.held == input::kButtonRight, "mode 3: player 2 uses layout 1");

  // Flags follow the pressed edges: trigger still held, reload newly pressed.
  host.mouse.rgbButtons[1] = 0x80;
  input::UpdateInput(m, host);
  Check(game.trigger_flags[0] == 0 && game.reload_flags[0] == 1,
        "mode 6: flags come from pressed edges, not held buttons");
  // Trigger and reload pressed in the same frame: only reload counts.
  game.mouse_previous = 0;
  input::UpdateInput(m, host);
  Check(game.trigger_flags[0] == 0 && game.reload_flags[0] == 1,
        "mode 6: trigger and reload pressed together -> reload only");

  // A pending cursor callback that returns 0 keeps the old aim position.
  host.cursor = {0, 0};
  game.cursor_callback = 0x1234;
  host.callback_result = 0;
  input::UpdateInput(m, host);
  Check(host.callback_calls == 1 && game.cursor_callback == 0, "cursor callback runs once");
  Check(game.cursor[0][0] == 320, "cursor callback 0 -> position not updated");
}

void TestNameEntry() {
  FakeGame game;
  FakeHost host;
  game.keyboard = kKeyboard;
  game.player1_mode = 2;
  input::InputMemory m = game.View();
  game.key_chars[DIK_A] = 'A';
  game.key_chars[DIK_B] = 'B';

  // Each ReadKeyboard call clears the event buffer and reads again, so only the
  // second call (layout 1) leaves events for text entry (original behaviour).
  host.key_events.push_back({DIK_A, 0x80, 0, 0});
  input::UpdateInput(m, host);
  Check(game.name_entry[2] == 0, "mode 2: events read by layout 0 are lost");

  host.keep_events = true;
  host.keys[DIK_LSHIFT] = 0x80;
  host.key_events.push_back({DIK_A, 0x80, 0, 0});
  host.key_events.push_back({DIK_A, 0x00, 0, 0});  // key up: ignored
  host.key_events.push_back({DIK_F1, 0x80, 0, 0});  // no character: ignored
  host.key_events.push_back({DIK_B, 0x80, 0, 0});
  input::UpdateInput(m, host);
  Check(game.player_records[0].type == input::kRecordNameEntry, "mode 2: name entry record");
  Check(game.name_entry[0] == 0x02, "mode 2: left shift modifier bit");
  Check(game.name_entry[2] == 'A' && game.name_entry[3] == 'B' && game.name_entry[4] == 0,
        "mode 2: typed characters");

  host.key_events.clear();
  for (int i = 0; i < 8; ++i) {
    host.key_events.push_back({DIK_A, 0x80, 0, 0});
  }
  input::UpdateInput(m, host);
  Check(game.name_entry[2] == 1 && game.name_entry[3] == 'A', "mode 2: 8th key sets overflow marker");
}

void TestJoystick() {
  FakeGame game;
  FakeHost host;
  game.joystick_count = 1;
  game.joysticks[0] = kJoystick;
  game.joystick_button_map[0] = input::kButtonTrigger;
  game.joystick_button_map[3] = input::kButtonStart;
  game.player1_mode = 7;  // joystick 0
  input::InputMemory m = game.View();

  host.joystick.lX = 0x41;
  host.joystick.lY = -0x40;
  host.joystick.rgbButtons[3] = 0x80;
  input::UpdateInput(m, host);
  Check(game.joystick_records[0].held == (input::kButtonRight | input::kButtonUp | input::kButtonStart),
        "joystick: stick past +-0x40 and mapped button");
  Check(game.player_records[0].held == game.joystick_records[0].held, "mode 7: player 1 uses joystick 0");

  host.joystick.lX = 0x40;
  host.joystick.lY = -0x3F;
  input::UpdateInput(m, host);
  Check((game.joystick_records[0].held & 0xF0) == 0, "joystick: +0x40 / -0x3F stay in the dead zone");
}

}  // namespace

int main() {
  TestKeyboardLayouts();
  TestMouseButtons();
  TestMouseAndKeyboardMode();
  TestNameEntry();
  TestJoystick();
  std::printf("%d failure(s)\n", g_failures);
  return g_failures;
}
