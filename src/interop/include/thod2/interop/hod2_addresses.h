// Fixed addresses (VA) in the supported Hod2.exe build.
//
// Only valid when thod2::core::FindKnownExe() accepted the host executable.
// Every entry must also be listed in re/*.csv with the evidence it is based
// on; tools/re/check_re.py (ctest re_tables) fails otherwise.
#pragma once

#include <cstddef>
#include <cstdint>

namespace thod2::interop::hod2 {

// Return address of the MessageBoxA call that shows the startup device
// confirmation ("Attention", "... device will be used.", MB_YESNO | MB_ICONEXCLAMATION).
// Evidence: dinput_probe run log (docs/work/loader.md).
constexpr std::uintptr_t kDeviceDialogMessageBoxReturn = 0x4A4758;

// Frame wait (docs/work/platform-timing.md "CPU 100% 분석 결과").
// The frame end function 0x4A4B30 busy-waits for the target frame time and
// calls the function pointer in kFrameWaitCallbackSlot with the remaining ms on
// every spin. The pointer is stored by `mov dword [0x7DCDFC], 0x49E0C0` at
// 0x49DF0B; the 32-bit immediate of that instruction is at kFrameWaitCallbackImmediate.
// Evidence: byte decoding of 0x4A4D0F..0x4A4D63 and 0x49DF0B, profiler hotspot.
constexpr std::uintptr_t kFrameWaitCallbackImmediate = 0x49DF11;
constexpr std::uintptr_t kFrameWaitCallbackSlot = 0x7DCDFC;
// Original callback: void __cdecl(float remaining_ms). Processes one queued job
// (0x41D620) when more than 1 ms remains.
constexpr std::uintptr_t kFrameWaitCallback = 0x49E0C0;

// Job queue processed by the frame wait callback: 64 entries of 16 bytes at
// 0x7D8EB0, read index and write index below. Empty when they are equal.
constexpr std::uintptr_t kJobQueueReadIndex = 0x7D8AA8;
constexpr std::uintptr_t kJobQueueWriteIndex = 0x7D8AAC;

// Main loop (docs/work/platform-timing.md "메인 루프 분석 결과"). Every tick ends
// in 0x4AB2F0, whose first instruction `call WaitFrameEnd` (E8 3B 98 FF FF) is
// at kWaitFrameEndCall. WaitFrameEnd flips (ID3DXContext::UpdateFrame) and then
// waits. When a frame was late it sets kFrameSkipCount, and while that is not 0
// the next ticks skip BeginScene, the sorted draws and EndScene.
// Evidence: Ghidra decompile of 0x4A5920, 0x49DBD0, 0x4AB2F0, 0x4A4B30, 0x4AB320, 0x4AB350.
constexpr std::uintptr_t kWaitFrameEnd = 0x4A4B30;
constexpr std::uintptr_t kWaitFrameEndCall = 0x4AB2F0;
constexpr std::uintptr_t kFrameSkipCount = 0x7DCE2C;  // int32
constexpr std::uintptr_t kFrameTimeMs = 0x597444;     // float, 16.6667
// Direct3D objects, all from the D3DX context (D3DX 7 is linked statically):
// GetDD (+0x0C) and GetD3DDevice (+0x14) at 0x4A41C0.
constexpr std::uintptr_t kD3dxContext = 0x7DCDE8;  // ID3DXContext*
constexpr std::uintptr_t kDirectDraw = 0x7DCDEC;   // IDirectDraw7*
constexpr std::uintptr_t kD3dDevice = 0x7DCDF4;    // IDirect3DDevice7*

// Input update (docs/work/platform-input.md "입력 갱신 분석 결과").
// UpdateInput 0x41E120 runs once per frame; its only caller is the
// `call 0x41E120` (E8 2B FB 00 00) at kUpdateInputCall, the first instruction of 0x40E5F0.
// Evidence: Ghidra decompile and byte decoding of 0x41E120..0x41E63B.
constexpr std::uintptr_t kUpdateInput = 0x41E120;
constexpr std::uintptr_t kUpdateInputCall = 0x40E5F0;
// GetCursorClientPos 0x41E670 (cursor in client pixels); called at these two places.
constexpr std::uintptr_t kGetCursorClientPos = 0x41E670;
constexpr std::uintptr_t kGetCursorClientPosCall1 = 0x41E4DA;  // in UpdateInput
constexpr std::uintptr_t kGetCursorClientPosCall2 = 0x41E928;  // in CreateInputDevices
// Original readers still used by the reimplementation (gun joystick, SEGAJOY.VXD gun).
// int __cdecl(ButtonRecord* record, int device_or_gun, int slot).
constexpr std::uintptr_t kReadGunJoystick = 0x41F000;
constexpr std::uintptr_t kReadSegaGun = 0x41F210;

// Projection and screen-space drawing (docs/work/renderer.md "화면 비율 분석").
// SetPerspective 0x4AB160(fov, aspect, near, far) is called from 0x418590 and
// 0x419590 with `push 0x3FAAAAAB` (4/3); these are the immediates of those pushes.
// Evidence: Ghidra decompile and byte decoding of 0x4185C9..0x4185DD, 0x4195BB..0x4195CB.
constexpr std::uintptr_t kPerspectiveAspectImmediate1 = 0x4185D4;
constexpr std::uintptr_t kPerspectiveAspectImmediate2 = 0x4195C2;
// DrawScreenQuad 0x4A6ED0(sprite): the only caller of the XYZRHW DrawPrimitive
// wrapper besides the screen flash. Called at these two places.
constexpr std::uintptr_t kDrawScreenQuad = 0x4A6ED0;
constexpr std::uintptr_t kDrawScreenQuadCall1 = 0x4A6EBE;  // in SubmitScreenQuad 0x4A6E60
constexpr std::uintptr_t kDrawScreenQuadCall2 = 0x4A7DF9;  // in FlushSortedDraws 0x4A7D00
// Model draws: 0x4A7310 / 0x4A7860 (model record: +0xC model, +0x34 world-view
// matrix passed to SetTransform(WORLD)). Called at these places.
constexpr std::uintptr_t kDrawModel = 0x4A7310;
constexpr std::uintptr_t kDrawModelAlt = 0x4A7860;
constexpr std::size_t kDrawModelSize = 0x492;     // bytes, from the Ghidra function bodies
constexpr std::size_t kDrawModelAltSize = 0x4A0;
constexpr std::uintptr_t kDrawModelCall1 = 0x4A72AF;     // DrawModel in 0x4A7270
constexpr std::uintptr_t kDrawModelCall2 = 0x4A7DEE;     // DrawModel in FlushSortedDraws
constexpr std::uintptr_t kDrawModelAltCall1 = 0x4A77EF;  // DrawModelAlt in 0x4A77B0
constexpr std::uintptr_t kDrawModelAltCall2 = 0x4A7DE4;  // DrawModelAlt in FlushSortedDraws
// Model table: 16-byte entries, +4 model pointer, +0xC flags (0x8000 | 1 = drawable).
// Entry 0x93E is the cinematic letterbox bar drawn by 0x413A40.
constexpr std::uintptr_t kModelTable = 0x9A4920;
constexpr std::size_t kModelTableStride = 0x10;
constexpr std::size_t kLetterboxModelIndex = 0x93E;
// 4:3 horizontal limits in software checks (docs/work/renderer.md "4:3 가로 범위").
// Each entry is the address of the 32-bit absolute operand of an x87 instruction
// that reads 320.0f (0x4C39CC), -320.0f (0x4D0D10) or 0.5f (0x4C33AC, as focal*0.5).
inline constexpr std::uintptr_t kHalfWidthOperands[] = {
    0x406335,  // fld [320] in 0x406330 (half view angle)
    0x4455DE, 0x4455EB,  // fcomp/fcom [320] in 0x445530 (screen box)
    0x45C6F2, 0x45C6FF,  // same in 0x45C620
    0x44C794,  // fld [320] in 0x44C760
    0x40D16A,  // fcomp [320] in MoveAimDigital 0x40D000
};
inline constexpr std::uintptr_t kNegHalfWidthOperands[] = {
    0x4455FC, 0x445609,  // fcomp [-320] in 0x445530
    0x45C710, 0x45C71D,  // same in 0x45C620
    0x40D181,  // fcomp [-320] in MoveAimDigital
};
constexpr std::uintptr_t kIsInScreenRectHalfOperand = 0x409C38;  // fmul [0.5] in IsInScreenRect
// MoveAimDigital clamps with `mov [esi+8], 0x43A00000 / 0xC3A00000` (+-320.0f); immediates:
constexpr std::uintptr_t kMoveAimClampImmediate = 0x40D178;
constexpr std::uintptr_t kMoveAimClampNegImmediate = 0x40D18F;
// UpdateAim passes the x range to MapAimAxis as `push 0x13F` (319) and
// `push 0xFFFFFEC0` (-320); MapAimAxis returns 0 (invalid) outside it. Immediates:
inline constexpr std::uintptr_t kAimMaxXImmediates[] = {0x40CCD3, 0x40CD67};
inline constexpr std::uintptr_t kAimMinXImmediates[] = {0x40CCD8, 0x40CD75};
// UpdateAim loads the raw x zero-extended (`xor eax,eax; mov ax,[esi+0x9C7240]`,
// 9 bytes), so a negative cursor x becomes 65536 - x. Starts of those pairs:
inline constexpr std::uintptr_t kAimRawXLoads[] = {0x40CCC8, 0x40CD6B};
constexpr std::uintptr_t k320FloatConstant = 0x4C39CC;
constexpr std::uintptr_t kMinus320FloatConstant = 0x4D0D10;
constexpr std::uintptr_t kHalfFloatConstant = 0x4C33AC;
// Sprite draws: DrawSprite2D(int *def) and DrawSpriteAt(sprite, x, y, depth, sx, sy, p7, p8).
// Their call sites are listed in src/core/patches/hud_call_sites.h.
constexpr std::uintptr_t kDrawSprite2D = 0x41C700;
constexpr std::uintptr_t kDrawSpriteAt = 0x41C7A0;
constexpr std::uintptr_t kDrawSpriteAtInnerCall = 0x41C818;  // DrawSpriteAt -> DrawSprite2D
// Pointers to the texture size tables ({uint32 width, uint32 height} per id)
// DrawScreenQuad reads; the second one is for ids with the high bit set.
constexpr std::uintptr_t kTextureSizeTable = 0x80EC80;
constexpr std::uintptr_t kTextureSizeTable2 = 0x80ECC0;

// Input globals. Layouts: thod2/platform/input.h.
constexpr std::uintptr_t kInputSlotStates = 0x7D98F8;      // [24][0x78]
constexpr std::uintptr_t kInputPlayer2Index = 0x7D98EC;    // int, Hod2.ini PLAYER2_INDEX
constexpr std::uintptr_t kInputJoystickPrevious = 0x7DA9B0;  // uint32[8]
constexpr std::uintptr_t kInputCursor = 0x7DA9D8;          // int32[24][2]
constexpr std::uintptr_t kInputReloadFlags = 0x7DA450;     // uint32[24]
constexpr std::uintptr_t kInputKeyChars = 0x7DA4B0;        // uint32[256]
constexpr std::uintptr_t kInputKeyState = 0x7DA8B0;        // uint8[256]
constexpr std::uintptr_t kInputPlayer1Index = 0x7DAABC;    // int, Hod2.ini PLAYER1_INDEX
constexpr std::uintptr_t kInputMouseRecord = 0x7DAAC0;     // ButtonRecord
constexpr std::uintptr_t kInputKeyboardRecords = 0x7DAAF4;  // ButtonRecord[2]
constexpr std::uintptr_t kInputJoystickRecords = 0x7DAB5C;  // ButtonRecord[8]
constexpr std::uintptr_t kInputKeyEvents = 0x7DAC98;       // DIDEVICEOBJECTDATA[16]
constexpr std::uintptr_t kInputTriggerFlags = 0x7DADA8;    // uint32[24]
constexpr std::uintptr_t kInputNameEntry = 0x7DAE08;       // uint8[8]
constexpr std::uintptr_t kInputJoysticks = 0x7DAE18;       // IDirectInputDevice7A*[8] (g_diDevices)
constexpr std::uintptr_t kInputPlayerRecords = 0x7DAE40;   // ButtonRecord[24]
constexpr std::uintptr_t kInputPlayer2Mode = 0x7DB320;     // uint32
constexpr std::uintptr_t kInputKeyboard = 0x7DB324;        // IDirectInputDevice7A*
constexpr std::uintptr_t kInputMouse = 0x7DB328;           // IDirectInputDevice7A*
constexpr std::uintptr_t kInputJoystickCount = 0x7DB334;   // uint32
constexpr std::uintptr_t kInputCursorCallback = 0x7DB338;  // int (__cdecl*)(void), one-shot
constexpr std::uintptr_t kInputKeyboardPrevious = 0x7DB33C;  // uint32
constexpr std::uintptr_t kInputMousePrevious = 0x7DB340;   // uint32
constexpr std::uintptr_t kInputPlayer1Mode = 0x587D14;     // uint32 (initialized data)
constexpr std::uintptr_t kInputJoystickButtonMap = 0x587CDC;  // uint32[14] (initialized data)
constexpr std::uintptr_t kMainWindow = 0x7DC8AC;           // HWND g_hWnd

// Texture creation (docs/formats/tex.md). CreateTextureFromPvr(entry, pixels)
// builds one texture from a 16-byte bank entry (int16 w, int16 h, uint32 PVR
// format, uint32 offset, int32 texture id) and returns its handle; the three
// calls below are all its callers. All functions are __cdecl.
// Evidence: byte decoding of 0x4ABFC6..0x4ABFE2, 0x4AB690..0x4AB6E7, 0x4ABC76..0x4ABCA5.
constexpr std::uintptr_t kCreateTextureFromPvr = 0x4AB690;
constexpr std::uintptr_t kCreateTextureFromPvrCall1 = 0x4ABFD1;  // in LoadBankTexture 0x4ABF70
constexpr std::uintptr_t kCreateTextureFromPvrCall2 = 0x4ABE1D;  // in LoadModelTextures 0x4ABDA0
constexpr std::uintptr_t kCreateTextureFromPvrCall3 = 0x4ABE42;  // in LoadModelTextures 0x4ABDA0
// IDirectDrawSurface7* (int w, int h, int format_slot): system memory surface.
constexpr std::uintptr_t kCreateTextureSurface = 0x4A53B0;
// handle (IDirectDrawSurface7* surface, uint8 flags): video texture made from it.
constexpr std::uintptr_t kUploadTextureSurface = 0x4A5430;
// Texture banks (docs/formats/tex.md), indexed 1..kTexBankCount: file name
// (const char*), entry table (16-byte entries) and int16 entry count.
constexpr std::uintptr_t kTexBankNames = 0x4D0410;
constexpr std::uintptr_t kTexBankEntries = 0x55A9B8;
constexpr std::uintptr_t kTexBankCounts = 0x55B0A8;
constexpr std::size_t kTexBankCount = 443;
constexpr std::uintptr_t kTextureFormats = 0x7DCE50;   // DDPIXELFORMAT[8], slot 7 = 32-bit
constexpr std::uintptr_t kTextureMaxWidth = 0x7DC94C;  // uint32, device limit
constexpr std::uintptr_t kTextureMaxHeight = 0x7DC950;  // uint32, device limit

// Memory the input update may write: kInputSlotStates up to the end of the
// SEGAJOY gun state (0x7DB368), and the gun state at 0x587D14..0x587D1F.
constexpr std::uintptr_t kInputStateBegin = 0x7D98F8;
constexpr std::uintptr_t kInputStateEnd = kInputStateBegin + 0x1A78;  // 0x7DB370
constexpr std::uintptr_t kInputStateExtraBegin = 0x587D14;
constexpr std::uintptr_t kInputStateExtraEnd = kInputStateExtraBegin + 0xC;  // 0x587D20

}  // namespace thod2::interop::hod2
