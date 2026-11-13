/*
 * Nintendo Switch: the SDL3 functions the port uses, on SDL2 (see include/SDL3/SDL.h).
 */
#define SDL3COMPAT_IMPL
#include <SDL3/SDL.h>
#include <SDL2/SDL_image.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define HOME_DIR "sdmc:/switch/recvx/"

/* ================= events ================= */

#define QN 64
static SDL_Event pushed[QN];
static int q_head, q_n;
static SDL_SpinLock q_lock;

bool sdl3c_PushEvent(SDL_Event* e)
{
    bool ok = false;
    SDL_AtomicLock(&q_lock);
    if (q_n < QN) { pushed[(q_head + q_n++) % QN] = *e; pushed[(q_head + q_n - 1) % QN].sdl2.ev.type = 0; ok = true; }
    SDL_AtomicUnlock(&q_lock);
    if (ok) { SDL2_Event wake; SDL_zero(wake); wake.type = SDL_USEREVENT; SDL_PushEvent(&wake); }   /* ends a wait */
    return ok;
}

static bool pop_pushed(SDL_Event* e)
{
    bool ok = false;
    SDL_AtomicLock(&q_lock);
    if (q_n) { *e = pushed[q_head]; q_head = (q_head + 1) % QN; q_n--; ok = true; }
    SDL_AtomicUnlock(&q_lock);
    return ok;
}

static Uint64 ms_ns(Uint32 ms) { return (Uint64)ms * 1000000u; }

static void ensure_mappings(void);

static void translate(const SDL2_Event* s, SDL_Event* e)
{
    SDL_zerop(e);
    e->sdl2.ev = *s;
    e->type = SDL_EVENT_COMPAT_OTHER;
    e->common.timestamp = ms_ns(s->common.timestamp);
    switch (s->type) {
    case SDL_QUIT: e->type = SDL_EVENT_QUIT; break;
    case SDL_KEYDOWN: case SDL_KEYUP:
        e->type = s->type == SDL_KEYDOWN ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        e->key.windowID = s->key.windowID;
        e->key.scancode = s->key.keysym.scancode;
        e->key.key = s->key.keysym.sym;
        e->key.mod = s->key.keysym.mod;
        e->key.down = s->key.state == SDL_PRESSED;
        e->key.repeat = s->key.repeat != 0;
        break;
    case SDL_TEXTINPUT:
        e->type = SDL_EVENT_TEXT_INPUT;
        e->text.windowID = s->text.windowID;
        e->text.text = e->sdl2.ev.text.text;   /* in this event: read it before the next */
        break;
    case SDL_MOUSEMOTION:
        e->type = SDL_EVENT_MOUSE_MOTION;
        e->motion.windowID = s->motion.windowID;
        e->motion.which = s->motion.which;
        e->motion.state = s->motion.state;
        e->motion.x = (float)s->motion.x; e->motion.y = (float)s->motion.y;
        e->motion.xrel = (float)s->motion.xrel; e->motion.yrel = (float)s->motion.yrel;
        break;
    case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP:
        e->type = s->type == SDL_MOUSEBUTTONDOWN ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        e->button.windowID = s->button.windowID;
        e->button.which = s->button.which;
        e->button.button = s->button.button;
        e->button.down = s->button.state == SDL_PRESSED;
        e->button.clicks = s->button.clicks;
        e->button.x = (float)s->button.x; e->button.y = (float)s->button.y;
        break;
    case SDL_MOUSEWHEEL:
        e->type = SDL_EVENT_MOUSE_WHEEL;
        e->wheel.windowID = s->wheel.windowID;
        e->wheel.which = s->wheel.which;
        e->wheel.x = s->wheel.preciseX; e->wheel.y = s->wheel.preciseY;
        e->wheel.direction = s->wheel.direction;
        e->wheel.mouse_x = (float)s->wheel.mouseX; e->wheel.mouse_y = (float)s->wheel.mouseY;
        break;
    case SDL_CONTROLLERAXISMOTION:
        e->type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
        e->gaxis.which = s->caxis.which + 1; e->gaxis.axis = s->caxis.axis; e->gaxis.value = s->caxis.value;
        break;
    case SDL_CONTROLLERBUTTONDOWN: case SDL_CONTROLLERBUTTONUP:
        e->type = s->type == SDL_CONTROLLERBUTTONDOWN ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP;
        e->gbutton.which = s->cbutton.which + 1; e->gbutton.button = s->cbutton.button < 4 ? (Uint8)(s->cbutton.button ^ 1) : s->cbutton.button;   /* A<->B, X<->Y: see SDL3C_SWAP_FACE */
        e->gbutton.down = s->cbutton.state == SDL_PRESSED;
        break;
    case SDL_JOYDEVICEADDED:   /* a joystick without a mapping gets one, then a CONTROLLERDEVICEADDED */
        ensure_mappings();
        break;
    case SDL_CONTROLLERDEVICEADDED:   /* SDL2: a device index here, an instance id elsewhere */
        e->type = SDL_EVENT_GAMEPAD_ADDED;
        e->gdevice.which = SDL_JoystickGetDeviceInstanceID(s->cdevice.which) + 1;
        break;
    case SDL_CONTROLLERDEVICEREMOVED:
        e->type = SDL_EVENT_GAMEPAD_REMOVED;
        e->gdevice.which = s->cdevice.which + 1;
        break;
    case SDL_WINDOWEVENT:
        e->window.windowID = s->window.windowID;
        e->window.data1 = s->window.data1; e->window.data2 = s->window.data2;
        switch (s->window.event) {
        case SDL_WINDOWEVENT_RESIZED: e->type = SDL_EVENT_WINDOW_RESIZED; break;
        case SDL_WINDOWEVENT_SIZE_CHANGED: e->type = SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED; break;
        case SDL_WINDOWEVENT_FOCUS_GAINED: e->type = SDL_EVENT_WINDOW_FOCUS_GAINED; break;
        case SDL_WINDOWEVENT_FOCUS_LOST: e->type = SDL_EVENT_WINDOW_FOCUS_LOST; break;
        case SDL_WINDOWEVENT_CLOSE: e->type = SDL_EVENT_WINDOW_CLOSE_REQUESTED; break;
        case SDL_WINDOWEVENT_DISPLAY_CHANGED: e->type = SDL_EVENT_WINDOW_DISPLAY_CHANGED; break;
        }
        break;
    }
}

/* the wake-up SDL_PushEvent sent above carries nothing */
static bool is_wake(const SDL2_Event* s) { return s->type == SDL_USEREVENT && s->user.code == 0 && !s->user.data1; }

bool sdl3c_PollEvent(SDL_Event* e)
{
    SDL2_Event s;
    /* SDL2's Switch joystick driver skips padUpdate while text input is active (it is on by
       default after video init): the gamepad would never update. Nothing here types text. */
    if (SDL_IsTextInputActive()) {
        extern void host_log(const char* fmt, ...);
        SDL_StopTextInput();
        host_log("switch: text input stopped (the gamepad reads again)\n");
    }
    if (pop_pushed(e)) return true;
    while (SDL_PollEvent(&s)) {
        if (is_wake(&s)) { if (pop_pushed(e)) return true; continue; }
        if (e) translate(&s, e);
        return true;
    }
    return false;
}

bool sdl3c_WaitEventTimeout(SDL_Event* e, Sint32 ms)
{
    SDL2_Event s;
    if (pop_pushed(e)) return true;
    if (!SDL_WaitEventTimeout(&s, ms)) return false;
    if (is_wake(&s)) return pop_pushed(e) || sdl3c_PollEvent(e);
    if (e) translate(&s, e);
    return true;
}

/* ================= time ================= */

Uint64 sdl3c_GetTicksNS(void)
{
    static Uint64 f, c0;
    Uint64 c;
    if (!f) { f = SDL_GetPerformanceFrequency(); c0 = SDL_GetPerformanceCounter(); }
    c = SDL_GetPerformanceCounter() - c0;
    return c / f * 1000000000u + c % f * 1000000000u / f;
}
/* a real symbol too: movie_glue.c declares it itself, without the SDL header */
Uint64 SDL_GetTicksNS(void) { return sdl3c_GetTicksNS(); }

void sdl3c_DelayNS(Uint64 ns)
{
    struct timespec t = { (time_t)(ns / 1000000000u), (long)(ns % 1000000000u) };
    nanosleep(&t, NULL);
}

void sdl3c_DelayPrecise(Uint64 ns)
{
    Uint64 end = sdl3c_GetTicksNS() + ns, now;
    while ((now = sdl3c_GetTicksNS()) + 1000000u < end) sdl3c_DelayNS(end - now - 1000000u);
    while (sdl3c_GetTicksNS() < end) {}
}

void sdl3c_SetAtomicU32(SDL_AtomicU32* a, Uint32 v) { __atomic_store_n(&a->value, v, __ATOMIC_SEQ_CST); }
Uint32 sdl3c_GetAtomicU32(SDL_AtomicU32* a) { return __atomic_load_n(&a->value, __ATOMIC_SEQ_CST); }

/* ================= audio =================
   One SDL2 device (S16, stereo, 48 kHz) whose callback mixes every bound stream. A stream is an
   SDL2 SDL_AudioStream (the rate conversion) plus SDL3's get-callback, asked for what the device
   needs before the stream is read, and gain. */

struct SDL_AudioStream {
    SDL2_AudioStream* cvt;
    SDL_mutex* lock;   /* recursive: the get-callback puts data while it is held */
    SDL_AudioStreamCallback get;
    void* get_ud;
    float gain;
    int src_rate, dst_rate, src_frame, dst_frame;
    bool bound;
    struct SDL_AudioStream* next;
    SDL_threadID owner;   /* the recursive lock below */
    int depth;
};

/* s->lock, recursive whatever SDL2's mutexes are on this platform: the device callback holds it
   while the stream's get-callback puts data (SDL3's contract) */
static void slock(SDL_AudioStream* s)
{
    SDL_threadID me = SDL_ThreadID();
    SDL_mutex* m = s->lock;
    if (__atomic_load_n(&s->owner, __ATOMIC_ACQUIRE) == me) { s->depth++; return; }
    SDL_LockMutex(m);
    __atomic_store_n(&s->owner, me, __ATOMIC_RELEASE);
    s->depth = 1;
}
static void sunlock(SDL_AudioStream* s)
{
    SDL_mutex* m = s->lock;
    if (--s->depth) return;
    __atomic_store_n(&s->owner, (SDL_threadID)0, __ATOMIC_RELEASE);
    SDL_UnlockMutex(m);
}

static SDL_AudioDeviceID out_dev;
static SDL_AudioSpec out_spec;
static SDL_AudioStream* bound_list;
static Sint16* mixbuf;
static Sint32* accum;
static int mixcap;

static void SDLCALL device_cb(void* ud, Uint8* out, int len)
{
    int samples = len / 2;
    (void)ud;
    if (len > mixcap) {   /* the first call (SDL2 fixes the buffer size at open) */
        free(mixbuf); free(accum);
        mixbuf = (Sint16*)malloc((size_t)len); accum = (Sint32*)malloc((size_t)samples * sizeof(Sint32));
        mixcap = mixbuf && accum ? len : 0;
        if (!mixcap) { memset(out, 0, (size_t)len); return; }
    }
    memset(accum, 0, (size_t)samples * sizeof(Sint32));
    for (SDL_AudioStream* s = bound_list; s; s = s->next) {
        int got, avail;
        slock(s);
        avail = SDL_AudioStreamAvailable(s->cvt);
        if (s->get && avail < len) {
            int need = (int)(((Sint64)(len - avail) / s->dst_frame * s->src_rate + s->dst_rate - 1) / s->dst_rate) * s->src_frame;
            s->get(s->get_ud, s, need, need);
        }
        got = SDL_AudioStreamGet(s->cvt, mixbuf, len);
        if (got > 0) {
            int g = (int)(s->gain * 65536.0f + 0.5f);
            for (int i = 0; i < got / 2; i++) accum[i] += (Sint32)(((Sint64)mixbuf[i] * g) >> 16);
        }
        sunlock(s);
    }
    for (int i = 0; i < samples; i++) ((Sint16*)out)[i] = (Sint16)(accum[i] > 32767 ? 32767 : accum[i] < -32768 ? -32768 : accum[i]);
    {   /* the log: that the device runs and gets sound (once, after ~10 s) */
        static int calls, loud, streams;
        int nst = 0;
        for (SDL_AudioStream* s = bound_list; s; s = s->next) nst++;
        if (nst > streams) streams = nst;
        for (int i = 0; i < samples && !loud; i++) if (accum[i] > 64 || accum[i] < -64) loud = 1;
        if (++calls == 480) fprintf(stderr, "audio: device ran %d callbacks of %d bytes, up to %d streams, %s\n", calls, len, streams, loud ? "with sound" : "SILENT");
    }
}

SDL_AudioDeviceID sdl3c_OpenAudioDevice(SDL_AudioDeviceID dev, const SDL_AudioSpec* spec)
{
    SDL2_AudioSpec want, have;
    (void)dev;
    if (out_dev) return out_dev;
    SDL_zero(want);
    want.freq = spec ? spec->freq : 48000;
    want.format = spec ? spec->format : AUDIO_S16SYS;
    want.channels = (Uint8)(spec ? spec->channels : 2);
    want.samples = 1024;
    want.callback = device_cb;
    if (!(out_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0))) {
        fprintf(stderr, "audio: SDL_OpenAudioDevice (driver %s): %s\n", SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "none", SDL_GetError());
        return 0;
    }
    out_spec.format = have.format; out_spec.channels = have.channels; out_spec.freq = have.freq;
    fprintf(stderr, "audio: driver %s, device %u: %d Hz, %d channels, format 0x%x, %d samples\n",
            SDL_GetCurrentAudioDriver(), (unsigned)out_dev, have.freq, have.channels, have.format, have.samples);
    SDL_PauseAudioDevice(out_dev, 0);
    return out_dev;
}

static int frame_bytes(const SDL_AudioSpec* s) { return SDL_AUDIO_BITSIZE(s->format) / 8 * s->channels; }

SDL_AudioStream* sdl3c_CreateAudioStream(const SDL_AudioSpec* src, const SDL_AudioSpec* dst)
{
    SDL_AudioStream* s = (SDL_AudioStream*)SDL_calloc(1, sizeof(*s));
    const SDL_AudioSpec* d = dst ? dst : &out_spec;
    if (!s) return NULL;
    s->cvt = SDL_NewAudioStream(src->format, (Uint8)src->channels, src->freq, d->format, (Uint8)d->channels, d->freq);
    s->lock = SDL_CreateMutex();
    if (!s->cvt || !s->lock) { if (s->cvt) SDL_FreeAudioStream(s->cvt); if (s->lock) SDL_DestroyMutex(s->lock); SDL_free(s); return NULL; }
    s->gain = 1.0f;
    s->src_rate = src->freq; s->dst_rate = d->freq;
    s->src_frame = frame_bytes(src); s->dst_frame = frame_bytes(d);
    return s;
}

void sdl3c_UnbindAudioStream(SDL_AudioStream* s)
{
    if (!s || !out_dev) return;
    SDL_LockAudioDevice(out_dev);
    for (SDL_AudioStream** p = &bound_list; *p; p = &(*p)->next)
        if (*p == s) { *p = s->next; break; }
    s->bound = false; s->next = NULL;
    SDL_UnlockAudioDevice(out_dev);
}

bool sdl3c_BindAudioStream(SDL_AudioDeviceID dev, SDL_AudioStream* s)
{
    if (!s || !dev || dev != out_dev) return SDL_SetError("no such audio device"), false;
    SDL_LockAudioDevice(out_dev);
    if (!s->bound) { s->next = bound_list; bound_list = s; s->bound = true; }
    SDL_UnlockAudioDevice(out_dev);
    return true;
}

void sdl3c_DestroyAudioStream(SDL_AudioStream* s)
{
    if (!s) return;
    sdl3c_UnbindAudioStream(s);
    SDL_FreeAudioStream(s->cvt);
    SDL_DestroyMutex(s->lock);
    SDL_free(s);
}

bool sdl3c_PutAudioStreamData(SDL_AudioStream* s, const void* buf, int len)
{
    int r;
    if (!s) return false;
    slock(s);
    r = SDL_AudioStreamPut(s->cvt, buf, len);
    sunlock(s);
    return r == 0;
}

/* SDL3 counts the input not yet played, in the input format: the converted data scaled back */
int sdl3c_GetAudioStreamQueued(SDL_AudioStream* s)
{
    int n;
    if (!s) return -1;
    slock(s);
    n = SDL_AudioStreamAvailable(s->cvt);
    sunlock(s);
    return (int)((Sint64)n / s->dst_frame * s->src_rate / s->dst_rate) * s->src_frame;
}

int sdl3c_GetAudioStreamAvailable(SDL_AudioStream* s)
{
    int n;
    if (!s) return -1;
    slock(s);
    n = SDL_AudioStreamAvailable(s->cvt);
    sunlock(s);
    return n;
}

bool sdl3c_FlushAudioStream(SDL_AudioStream* s)
{
    int r;
    if (!s) return false;
    slock(s);
    r = SDL_AudioStreamFlush(s->cvt);
    sunlock(s);
    return r == 0;
}

bool sdl3c_SetAudioStreamGain(SDL_AudioStream* s, float gain) { if (!s) return false; slock(s); s->gain = gain < 0 ? 0 : gain; sunlock(s); return true; }
bool sdl3c_SetAudioStreamGetCallback(SDL_AudioStream* s, SDL_AudioStreamCallback cb, void* ud) { if (!s) return false; slock(s); s->get = cb; s->get_ud = ud; sunlock(s); return true; }
bool sdl3c_LockAudioStream(SDL_AudioStream* s) { if (!s) return false; slock(s); return true; }
bool sdl3c_UnlockAudioStream(SDL_AudioStream* s) { if (!s) return false; sunlock(s); return true; }

/* ================= gamepads ================= */

/* SDL3 ids are SDL2 instance ids + 1: SDL3 reserves 0 for "none", and SDL2 on the Switch
   gives player 1 instance id 0 */
static int device_index(SDL_JoystickID id)
{
    for (int i = 0, n = SDL_NumJoysticks(); i < n; i++)
        if (SDL_JoystickGetDeviceInstanceID(i) + 1 == id) return i;
    return -1;
}

/* Switch: a joystick SDL2 has no game controller mapping for (the console's own controllers in
   some SDL2 builds) gets one, in libnx's button order (A B X Y, sticks, L R ZL ZR, + -, d-pad
   left up right down); positional, so SDL's "a" (south) is the B button. SDL2 then sends
   SDL_CONTROLLERDEVICEADDED for it. */
static void ensure_mappings(void)
{
#ifdef __SWITCH__
    for (int i = 0, n = SDL_NumJoysticks(); i < n; i++) {
        char guid[64], map[512];
        if (SDL_IsGameController(i)) continue;
        SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(i), guid, sizeof(guid));
        snprintf(map, sizeof(map), "%s,%s,a:b1,b:b0,x:b3,y:b2,leftstick:b4,rightstick:b5,leftshoulder:b6,rightshoulder:b7,"
                 "lefttrigger:b8,righttrigger:b9,start:b10,back:b11,dpleft:b12,dpup:b13,dpright:b14,dpdown:b15,"
                 "leftx:a0,lefty:a1,rightx:a2,righty:a3,platform:Switch,",
                 guid, SDL_JoystickNameForIndex(i) ? SDL_JoystickNameForIndex(i) : "Switch Controller");
        fprintf(stderr, "pad: joystick %d '%s' had no mapping: %s\n", i, SDL_JoystickNameForIndex(i) ? SDL_JoystickNameForIndex(i) : "?",
                SDL_GameControllerAddMapping(map) >= 0 ? "added" : SDL_GetError());
    }
#endif
}

SDL_JoystickID* sdl3c_GetGamepads(int* count)
{
    int n, k = 0;
    ensure_mappings();
    n = SDL_NumJoysticks();
    SDL_JoystickID* ids = (SDL_JoystickID*)SDL_malloc(sizeof(SDL_JoystickID) * (size_t)(n > 0 ? n + 1 : 1));
    if (!ids) { if (count) *count = 0; return NULL; }
    for (int i = 0; i < n; i++) if (SDL_IsGameController(i)) ids[k++] = SDL_JoystickGetDeviceInstanceID(i) + 1;
    ids[k] = 0;
    if (count) *count = k;
    return ids;
}

SDL_Gamepad* sdl3c_OpenGamepad(SDL_JoystickID id) { int i = device_index(id); return i < 0 ? NULL : SDL_GameControllerOpen(i); }
SDL_Joystick* sdl3c_OpenJoystick(SDL_JoystickID id) { int i = device_index(id); return i < 0 ? NULL : SDL_JoystickOpen(i); }
const char* sdl3c_GetGamepadNameForID(SDL_JoystickID id) { int i = device_index(id); return i < 0 ? NULL : SDL_GameControllerNameForIndex(i); }
int sdl3c_GetGamepadPlayerIndexForID(SDL_JoystickID id) { int i = device_index(id); return i < 0 ? -1 : SDL_JoystickGetDevicePlayerIndex(i); }
SDL_JoystickID sdl3c_AttachVirtualJoystick(const SDL_VirtualJoystickDesc* d) { (void)d; SDL_SetError("no virtual joysticks on this platform"); return 0; }

SDL_GamepadType sdl3c_GetGamepadType(SDL_Gamepad* g)
{
    switch (SDL_GameControllerGetType(g)) {
    case SDL_CONTROLLER_TYPE_XBOX360: return SDL_GAMEPAD_TYPE_XBOX360;
    case SDL_CONTROLLER_TYPE_XBOXONE: return SDL_GAMEPAD_TYPE_XBOXONE;
    case SDL_CONTROLLER_TYPE_PS3: return SDL_GAMEPAD_TYPE_PS3;
    case SDL_CONTROLLER_TYPE_PS4: return SDL_GAMEPAD_TYPE_PS4;
    case SDL_CONTROLLER_TYPE_PS5: return SDL_GAMEPAD_TYPE_PS5;
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT: return SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT;
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT: return SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT;
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR: return SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR;
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO: return SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO;
    default:
#ifdef __SWITCH__
        return SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO;   /* the console's own controllers */
#else
        return SDL_GAMEPAD_TYPE_STANDARD;
#endif
    }
}

/* SDL3's buttons are positions (south is the bottom face button); the label printed there */
SDL_GamepadButtonLabel sdl3c_GetGamepadButtonLabelForType(SDL_GamepadType t, SDL_GamepadButton b)
{
    static const SDL_GamepadButtonLabel xbox[4] = { SDL_GAMEPAD_BUTTON_LABEL_A, SDL_GAMEPAD_BUTTON_LABEL_B, SDL_GAMEPAD_BUTTON_LABEL_X, SDL_GAMEPAD_BUTTON_LABEL_Y };
    static const SDL_GamepadButtonLabel ps[4] = { SDL_GAMEPAD_BUTTON_LABEL_CROSS, SDL_GAMEPAD_BUTTON_LABEL_CIRCLE, SDL_GAMEPAD_BUTTON_LABEL_SQUARE, SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE };
    static const SDL_GamepadButtonLabel nin[4] = { SDL_GAMEPAD_BUTTON_LABEL_B, SDL_GAMEPAD_BUTTON_LABEL_A, SDL_GAMEPAD_BUTTON_LABEL_Y, SDL_GAMEPAD_BUTTON_LABEL_X };
    static const SDL_GamepadButtonLabel gc[4] = { SDL_GAMEPAD_BUTTON_LABEL_A, SDL_GAMEPAD_BUTTON_LABEL_X, SDL_GAMEPAD_BUTTON_LABEL_B, SDL_GAMEPAD_BUTTON_LABEL_Y };
    if (b < SDL_GAMEPAD_BUTTON_SOUTH || b > SDL_GAMEPAD_BUTTON_NORTH) return SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN;
    switch (t) {
    case SDL_GAMEPAD_TYPE_PS3: case SDL_GAMEPAD_TYPE_PS4: case SDL_GAMEPAD_TYPE_PS5: return ps[b];
#ifdef __SWITCH__
    /* SDL2's console driver maps by label (its "A" is the A button, on the right), so the
       button the port calls south has A printed on it */
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO: case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR: (void)nin; return xbox[b];
#else
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO: case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR: return nin[b];
#endif
    case SDL_GAMEPAD_TYPE_GAMECUBE: return gc[b];
    default: return xbox[b];
    }
}

/* the one property the port reads back: a gamepad's rumble */
#define GAMEPAD_PROPS 0x40000000u
SDL_PropertiesID sdl3c_GetGamepadProperties(SDL_Gamepad* g)
{
    return g ? GAMEPAD_PROPS | ((Uint32)SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(g)) & 0xFFFFFFu) : 0;
}

bool sdl3c_GetBooleanProperty(SDL_PropertiesID p, const char* name, bool def)
{
    if ((p & GAMEPAD_PROPS) && name && !strcmp(name, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN)) {
        SDL_GameController* g = SDL_GameControllerFromInstanceID((SDL_JoystickID)(p & 0xFFFFFFu));
        return g && SDL_GameControllerHasRumble(g);
    }
    return def;
}

/* ================= windows, displays, mouse ================= */

static bool exclusive_mode;

SDL_Window* sdl3c_CreateWindow(const char* title, int w, int h, Uint64 flags)
{
#ifdef __SWITCH__
    w = 1280; h = 720;   /* the screen: the system scales it to the TV when docked */
    flags &= ~(Uint64)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
#endif
    return SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, (Uint32)flags);
}

bool sdl3c_SetWindowFullscreen(SDL_Window* w, bool on)
{
#ifdef __SWITCH__
    (void)w; (void)on;   /* always the whole screen */
    return true;
#endif
    return SDL_SetWindowFullscreen(w, on ? (exclusive_mode ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_FULLSCREEN_DESKTOP) : 0) == 0;
}

static void from2(SDL_DisplayMode* d, const SDL2_DisplayMode* s, SDL_DisplayID id)
{
    SDL_zerop(d);
    d->displayID = id; d->format = s->format; d->w = s->w; d->h = s->h;
    d->pixel_density = 1.0f; d->refresh_rate = (float)s->refresh_rate;
    d->refresh_rate_numerator = s->refresh_rate; d->refresh_rate_denominator = 1;
}

bool sdl3c_SetWindowFullscreenMode(SDL_Window* w, const SDL_DisplayMode* m)
{
    SDL2_DisplayMode s;
    exclusive_mode = m != NULL;
#ifdef __SWITCH__
    (void)w;   /* one mode: the screen's */
    return true;
#endif
    if (!m) return true;
    SDL_zero(s);
    s.format = m->format; s.w = m->w; s.h = m->h; s.refresh_rate = (int)(m->refresh_rate + 0.5f);
    return SDL_SetWindowDisplayMode(w, &s) == 0;
}

SDL_DisplayID* sdl3c_GetDisplays(int* count)
{
    int n = SDL_GetNumVideoDisplays();
    SDL_DisplayID* ids = (SDL_DisplayID*)SDL_malloc(sizeof(SDL_DisplayID) * (size_t)(n > 0 ? n + 1 : 1));
    if (n < 0) n = 0;
    if (!ids) { if (count) *count = 0; return NULL; }
    for (int i = 0; i < n; i++) ids[i] = (SDL_DisplayID)(i + 1);
    ids[n] = 0;
    if (count) *count = n;
    return ids;
}

#define MAXD 8
static SDL_DisplayMode desk_modes[MAXD], cur_modes[MAXD];

const SDL_DisplayMode* sdl3c_GetDesktopDisplayMode(SDL_DisplayID d)
{
    SDL2_DisplayMode s;
    if (d < 1 || d > MAXD || SDL_GetDesktopDisplayMode((int)d - 1, &s)) return NULL;
    from2(&desk_modes[d - 1], &s, d);
    return &desk_modes[d - 1];
}

const SDL_DisplayMode* sdl3c_GetCurrentDisplayMode(SDL_DisplayID d)
{
    SDL2_DisplayMode s;
    if (d < 1 || d > MAXD || SDL_GetCurrentDisplayMode((int)d - 1, &s)) return NULL;
    from2(&cur_modes[d - 1], &s, d);
    return &cur_modes[d - 1];
}

SDL_DisplayMode** sdl3c_GetFullscreenDisplayModes(SDL_DisplayID d, int* count)
{
    int n = d >= 1 ? SDL_GetNumDisplayModes((int)d - 1) : -1;
    SDL_DisplayMode** list;
    SDL_DisplayMode* modes;
    if (n < 0) n = 0;
    list = (SDL_DisplayMode**)SDL_malloc((size_t)(n + 1) * sizeof(*list) + (size_t)n * sizeof(**list));   /* one SDL_free */
    if (!list) { if (count) *count = 0; return NULL; }
    modes = (SDL_DisplayMode*)(list + n + 1);
    for (int i = 0; i < n; i++) {
        SDL2_DisplayMode s;
        SDL_zero(s);
        SDL_GetDisplayMode((int)d - 1, i, &s);
        from2(&modes[i], &s, d);
        list[i] = &modes[i];
    }
    list[n] = NULL;
    if (count) *count = n;
    return list;
}

bool sdl3c_GetClosestFullscreenDisplayMode(SDL_DisplayID d, int w, int h, float hz, bool hd, SDL_DisplayMode* out)
{
    SDL2_DisplayMode want, got;
    (void)hd;
    SDL_zero(want);
    want.w = w; want.h = h; want.refresh_rate = (int)(hz + 0.5f);
    if (d < 1 || !SDL_GetClosestDisplayMode((int)d - 1, &want, &got)) return false;
    if (out) from2(out, &got, d);
    return true;
}

SDL_MouseButtonFlags sdl3c_GetMouseState(float* x, float* y)
{
    int ix, iy;
    Uint32 b = SDL_GetMouseState(&ix, &iy);
    if (x) *x = (float)ix;
    if (y) *y = (float)iy;
    return b;
}

/* ================= surfaces ================= */

bool sdl3c_FlipSurface(SDL_Surface* s, int flip)
{
    Uint8* tmp;
    if (!s || flip != SDL_FLIP_VERTICAL) return SDL_SetError("only vertical flips"), false;
    if (!(tmp = (Uint8*)malloc((size_t)s->pitch))) return false;
    SDL_LockSurface(s);
    for (int y = 0; y < s->h / 2; y++) {
        Uint8* a = (Uint8*)s->pixels + (size_t)y * s->pitch;
        Uint8* b = (Uint8*)s->pixels + (size_t)(s->h - 1 - y) * s->pitch;
        memcpy(tmp, a, (size_t)s->pitch); memcpy(a, b, (size_t)s->pitch); memcpy(b, tmp, (size_t)s->pitch);
    }
    SDL_UnlockSurface(s);
    free(tmp);
    return true;
}

SDL_Surface* sdl3c_LoadPNG(const char* path) { return IMG_Load(path); }
bool sdl3c_SavePNG(SDL_Surface* s, const char* path) { return IMG_SavePNG(s, path) == 0; }

/* ================= filesystem ================= */

const char* sdl3c_GetBasePath(void)
{
#ifdef __SWITCH__
    return HOME_DIR;   /* the NRO's folder: recvx.ini, save/, mods/, the disc image */
#else
    static char* base;
    if (!base) base = SDL_GetBasePath();
    return base;
#endif
}

char* sdl3c_GetCurrentDirectory(void)
{
    char buf[1024];
    size_t n;
    char* r;
    if (!getcwd(buf, sizeof(buf) - 1)) return SDL_SetError("getcwd: %s", strerror(errno)), (char*)NULL;
    n = strlen(buf);
    if (!n || buf[n - 1] != '/') buf[n++] = '/', buf[n] = 0;
    if ((r = (char*)SDL_malloc(n + 1))) memcpy(r, buf, n + 1);
    return r;
}

bool sdl3c_GetPathInfo(const char* path, SDL_PathInfo* info)
{
    struct stat st;
    if (info) SDL_zerop(info);
    if (!path || stat(path, &st)) return SDL_SetError("%s: %s", path ? path : "(null)", strerror(errno)), false;
    if (info) {
        info->type = S_ISREG(st.st_mode) ? SDL_PATHTYPE_FILE : S_ISDIR(st.st_mode) ? SDL_PATHTYPE_DIRECTORY : SDL_PATHTYPE_OTHER;
        info->size = (Uint64)st.st_size;
        info->modify_time = (SDL_Time)st.st_mtime * 1000000000;
        info->create_time = (SDL_Time)st.st_ctime * 1000000000;
        info->access_time = (SDL_Time)st.st_atime * 1000000000;
    }
    return true;
}

/* SDL3 makes the missing parents too */
bool sdl3c_CreateDirectory(const char* path)
{
    char buf[1024];
    size_t n = strlen(path);
    struct stat st;
    if (n >= sizeof(buf)) return SDL_SetError("path too long"), false;
    memcpy(buf, path, n + 1);
    for (char* p = buf + 1; *p; p++) {
        if (*p != '/' || p[-1] == ':') continue;   /* sdmc:/ */
        *p = 0;
        if (stat(buf, &st) && mkdir(buf, 0777) && errno != EEXIST) return SDL_SetError("mkdir %s: %s", buf, strerror(errno)), false;
        *p = '/';
    }
    if (!stat(buf, &st)) return S_ISDIR(st.st_mode) ? true : (SDL_SetError("%s is a file", buf), false);
    return mkdir(buf, 0777) == 0 || errno == EEXIST ? true : (SDL_SetError("mkdir %s: %s", buf, strerror(errno)), false);
}

bool sdl3c_RemovePath(const char* path)
{
    struct stat st;
    if (stat(path, &st)) return errno == ENOENT ? true : (SDL_SetError("%s: %s", path, strerror(errno)), false);
    if ((S_ISDIR(st.st_mode) ? rmdir(path) : remove(path)) == 0) return true;
    return SDL_SetError("remove %s: %s", path, strerror(errno)), false;
}

bool sdl3c_EnumerateDirectory(const char* path, SDL_EnumerateDirectoryCallback cb, void* ud)
{
    char dir[1024];
    size_t n = strlen(path);
    DIR* d;
    struct dirent* e;
    SDL_EnumerationResult r = SDL_ENUM_CONTINUE;
    if (n + 2 > sizeof(dir)) return SDL_SetError("path too long"), false;
    memcpy(dir, path, n + 1);
    if (n && dir[n - 1] != '/') dir[n++] = '/', dir[n] = 0;   /* SDL3 hands the folder with its separator */
    if (!(d = opendir(path))) return SDL_SetError("opendir %s: %s", path, strerror(errno)), false;
    while (r == SDL_ENUM_CONTINUE && (e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        r = cb(ud, dir, e->d_name);
    }
    closedir(d);
    return r != SDL_ENUM_FAILURE;
}

static bool glob_match(const char* p, const char* s, bool icase)
{
    for (; *p; p++, s++) {
        if (*p == '*') {
            while (p[1] == '*') p++;
            if (!p[1]) return true;
            for (; *s; s++) if (glob_match(p + 1, s, icase)) return true;
            return false;
        }
        if (!*s) return false;
        if (*p != '?' && (icase ? SDL_tolower((unsigned char)*p) != SDL_tolower((unsigned char)*s) : *p != *s)) return false;
    }
    return !*s;
}

char** sdl3c_GlobDirectory(const char* path, const char* pattern, SDL_GlobFlags flags, int* count)
{
    DIR* d = opendir(path);
    struct dirent* e;
    size_t cap = 16, n = 0, bytes = 0;
    char** names;
    char** out;
    char* str;
    if (count) *count = 0;
    if (!d) return SDL_SetError("opendir %s: %s", path, strerror(errno)), (char**)NULL;
    if (!(names = (char**)malloc(cap * sizeof(*names)))) { closedir(d); return NULL; }
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        if (pattern && !glob_match(pattern, e->d_name, (flags & SDL_GLOB_CASEINSENSITIVE) != 0)) continue;
        if (n == cap) { char** t = (char**)realloc(names, (cap *= 2) * sizeof(*names)); if (!t) break; names = t; }
        if (!(names[n] = strdup(e->d_name))) break;
        bytes += strlen(names[n++]) + 1;
    }
    closedir(d);
    out = (char**)SDL_malloc((n + 1) * sizeof(*out) + bytes);   /* one SDL_free, as SDL3's */
    if (out) {
        str = (char*)(out + n + 1);
        for (size_t i = 0; i < n; i++) { size_t l = strlen(names[i]) + 1; memcpy(str, names[i], l); out[i] = str; str += l; }
        out[n] = NULL;
        if (count) *count = (int)n;
    }
    for (size_t i = 0; i < n; i++) free(names[i]);
    free(names);
    return out;
}

/* No file picker: the first disc image in the current folder is the choice (none: cancelled). */
void sdl3c_ShowFileDialogWithProperties(SDL_FileDialogType type, SDL_DialogFileCallback cb, void* ud, SDL_PropertiesID props)
{
    static char pick[1024];
    const char* list[2] = { NULL, NULL };
    int n = 0;
    char** isos;
    (void)props;
    if (type == SDL_FILEDIALOG_OPENFILE && (isos = sdl3c_GlobDirectory(HOME_DIR, "*.iso", SDL_GLOB_CASEINSENSITIVE, &n))) {
        if (n) { snprintf(pick, sizeof(pick), HOME_DIR "%s", isos[0]); list[0] = pick; }
        SDL_free(isos);
    }
    fprintf(stderr, "switch: disc image in " HOME_DIR ": %s\n", list[0] ? list[0] : "none (*.iso)");
    cb(ud, list, -1);
}
