// Reimplementation of the original per-frame input update (Hod2.exe 0x41E120)
// and the device readers it calls. See docs/work/platform-input.md.
//
// The code works on a view of the game's input globals (InputMemory) and does
// every DirectInput / Win32 call through InputHost, so that it can run on the
// game's memory, on test buffers, or against a recording of the original.
#pragma once

#define DIRECTINPUT_VERSION 0x0700
#include <windows.h>
#include <dinput.h>

#include <cstddef>
#include <cstdint>

namespace thod2::platform::input {

// Button bits used by every record. The meaning comes from the keyboard,
// mouse and joystick mappings (docs/work/platform-input.md "踰꾪듉 鍮꾪듃").
inline constexpr std::uint32_t kButtonReload = 0x02;
inline constexpr std::uint32_t kButtonTrigger = 0x04;
inline constexpr std::uint32_t kButtonStart = 0x08;
inline constexpr std::uint32_t kButtonUp = 0x10;
inline constexpr std::uint32_t kButtonDown = 0x20;
inline constexpr std::uint32_t kButtonLeft = 0x40;
inline constexpr std::uint32_t kButtonRight = 0x80;

// Value of ButtonRecord::type.
inline constexpr std::uint32_t kRecordDigital = 0x01;    // keyboard, joystick
inline constexpr std::uint32_t kRecordNameEntry = 0x40;  // keyboard text entry
inline constexpr std::uint32_t kRecordPointer = 0x80;    // mouse, light gun

// Written to ButtonRecord::digital_id for digital devices. Meaning unknown.
inline constexpr std::uint32_t kDigitalId = 0xF06FE;

// One 0x34-byte input record. The game keeps one per device (raw records) and
// one per player slot (player records, which also point to a slot state).
struct ButtonRecord {
  std::uint32_t type;        // +0x00 kRecord*
  std::uint32_t digital_id;  // +0x04 kDigitalId for digital devices
  std::uint32_t held;        // +0x08 buttons down this frame
  std::uint32_t not_held;    // +0x0C ~held (OR of both sources in combined modes)
  std::uint32_t pressed;     // +0x10 went down this frame
  std::uint32_t released;    // +0x14 went up this frame
  std::uint32_t unknown_18[6];
  void* slot_state;  // +0x30 player records only: &slot_states[slot]
};
static_assert(sizeof(ButtonRecord) == 0x34);

inline constexpr std::size_t kSlotStateSize = 0x78;
inline constexpr std::size_t kSlotCount = 24;
inline constexpr std::size_t kMaxJoysticks = 8;
inline constexpr std::size_t kKeyEventCount = 16;
inline constexpr std::size_t kNameEntryMaxChars = 7;

// Player slots used by the update: player 1 is slot 0, player 2 is slot 6.
inline constexpr std::uint32_t kPlayer1Slot = 0;
inline constexpr std::uint32_t kPlayer2Slot = 6;

// Pointers to the game's input globals (addresses in the core patch).
struct InputMemory {
  std::uint32_t* player1_mode;  // device mode, high bit = "joystick is a gun"
  std::uint32_t* player2_mode;
  std::int32_t* player1_index;  // Hod2.ini PLAYER1_INDEX
  std::int32_t* player2_index;  // Hod2.ini PLAYER2_INDEX
  std::uint32_t* joystick_count;
  IDirectInputDevice7A** joysticks;  // [kMaxJoysticks]
  IDirectInputDevice7A** keyboard;
  IDirectInputDevice7A** mouse;
  ButtonRecord* joystick_records;  // [kMaxJoysticks]
  ButtonRecord* mouse_record;
  ButtonRecord* keyboard_records;  // [2], one key layout per player
  ButtonRecord* player_records;    // [kSlotCount]
  std::uint8_t* slot_states;       // [kSlotCount][kSlotStateSize]
  std::int32_t (*cursor)[2];       // [kSlotCount] aim position in client pixels
  std::uint32_t* trigger_flags;    // [kSlotCount]
  std::uint32_t* reload_flags;     // [kSlotCount]
  std::uint8_t* key_state;         // [256] DIK state
  DIDEVICEOBJECTDATA* key_events;  // [kKeyEventCount] buffered keyboard data
  const std::uint32_t* key_chars;  // [256] DIK -> character for text entry
  std::uint8_t* name_entry;        // [8] modifiers, unused, up to 7 characters
  std::uint32_t* keyboard_previous;  // shared by both keyboard layouts (original quirk)
  std::uint32_t* mouse_previous;
  std::uint32_t* joystick_previous;          // [kMaxJoysticks]
  const std::uint32_t* joystick_button_map;  // [14] button index -> kButton*
  std::uintptr_t* cursor_callback;  // one-shot callback, see ConsumeCursorCallback
  HWND* window;
};

// Everything the input code asks of the outside world. The default
// implementations call DirectInput and Win32 directly.
class InputHost {
 public:
  virtual ~InputHost() = default;
  virtual HRESULT Acquire(IDirectInputDevice7A* device);
  virtual HRESULT Poll(IDirectInputDevice7A* device);
  virtual HRESULT GetDeviceState(IDirectInputDevice7A* device, DWORD size, void* data);
  virtual HRESULT GetDeviceData(IDirectInputDevice7A* device, DWORD object_size,
                                DIDEVICEOBJECTDATA* data, DWORD* count, DWORD flags);
  virtual BOOL GetCursorPos(POINT* point);
  virtual BOOL ClientToScreen(HWND window, POINT* point);
  // Adjusts the cursor after GetCursorClientPos (e.g. window pixels -> game
  // screen). The default leaves it as the original does.
  virtual void MapCursor(std::int32_t* /*xy*/) {}
  // Calls the game's one-shot cursor callback (int __cdecl fn(void)).
  virtual int CallCursorCallback(std::uintptr_t callback) = 0;
  // Original readers that are not reimplemented yet (no hardware to verify).
  virtual int ReadGunJoystick(ButtonRecord* record, std::uint32_t device, std::uint32_t slot) = 0;
  virtual int ReadSegaGun(ButtonRecord* record, std::uint32_t gun, std::uint32_t slot) = 0;
};

// Original: 0x41E120 UpdateInput
void UpdateInput(const InputMemory& memory, InputHost& host);

// Original: 0x41ED80 ReadKeyboard
void ReadKeyboard(const InputMemory& memory, InputHost& host, ButtonRecord* record,
                  std::uint32_t layout);
// Original: 0x41EF50 ReadMouse
void ReadMouse(const InputMemory& memory, InputHost& host, ButtonRecord* record);
// Original: 0x41F430 ReadJoystick
void ReadJoystick(const InputMemory& memory, InputHost& host, ButtonRecord* record,
                  std::uint32_t device);
// Original: 0x41E670 GetCursorClientPos
void GetCursorClientPos(const InputMemory& memory, InputHost& host, std::int32_t* xy);
// Original: 0x41E7C0 ConsumeCursorCallback
int ConsumeCursorCallback(const InputMemory& memory, InputHost& host);

}  // namespace thod2::platform::input
