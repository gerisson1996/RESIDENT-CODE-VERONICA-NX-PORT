/*
 * Nintendo Switch: the part of the SDL3 API the port uses, over devkitPro's SDL2 (switch-sdl2).
 *
 * The port's sources are SDL3 code; devkitPro ships SDL2 only. This header makes them compile
 * unchanged: SDL2's types whose names SDL3 reuses with another layout (SDL_Event, SDL_AudioSpec,
 * SDL_AudioStream, SDL_DisplayMode, SDL_VirtualJoystickDesc) are renamed SDL2_* while SDL2's
 * headers are read, then the SDL3 ones are declared; functions that SDL3 kept with another
 * signature or return convention (bool for success where SDL2 returns 0) go through macros or
 * the sdl3c_* functions of pc/switch/sdl3compat.c. SDL3COMPAT_IMPL (that file) sees the
 * declarations only, so it can call SDL2's originals.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define SDL_Event SDL2_Event
#define SDL_AudioSpec SDL2_AudioSpec
#define SDL_AudioStream SDL2_AudioStream
#define _SDL_AudioStream _SDL2_AudioStream
#define SDL_DisplayMode SDL2_DisplayMode
#define SDL_VirtualJoystickDesc SDL2_VirtualJoystickDesc
#define SDL_EventType SDL2_EventType
#define SDL_CommonEvent SDL2_CommonEvent
#define SDL_DisplayEvent SDL2_DisplayEvent
#define SDL_WindowEvent SDL2_WindowEvent
#define SDL_KeyboardEvent SDL2_KeyboardEvent
#define SDL_TextInputEvent SDL2_TextInputEvent
#define SDL_MouseMotionEvent SDL2_MouseMotionEvent
#define SDL_MouseButtonEvent SDL2_MouseButtonEvent
#define SDL_MouseWheelEvent SDL2_MouseWheelEvent
#define SDL_UserEvent SDL2_UserEvent
#define SDL_QuitEvent SDL2_QuitEvent
#include <SDL2/SDL.h>
#undef SDL_EventType
#undef SDL_CommonEvent
#undef SDL_DisplayEvent
#undef SDL_WindowEvent
#undef SDL_KeyboardEvent
#undef SDL_TextInputEvent
#undef SDL_MouseMotionEvent
#undef SDL_MouseButtonEvent
#undef SDL_MouseWheelEvent
#undef SDL_UserEvent
#undef SDL_QuitEvent
#undef SDL_Event
#undef SDL_AudioSpec
#undef SDL_AudioStream
#undef _SDL_AudioStream
#undef SDL_DisplayMode
#undef SDL_VirtualJoystickDesc

#ifdef __cplusplus
extern "C" {
#endif

/* ---- plain renames ---- */
typedef SDL_mutex SDL_Mutex;
typedef SDL_cond SDL_Condition;
typedef SDL_GameController SDL_Gamepad;
typedef SDL_atomic_t SDL_AtomicInt;
typedef struct SDL_AtomicU32 { Uint32 value; } SDL_AtomicU32;
typedef void SDL_SharedObject;
typedef Uint32 SDL_DisplayID;
typedef Uint32 SDL_WindowID;
typedef Uint32 SDL_PropertiesID;
typedef Uint32 SDL_MouseButtonFlags;
typedef Sint64 SDL_Time;

#define SDL_INIT_GAMEPAD SDL_INIT_GAMECONTROLLER
#define SDL_SCANCODE_COUNT SDL_NUM_SCANCODES
#define SDL_KMOD_NONE KMOD_NONE
#define SDL_BUTTON_MASK(X) SDL_BUTTON(X)
#define SDL_HINT_VIDEO_DRIVER SDL_HINT_VIDEODRIVER
#define SDL_JOYSTICK_TYPE_GAMEPAD SDL_JOYSTICK_TYPE_GAMECONTROLLER
#define SDL_BORDERLESS_WINDOWED_STYLE 0

/* ---- gamepads (SDL3 numbering; 0..20 are SDL2's too) ---- */
typedef enum SDL_GamepadButton {
    SDL_GAMEPAD_BUTTON_INVALID = -1,
    SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH,
    SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_GUIDE, SDL_GAMEPAD_BUTTON_START,
    SDL_GAMEPAD_BUTTON_LEFT_STICK, SDL_GAMEPAD_BUTTON_RIGHT_STICK,
    SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
    SDL_GAMEPAD_BUTTON_MISC1,
    SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,
    SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2, SDL_GAMEPAD_BUTTON_LEFT_PADDLE2,
    SDL_GAMEPAD_BUTTON_TOUCHPAD,
    SDL_GAMEPAD_BUTTON_MISC2, SDL_GAMEPAD_BUTTON_MISC3, SDL_GAMEPAD_BUTTON_MISC4,
    SDL_GAMEPAD_BUTTON_MISC5, SDL_GAMEPAD_BUTTON_MISC6,
    SDL_GAMEPAD_BUTTON_COUNT
} SDL_GamepadButton;

typedef enum SDL_GamepadAxis {
    SDL_GAMEPAD_AXIS_INVALID = -1,
    SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY, SDL_GAMEPAD_AXIS_RIGHTX, SDL_GAMEPAD_AXIS_RIGHTY,
    SDL_GAMEPAD_AXIS_LEFT_TRIGGER, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,
    SDL_GAMEPAD_AXIS_COUNT
} SDL_GamepadAxis;

typedef enum SDL_GamepadType {
    SDL_GAMEPAD_TYPE_UNKNOWN, SDL_GAMEPAD_TYPE_STANDARD, SDL_GAMEPAD_TYPE_XBOX360, SDL_GAMEPAD_TYPE_XBOXONE,
    SDL_GAMEPAD_TYPE_PS3, SDL_GAMEPAD_TYPE_PS4, SDL_GAMEPAD_TYPE_PS5,
    SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO, SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT,
    SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT, SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR,
    SDL_GAMEPAD_TYPE_GAMECUBE,
    SDL_GAMEPAD_TYPE_COUNT
} SDL_GamepadType;

typedef enum SDL_GamepadButtonLabel {
    SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN, SDL_GAMEPAD_BUTTON_LABEL_A, SDL_GAMEPAD_BUTTON_LABEL_B,
    SDL_GAMEPAD_BUTTON_LABEL_X, SDL_GAMEPAD_BUTTON_LABEL_Y, SDL_GAMEPAD_BUTTON_LABEL_CROSS,
    SDL_GAMEPAD_BUTTON_LABEL_CIRCLE, SDL_GAMEPAD_BUTTON_LABEL_SQUARE, SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE
} SDL_GamepadButtonLabel;

/* ---- events: SDL3's layout; a translated event carries the SDL2 original (for Dear ImGui) ---- */
typedef enum SDL_EventType {
    SDL_EVENT_FIRST = 0,
    SDL_EVENT_QUIT = 0x100,
    SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED = 0x155,
    SDL_EVENT_WINDOW_RESIZED = 0x206,
    SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED = 0x207,
    SDL_EVENT_WINDOW_FOCUS_GAINED = 0x20E,
    SDL_EVENT_WINDOW_FOCUS_LOST = 0x20F,
    SDL_EVENT_WINDOW_CLOSE_REQUESTED = 0x210,
    SDL_EVENT_WINDOW_DISPLAY_CHANGED = 0x213,
    SDL_EVENT_KEY_DOWN = 0x300, SDL_EVENT_KEY_UP, SDL_EVENT_TEXT_EDITING, SDL_EVENT_TEXT_INPUT,
    SDL_EVENT_MOUSE_MOTION = 0x400, SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_EVENT_MOUSE_BUTTON_UP, SDL_EVENT_MOUSE_WHEEL,
    SDL_EVENT_GAMEPAD_AXIS_MOTION = 0x650, SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_EVENT_GAMEPAD_BUTTON_UP,
    SDL_EVENT_GAMEPAD_ADDED, SDL_EVENT_GAMEPAD_REMOVED,
    SDL_EVENT_COMPAT_OTHER = 0x7F00,   /* an SDL2 event with no SDL3 counterpart the port reads */
    SDL_EVENT_USER = 0x8000
} SDL_EventType;

#define SDL3C_EVHDR Uint32 type; Uint32 reserved; Uint64 timestamp
typedef struct { SDL3C_EVHDR; } SDL_CommonEvent;
typedef struct { SDL3C_EVHDR; SDL_DisplayID displayID; Sint32 data1, data2; } SDL_DisplayEvent;
typedef struct { SDL3C_EVHDR; SDL_WindowID windowID; Sint32 data1, data2; } SDL_WindowEvent;
typedef struct { SDL3C_EVHDR; SDL_WindowID windowID; Uint32 which; SDL_Scancode scancode; SDL_Keycode key; Uint16 mod; Uint16 raw; bool down; bool repeat; } SDL_KeyboardEvent;
typedef struct { SDL3C_EVHDR; SDL_WindowID windowID; const char* text; } SDL_TextInputEvent;
typedef struct { SDL3C_EVHDR; SDL_WindowID windowID; Uint32 which; SDL_MouseButtonFlags state; float x, y, xrel, yrel; } SDL_MouseMotionEvent;
typedef struct { SDL3C_EVHDR; SDL_WindowID windowID; Uint32 which; Uint8 button; bool down; Uint8 clicks; Uint8 padding; float x, y; } SDL_MouseButtonEvent;
typedef struct { SDL3C_EVHDR; SDL_WindowID windowID; Uint32 which; float x, y; Uint32 direction; float mouse_x, mouse_y; } SDL_MouseWheelEvent;
typedef struct { SDL3C_EVHDR; SDL_JoystickID which; Uint8 axis; Uint8 padding1, padding2, padding3; Sint16 value; Uint16 padding4; } SDL_GamepadAxisEvent;
typedef struct { SDL3C_EVHDR; SDL_JoystickID which; Uint8 button; bool down; Uint8 padding1, padding2; } SDL_GamepadButtonEvent;
typedef struct { SDL3C_EVHDR; SDL_JoystickID which; } SDL_GamepadDeviceEvent;
typedef struct { SDL3C_EVHDR; SDL_WindowID windowID; Sint32 code; void* data1; void* data2; } SDL_UserEvent;
typedef struct { SDL3C_EVHDR; } SDL_QuitEvent;

typedef union SDL_Event {
    Uint32 type;
    SDL_CommonEvent common;
    SDL_DisplayEvent display;
    SDL_WindowEvent window;
    SDL_KeyboardEvent key;
    SDL_TextInputEvent text;
    SDL_MouseMotionEvent motion;
    SDL_MouseButtonEvent button;
    SDL_MouseWheelEvent wheel;
    SDL_GamepadAxisEvent gaxis;
    SDL_GamepadButtonEvent gbutton;
    SDL_GamepadDeviceEvent gdevice;
    SDL_UserEvent user;
    SDL_QuitEvent quit;
    struct { Uint8 head[64]; SDL2_Event ev; } sdl2;   /* the original; type 0 when pushed by the port */
    Uint8 padding[128];
} SDL_Event;

/* ---- audio: SDL3 streams mixed by one SDL2 device callback ---- */
typedef struct SDL_AudioSpec { SDL_AudioFormat format; int channels; int freq; } SDL_AudioSpec;
typedef struct SDL_AudioStream SDL_AudioStream;
typedef void (SDLCALL *SDL_AudioStreamCallback)(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount);
#define SDL_AUDIO_S16 AUDIO_S16SYS
#define SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK ((SDL_AudioDeviceID)0xFFFFFFFFu)

/* ---- displays ---- */
typedef struct SDL_DisplayMode {
    SDL_DisplayID displayID; Uint32 format; int w, h; float pixel_density; float refresh_rate;
    int refresh_rate_numerator, refresh_rate_denominator; void* internal;
} SDL_DisplayMode;

/* ---- filesystem ---- */
typedef enum SDL_PathType { SDL_PATHTYPE_NONE, SDL_PATHTYPE_FILE, SDL_PATHTYPE_DIRECTORY, SDL_PATHTYPE_OTHER } SDL_PathType;
typedef struct SDL_PathInfo { SDL_PathType type; Uint64 size; SDL_Time create_time, modify_time, access_time; } SDL_PathInfo;
typedef enum SDL_EnumerationResult { SDL_ENUM_CONTINUE, SDL_ENUM_SUCCESS, SDL_ENUM_FAILURE } SDL_EnumerationResult;
typedef SDL_EnumerationResult (SDLCALL *SDL_EnumerateDirectoryCallback)(void* userdata, const char* dirname, const char* fname);
typedef Uint32 SDL_GlobFlags;
#define SDL_GLOB_CASEINSENSITIVE (1u << 0)

typedef struct SDL_DialogFileFilter { const char* name; const char* pattern; } SDL_DialogFileFilter;
typedef void (SDLCALL *SDL_DialogFileCallback)(void* userdata, const char* const* filelist, int filter);
typedef enum SDL_FileDialogType { SDL_FILEDIALOG_OPENFILE, SDL_FILEDIALOG_SAVEFILE, SDL_FILEDIALOG_OPENFOLDER } SDL_FileDialogType;

/* ---- properties: none are kept; the getters answer their default (rumble aside) ---- */
#define SDL_PROP_WINDOW_SDR_WHITE_LEVEL_FLOAT "SDL.window.SDR_white_level"
#define SDL_PROP_WINDOW_HDR_HEADROOM_FLOAT "SDL.window.HDR_headroom"
#define SDL_PROP_WINDOW_HDR_ENABLED_BOOLEAN "SDL.window.HDR_enabled"
#define SDL_PROP_WINDOW_WIN32_HWND_POINTER "SDL.window.win32.hwnd"
#define SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN "SDL.joystick.cap.rumble"
#define SDL_PROP_FILE_DIALOG_FILTERS_POINTER "SDL.filedialog.filters"
#define SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER "SDL.filedialog.nfilters"
#define SDL_PROP_FILE_DIALOG_TITLE_STRING "SDL.filedialog.title"

/* ---- virtual joysticks (a test aid on PC): attaching fails ---- */
typedef struct SDL_VirtualJoystickDesc {
    Uint32 version; Uint16 type, padding, vendor_id, product_id, naxes, nbuttons, nballs, nhats, ntouchpads, nsensors;
    Uint16 padding2[2]; Uint32 button_mask, axis_mask; const char* name;
    const void* touchpads; const void* sensors; void* userdata;
    void (SDLCALL *Update)(void*); void (SDLCALL *SetPlayerIndex)(void*, int);
    bool (SDLCALL *Rumble)(void*, Uint16, Uint16); bool (SDLCALL *RumbleTriggers)(void*, Uint16, Uint16);
    bool (SDLCALL *SetLED)(void*, Uint8, Uint8, Uint8); bool (SDLCALL *SendEffect)(void*, const void*, int);
    bool (SDLCALL *SetSensorsEnabled)(void*, bool); void (SDLCALL *Cleanup)(void*);
} SDL_VirtualJoystickDesc;
#define SDL_INIT_INTERFACE(d) (SDL_memset((d), 0, sizeof(*(d))), (d)->version = (Uint32)sizeof(*(d)))

/* ---- the sdl3c_* functions (sdl3compat.c) ---- */
bool sdl3c_PollEvent(SDL_Event* e);
bool sdl3c_WaitEventTimeout(SDL_Event* e, Sint32 ms);
bool sdl3c_PushEvent(SDL_Event* e);

SDL_AudioDeviceID sdl3c_OpenAudioDevice(SDL_AudioDeviceID dev, const SDL_AudioSpec* spec);
SDL_AudioStream* sdl3c_CreateAudioStream(const SDL_AudioSpec* src, const SDL_AudioSpec* dst);
void sdl3c_DestroyAudioStream(SDL_AudioStream* s);
bool sdl3c_PutAudioStreamData(SDL_AudioStream* s, const void* buf, int len);
int sdl3c_GetAudioStreamQueued(SDL_AudioStream* s);
int sdl3c_GetAudioStreamAvailable(SDL_AudioStream* s);
bool sdl3c_FlushAudioStream(SDL_AudioStream* s);
bool sdl3c_SetAudioStreamGain(SDL_AudioStream* s, float gain);
bool sdl3c_SetAudioStreamGetCallback(SDL_AudioStream* s, SDL_AudioStreamCallback cb, void* userdata);
bool sdl3c_BindAudioStream(SDL_AudioDeviceID dev, SDL_AudioStream* s);
void sdl3c_UnbindAudioStream(SDL_AudioStream* s);
bool sdl3c_LockAudioStream(SDL_AudioStream* s);
bool sdl3c_UnlockAudioStream(SDL_AudioStream* s);

Uint64 sdl3c_GetTicksNS(void);
void sdl3c_DelayNS(Uint64 ns);
void sdl3c_DelayPrecise(Uint64 ns);

SDL_JoystickID* sdl3c_GetGamepads(int* count);
SDL_Gamepad* sdl3c_OpenGamepad(SDL_JoystickID id);
const char* sdl3c_GetGamepadNameForID(SDL_JoystickID id);
int sdl3c_GetGamepadPlayerIndexForID(SDL_JoystickID id);
SDL_GamepadType sdl3c_GetGamepadType(SDL_Gamepad* g);
SDL_GamepadButtonLabel sdl3c_GetGamepadButtonLabelForType(SDL_GamepadType t, SDL_GamepadButton b);
SDL_PropertiesID sdl3c_GetGamepadProperties(SDL_Gamepad* g);
SDL_Joystick* sdl3c_OpenJoystick(SDL_JoystickID id);
SDL_JoystickID sdl3c_AttachVirtualJoystick(const SDL_VirtualJoystickDesc* d);

SDL_Window* sdl3c_CreateWindow(const char* title, int w, int h, Uint64 flags);
bool sdl3c_SetWindowFullscreen(SDL_Window* w, bool on);
bool sdl3c_SetWindowFullscreenMode(SDL_Window* w, const SDL_DisplayMode* m);
SDL_DisplayID* sdl3c_GetDisplays(int* count);
const SDL_DisplayMode* sdl3c_GetDesktopDisplayMode(SDL_DisplayID d);
const SDL_DisplayMode* sdl3c_GetCurrentDisplayMode(SDL_DisplayID d);
SDL_DisplayMode** sdl3c_GetFullscreenDisplayModes(SDL_DisplayID d, int* count);
bool sdl3c_GetClosestFullscreenDisplayMode(SDL_DisplayID d, int w, int h, float hz, bool hd, SDL_DisplayMode* out);
SDL_MouseButtonFlags sdl3c_GetMouseState(float* x, float* y);

const char* sdl3c_GetBasePath(void);
char* sdl3c_GetCurrentDirectory(void);
bool sdl3c_GetPathInfo(const char* path, SDL_PathInfo* info);
bool sdl3c_CreateDirectory(const char* path);
bool sdl3c_RemovePath(const char* path);
bool sdl3c_EnumerateDirectory(const char* path, SDL_EnumerateDirectoryCallback cb, void* userdata);
char** sdl3c_GlobDirectory(const char* path, const char* pattern, SDL_GlobFlags flags, int* count);
void sdl3c_ShowFileDialogWithProperties(SDL_FileDialogType type, SDL_DialogFileCallback cb, void* userdata, SDL_PropertiesID props);

bool sdl3c_FlipSurface(SDL_Surface* s, int flip);
SDL_Surface* sdl3c_LoadPNG(const char* path);
bool sdl3c_SavePNG(SDL_Surface* s, const char* path);

bool sdl3c_GetBooleanProperty(SDL_PropertiesID p, const char* name, bool def);
void sdl3c_SetAtomicU32(SDL_AtomicU32* a, Uint32 v);
Uint32 sdl3c_GetAtomicU32(SDL_AtomicU32* a);

static inline bool sdl3c_ShowCursor(void) { return SDL_ShowCursor(SDL_ENABLE) >= 0; }
static inline bool sdl3c_HideCursor(void) { return SDL_ShowCursor(SDL_DISABLE) >= 0; }

#ifndef SDL3COMPAT_IMPL
/* events */
#define SDL_PollEvent sdl3c_PollEvent
#define SDL_WaitEventTimeout sdl3c_WaitEventTimeout
#define SDL_PushEvent sdl3c_PushEvent

/* init, hints, messages */
#define SDL_Init(f) (SDL_Init(f) == 0)
#define SDL_InitSubSystem(f) (SDL_InitSubSystem(f) == 0)
#define SDL_setenv_unsafe SDL_setenv
#define SDL_ShowSimpleMessageBox(f, t, m, w) (SDL_ShowSimpleMessageBox(f, t, m, w) == 0)
#define SDL_OpenURL(u) ((void)(u), SDL_SetError("no browser"), false)

/* time, threads, atomics */
#define SDL_GetTicks SDL_GetTicks64
#define SDL_GetTicksNS sdl3c_GetTicksNS
#define SDL_DelayNS sdl3c_DelayNS
#define SDL_DelayPrecise sdl3c_DelayPrecise
#define SDL_CreateCondition SDL_CreateCond
#define SDL_DestroyCondition SDL_DestroyCond
#define SDL_SignalCondition SDL_CondSignal
#define SDL_BroadcastCondition SDL_CondBroadcast
#define SDL_WaitCondition(c, m) ((void)SDL_CondWait(c, m))
#define SDL_WaitConditionTimeout(c, m, ms) (SDL_CondWaitTimeout(c, m, ms) == 0)
#define SDL_SetAtomicInt SDL_AtomicSet
#define SDL_GetAtomicInt SDL_AtomicGet
#define SDL_AddAtomicInt SDL_AtomicAdd
#define SDL_CompareAndSwapAtomicInt SDL_AtomicCAS
#define SDL_SetAtomicU32 sdl3c_SetAtomicU32
#define SDL_GetAtomicU32 sdl3c_GetAtomicU32
#define SDL_LockSpinlock SDL_AtomicLock
#define SDL_UnlockSpinlock SDL_AtomicUnlock

/* audio */
#define SDL_OpenAudioDevice sdl3c_OpenAudioDevice
#define SDL_CreateAudioStream sdl3c_CreateAudioStream
#define SDL_DestroyAudioStream sdl3c_DestroyAudioStream
#define SDL_PutAudioStreamData sdl3c_PutAudioStreamData
#define SDL_GetAudioStreamQueued sdl3c_GetAudioStreamQueued
#define SDL_GetAudioStreamAvailable sdl3c_GetAudioStreamAvailable
#define SDL_FlushAudioStream sdl3c_FlushAudioStream
#define SDL_SetAudioStreamGain sdl3c_SetAudioStreamGain
#define SDL_SetAudioStreamGetCallback sdl3c_SetAudioStreamGetCallback
#define SDL_BindAudioStream sdl3c_BindAudioStream
#define SDL_UnbindAudioStream sdl3c_UnbindAudioStream
#define SDL_LockAudioStream sdl3c_LockAudioStream
#define SDL_UnlockAudioStream sdl3c_UnlockAudioStream

/* gamepads, joysticks */
#define SDL_GetGamepads sdl3c_GetGamepads
#define SDL_OpenGamepad sdl3c_OpenGamepad
#define SDL_CloseGamepad SDL_GameControllerClose
#define SDL_GamepadConnected(g) (SDL_GameControllerGetAttached(g) != 0)
/* Switch: SDL2 reports the face buttons by position (physical B = SOUTH), while the
   game wants the label (physical A = confirm = SOUTH). Swap A<->B and X<->Y (ids 0..3,
   xor 1) here and in the button events, so SOUTH is physical A everywhere. */
#define SDL3C_SWAP_FACE(b) ((int)(b) < 4 ? ((int)(b) ^ 1) : (int)(b))
#define SDL_GetGamepadButton(g, b) (SDL_GameControllerGetButton(g, (SDL_GameControllerButton)SDL3C_SWAP_FACE(b)) != 0)
#define SDL_GetGamepadAxis(g, a) SDL_GameControllerGetAxis(g, (SDL_GameControllerAxis)(a))
#define SDL_GetGamepadAxisFromString(s) ((SDL_GamepadAxis)SDL_GameControllerGetAxisFromString(s))
#define SDL_GetGamepadName SDL_GameControllerName
#define SDL_GetGamepadNameForID sdl3c_GetGamepadNameForID
#define SDL_GetGamepadPlayerIndexForID sdl3c_GetGamepadPlayerIndexForID
#define SDL_GetGamepadID(g) ((g) ? SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(g)) + 1 : 0)   /* + 1: sdl3compat.c device_index */
#define SDL_GetGamepadFromID(id) SDL_GameControllerFromInstanceID((SDL_JoystickID)(id) - 1)
#define SDL_GetGamepadType sdl3c_GetGamepadType
#define SDL_GetGamepadButtonLabelForType sdl3c_GetGamepadButtonLabelForType
#define SDL_GetGamepadProperties sdl3c_GetGamepadProperties
#define SDL_RumbleGamepad(g, lo, hi, ms) (SDL_GameControllerRumble(g, lo, hi, ms) == 0)
#define SDL_OpenJoystick sdl3c_OpenJoystick
#define SDL_CloseJoystick SDL_JoystickClose
#define SDL_GetJoystickID(j) ((j) ? SDL_JoystickInstanceID(j) + 1 : 0)
#define SDL_AttachVirtualJoystick sdl3c_AttachVirtualJoystick
#define SDL_DetachVirtualJoystick(id) ((void)(id), false)
#define SDL_SetJoystickVirtualAxis(j, a, v) ((void)(j), (void)(a), (void)(v), false)
#define SDL_SetJoystickVirtualButton(j, b, v) ((void)(j), (void)(b), (void)(v), false)

/* keyboard, mouse */
#define SDL_GetKeyboardState(n) ((const bool*)SDL_GetKeyboardState(n))
#define SDL_GetKeyFromScancode(sc, mod, ev) SDL_GetKeyFromScancode(sc)
#define SDL_GetMouseState sdl3c_GetMouseState
#define SDL_ShowCursor sdl3c_ShowCursor
#define SDL_HideCursor sdl3c_HideCursor
#define SDL_SetWindowRelativeMouseMode(w, on) ((void)(w), SDL_SetRelativeMouseMode((SDL_bool)(on)) == 0)
#define SDL_HasRectIntersection SDL_HasIntersection

/* windows, displays, GL */
#define SDL_CreateWindow sdl3c_CreateWindow
#define SDL_GetWindowSizeInPixels(w, x, y) (SDL_GL_GetDrawableSize(w, x, y), true)
#define SDL_GetWindowSize(w, x, y) (SDL_GetWindowSize(w, x, y), true)
#define SDL_GetWindowPosition(w, x, y) (SDL_GetWindowPosition(w, x, y), true)
#ifdef __SWITCH__
/* one window, the whole screen, always in front: no resizing, and the input focus is its own
   (SDL2 here never gives it, and the port reads the gamepad only while focused) */
#define SDL_SetWindowSize(w, x, y) ((void)(w), (void)(x), (void)(y), true)
#define SDL_GetWindowFlags(w) ((Uint64)SDL_GetWindowFlags(w) | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_FULLSCREEN)
#else
#define SDL_SetWindowSize(w, x, y) (SDL_SetWindowSize(w, x, y), true)
#endif
#define SDL_SetWindowPosition(w, x, y) (SDL_SetWindowPosition(w, x, y), true)
#define SDL_SetWindowResizable(w, on) (SDL_SetWindowResizable(w, (SDL_bool)(on)), true)
#define SDL_SetWindowBordered(w, on) (SDL_SetWindowBordered(w, (SDL_bool)(on)), true)
#define SDL_ShowWindow(w) (SDL_ShowWindow(w), true)
#define SDL_HideWindow(w) (SDL_HideWindow(w), true)
#define SDL_MaximizeWindow(w) (SDL_MaximizeWindow(w), true)
#define SDL_RestoreWindow(w) (SDL_RestoreWindow(w), true)
#define SDL_SyncWindow(w) ((void)(w), true)
#define SDL_GetWindowPixelDensity(w) ((void)(w), 1.0f)
#define SDL_GetWindowProperties(w) ((void)(w), (SDL_PropertiesID)0)
#define SDL_SetWindowFullscreen sdl3c_SetWindowFullscreen
#define SDL_SetWindowFullscreenMode sdl3c_SetWindowFullscreenMode
#define SDL_GetDisplayForWindow(w) ((SDL_DisplayID)(SDL_GetWindowDisplayIndex(w) + 1))
#define SDL_GetDisplays sdl3c_GetDisplays
#define SDL_GetDisplayBounds(d, r) (SDL_GetDisplayBounds((int)(d) - 1, r) == 0)
#define SDL_GetDesktopDisplayMode sdl3c_GetDesktopDisplayMode
#define SDL_GetCurrentDisplayMode sdl3c_GetCurrentDisplayMode
#define SDL_GetFullscreenDisplayModes sdl3c_GetFullscreenDisplayModes
#define SDL_GetClosestFullscreenDisplayMode sdl3c_GetClosestFullscreenDisplayMode
#define SDL_GL_SetAttribute(a, v) (SDL_GL_SetAttribute(a, v) == 0)
#define SDL_GL_MakeCurrent(w, c) (SDL_GL_MakeCurrent(w, c) == 0)
#define SDL_GL_SetSwapInterval(i) (SDL_GL_SetSwapInterval(i) == 0)
#define SDL_GL_SwapWindow(w) (SDL_GL_SwapWindow(w), true)
#define SDL_GL_DestroyContext(c) (SDL_GL_DeleteContext(c), true)

/* surfaces */
#define SDL_CreateSurface(w, h, f) SDL_CreateRGBSurfaceWithFormat(0, w, h, SDL_BITSPERPIXEL(f), f)
#define SDL_CreateSurfaceFrom(w, h, f, px, pitch) SDL_CreateRGBSurfaceWithFormatFrom(px, w, h, SDL_BITSPERPIXEL(f), pitch, f)
#define SDL_DestroySurface SDL_FreeSurface
#undef SDL_ConvertSurface
#define SDL_ConvertSurface(s, f) SDL_ConvertSurfaceFormat(s, f, 0)
#define SDL_FlipSurface sdl3c_FlipSurface
#undef SDL_SaveBMP
#define SDL_SaveBMP(s, f) (SDL_SaveBMP_RW(s, SDL_RWFromFile(f, "wb"), 1) == 0)
#define SDL_LoadPNG sdl3c_LoadPNG
#define SDL_SavePNG sdl3c_SavePNG

/* filesystem */
#define SDL_GetBasePath sdl3c_GetBasePath
#define SDL_GetCurrentDirectory sdl3c_GetCurrentDirectory
#define SDL_GetPathInfo sdl3c_GetPathInfo
#define SDL_CreateDirectory sdl3c_CreateDirectory
#define SDL_RemovePath sdl3c_RemovePath
#define SDL_EnumerateDirectory sdl3c_EnumerateDirectory
#define SDL_GlobDirectory sdl3c_GlobDirectory
#define SDL_ShowFileDialogWithProperties sdl3c_ShowFileDialogWithProperties

/* properties */
#define SDL_CreateProperties() ((SDL_PropertiesID)1)
#define SDL_DestroyProperties(p) ((void)(p))
#define SDL_SetPointerProperty(p, n, v) ((void)(p), (void)(n), (void)(v), true)
#define SDL_SetNumberProperty(p, n, v) ((void)(p), (void)(n), (void)(v), true)
#define SDL_SetStringProperty(p, n, v) ((void)(p), (void)(n), (void)(v), true)
#define SDL_SetFloatProperty(p, n, v) ((void)(p), (void)(n), (void)(v), true)
#define SDL_SetBooleanProperty(p, n, v) ((void)(p), (void)(n), (void)(v), true)
#define SDL_GetPointerProperty(p, n, d) ((void)(p), (void)(n), (d))
#define SDL_GetNumberProperty(p, n, d) ((void)(p), (void)(n), (d))
#define SDL_GetStringProperty(p, n, d) ((void)(p), (void)(n), (d))
#define SDL_GetFloatProperty(p, n, d) ((void)(p), (void)(n), (d))
#define SDL_GetBooleanProperty sdl3c_GetBooleanProperty
#endif /* SDL3COMPAT_IMPL */

#ifdef __cplusplus
}
#endif
