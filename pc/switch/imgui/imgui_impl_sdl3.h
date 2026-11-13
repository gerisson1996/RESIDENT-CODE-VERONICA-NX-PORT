// Switch: Dear ImGui's SDL3 platform backend as the overlay calls it, over the SDL2 one
// (switch/imgui_sdl2_shim.cpp, compiled against SDL2 itself).
#pragma once
#include "imgui.h"
#include <SDL3/SDL.h>

extern "C" {
bool sdl3c_imgui_init(SDL_Window* window, void* gl_context);
void sdl3c_imgui_shutdown(void);
void sdl3c_imgui_new_frame(void);
bool sdl3c_imgui_event(const void* sdl2_event);
}

static inline bool ImGui_ImplSDL3_InitForOpenGL(SDL_Window* window, void* gl_context) { return sdl3c_imgui_init(window, gl_context); }
static inline void ImGui_ImplSDL3_Shutdown() { sdl3c_imgui_shutdown(); }
static inline void ImGui_ImplSDL3_NewFrame() { sdl3c_imgui_new_frame(); }
// a translated event carries its SDL2 original; one the port pushed (type 0 there) has none
static inline bool ImGui_ImplSDL3_ProcessEvent(const SDL_Event* e) { return e->sdl2.ev.type ? sdl3c_imgui_event(&e->sdl2.ev) : false; }
