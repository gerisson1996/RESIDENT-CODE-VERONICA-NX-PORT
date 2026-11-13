// Switch: the SDL2 side of switch/imgui/imgui_impl_sdl3.h (no SDL3 header here).
#include <glad/glad.h>
#include <SDL.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"

extern "C" {
bool sdl3c_imgui_init(SDL_Window* window, void* gl_context)
{
    // imgui_impl_opengl3.cpp calls GL through glad here (no dlopen loader on the console)
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) return false;
    return ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
}
void sdl3c_imgui_shutdown(void) { ImGui_ImplSDL2_Shutdown(); }
void sdl3c_imgui_new_frame(void) { ImGui_ImplSDL2_NewFrame(); }
bool sdl3c_imgui_event(const void* e) { return ImGui_ImplSDL2_ProcessEvent(static_cast<const SDL_Event*>(e)); }
}
