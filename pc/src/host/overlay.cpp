/*
 * F1 overlay (Dear ImGui): display, audio and port settings. While it is open the game
 * keeps running but receives no pad input. Settings are applied immediately and saved
 * to <save_dir>/recvx.ini.
 */
#include "../ps2_platform.h"
extern "C" {
#include "../audio/audio.h"
}
#include "prof.h"
#include "cheat.h"
#include "actors.h"
#include "../gs/stream.h"
#include "../gs/materials.h"
extern "C" void gs_window_to_screen(float px, float py, float* x, float* y);
#include "tps.h"
#include "menu_mouse.h"
#include "bindings.h"
#include "settings.h"
#include "update.h"
#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <ctype.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

extern "C" void pad_set_game_input_enabled(int on);

static SDL_Window* win;
static bool ready, visible, cheats_visible;
static bool quick_visible;   /* Switch: the L3 + R3 quick menu */
static bool tps_key_wait;   /* the next key pressed becomes the third-person toggle key */

void overlay_init(void* window, void* gl_context)
{
    win = (SDL_Window*)window;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;   /* our settings live in recvx.ini */
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForOpenGL(win, (SDL_GLContext)gl_context);
    ImGui_ImplOpenGL3_Init("#version 330 core");
    ready = true;
    const char* show = SDL_getenv("RECVX_SHOW_OVERLAY");   /* debugging aid (screenshots): 1 F1, 2 the cheats (F2) */
    visible = show && strcmp(show, "2");
    cheats_visible = show && !strcmp(show, "2");
#ifdef __SWITCH__
    quick_visible = show && !strcmp(show, "3");   /* 3: the quick menu (L3 + R3) */
    if (quick_visible) visible = false;
#endif
#ifdef __SWITCH__
    /* no keyboard: the menus are driven by the pad (A select, B back, D-pad/stick move, L/R
       change tab) or the touch screen, in a larger font for the handheld screen */
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    ImGui::GetStyle().FontScaleMain = 1.5f;
#endif
}

#ifdef __SWITCH__
static char sw_help[512];   /* the focused row's help, shown in the footer */
static int sw_tab_step;     /* L / R pressed: the previous / next tab */
static bool sw_open;        /* the menu was just opened: focus it */

/* - and + held together: F1 (settings); with ZR also held: F2 (cheats). L / R change the tab */
static int switch_combo(const SDL_Event* ev)
{
    static bool minus, plus;
    if (ev->type != SDL_EVENT_GAMEPAD_BUTTON_DOWN && ev->type != SDL_EVENT_GAMEPAD_BUTTON_UP) return 0;
    bool down = ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    if (visible && !cheats_visible && down && (ev->gbutton.button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER || ev->gbutton.button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) {
        sw_tab_step = ev->gbutton.button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER ? -1 : 1;
        return 1;
    }
    if (ev->gbutton.button == SDL_GAMEPAD_BUTTON_BACK) minus = down;
    else if (ev->gbutton.button == SDL_GAMEPAD_BUTTON_START) plus = down;
    else return 0;
    if (!down || !minus || !plus) return 0;
    SDL_Gamepad* g = SDL_GetGamepadFromID(ev->gbutton.which);
    if (g && SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 16000) cheats_visible = !cheats_visible;
    else sw_open = visible = !visible;
    return 1;
}

/* ---- L3 + R3: the quick menu, the settings that matter most on the Switch on one page ----
   D-pad / left stick up and down choose, left and right (or A) change, B or L3 + R3 close */
static int quick_sel = -1;
enum { Q_HEAD, Q_SET, Q_ALL, Q_CLOSE };
struct QuickRow { int kind; const void* var; const char* text; };
static const QuickRow quick_rows[] = {
    { Q_HEAD, nullptr, "CAMERA AND AIM" },
    { Q_SET, (const void*)&g_tps.on }, { Q_SET, &g_tps.pad_gyro }, { Q_SET, &g_tps.pad_gyro_sens },
    { Q_SET, &g_tps.pad_gyro_invert_x }, { Q_SET, &g_tps.pad_gyro_invert_y },
    { Q_SET, &g_tps.pad_sens_x }, { Q_SET, &g_tps.pad_aim_sens }, { Q_SET, &g_tps.auto_aim }, { Q_SET, &g_tps.crosshair },
    { Q_HEAD, nullptr, "PICTURE" },
    { Q_SET, &g_config.scale }, { Q_SET, &g_config.interpolate }, { Q_SET, &g_config.aspect },
    { Q_SET, &g_config.brightness }, { Q_SET, &g_config.show_fps },
    { Q_HEAD, nullptr, "SOUND" },
    { Q_SET, &g_config.music_volume }, { Q_SET, &g_config.sfx_volume },
    { Q_HEAD, nullptr, "GAME" },
    { Q_SET, &g_config.skip_doors }, { Q_SET, &g_config.fast_loading },
    { Q_HEAD, nullptr, "" },
    { Q_ALL, nullptr, "All settings..." }, { Q_CLOSE, nullptr, "Back to the game" },
};
enum { QUICK_N = (int)(sizeof quick_rows / sizeof *quick_rows) };

static const Setting* quick_setting(const QuickRow* r)
{
    if (r->kind != Q_SET) return nullptr;
    for (int i = 0; i < g_settings_n; i++)
        if (g_settings[i].val == (const int*)r->var) return (g_settings[i].shown && !g_settings[i].shown()) ? nullptr : &g_settings[i];
    return nullptr;
}
static bool quick_selectable(int i) { return quick_rows[i].kind == Q_ALL || quick_rows[i].kind == Q_CLOSE || quick_setting(&quick_rows[i]); }

static int quick_combo(const SDL_Event* ev)
{
    static bool l3, r3;
    if (ev->type != SDL_EVENT_GAMEPAD_BUTTON_DOWN && ev->type != SDL_EVENT_GAMEPAD_BUTTON_UP) return 0;
    bool down = ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    if (ev->gbutton.button == SDL_GAMEPAD_BUTTON_LEFT_STICK) l3 = down;
    else if (ev->gbutton.button == SDL_GAMEPAD_BUTTON_RIGHT_STICK) r3 = down;
    else return 0;
    if (!down || !l3 || !r3) return 0;
    quick_visible = !quick_visible;
    if (quick_visible) visible = cheats_visible = false;
    return 1;
}

/* the cheats tab (L / R switch tabs): the F2 menu's essentials */
static int quick_tab, quick_csel;
enum { QC_HEALTH, QC_HEAL, QC_AMMO, QC_ITEM, QC_COUNT, QC_GIVE, QC_N };
static const char* const quick_cheat_help[QC_N] = {
    "Your health stays full.",
    "Fills your health once.",
    "The equipped weapon never runs out.",
    "The item to add: left and right go through the list (hold to go fast).",
    "How many to add: left and right change it by 1, A adds 10.",
    "Puts the item in your inventory (needs a free slot).",
};
static int quick_next_item(int id, int dir)
{
    for (int n = 0; n < CHEAT_ITEMS; n++) {
        id += dir;
        if (id < 1) id = CHEAT_ITEMS - 1;
        if (id >= CHEAT_ITEMS) id = 1;
        if (g_cheat.names[id][0]) return id;
    }
    return 0;   /* (names not loaded yet) */
}

static void quick_window(void)
{
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 ds = io.DisplaySize;
    const ImU32 accent = IM_COL32(200, 30, 30, 255), accent_dim = IM_COL32(200, 30, 30, 90);
    const ImVec4 head_col(0.86f, 0.25f, 0.22f, 1.0f), dim(0.62f, 0.62f, 0.66f, 1.0f), on_col(0.45f, 0.9f, 0.5f, 1.0f);
    bool changed = false;
    char val[96];

    if (quick_sel < 0 || !quick_selectable(quick_sel)) { quick_sel = 0; while (quick_sel < QUICK_N - 1 && !quick_selectable(quick_sel)) quick_sel++; }
    /* input: held directions repeat */
    int move = 0, adj = 0;
    if (ImGui::IsKeyPressed(ImGuiKey_GamepadDpadUp, true) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickUp, true)) move = -1;
    if (ImGui::IsKeyPressed(ImGuiKey_GamepadDpadDown, true) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickDown, true)) move = 1;
    if (ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft, true) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickLeft, true)) adj = -1;
    if (ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight, true) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickRight, true)) adj = 1;
    /* the Switch's A is on the right (SDL2's controller B, as ImGui reads it), B at the bottom */
    bool press = ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false);   /* A */
    bool back = ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false);     /* B */
    if (ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadR1, false)) quick_tab ^= 1;
    CheatState* c = &g_cheat;
    if (c->give_count < 1) c->give_count = 1;
    if (quick_tab == 1) {
        if (move) quick_csel = (quick_csel + move + QC_N) % QC_N;
        switch (quick_csel) {
        case QC_HEALTH: if (adj || press) c->inf_health = !c->inf_health; break;
        case QC_AMMO: if (adj || press) c->inf_ammo = !c->inf_ammo; break;
        case QC_HEAL: if (press) __atomic_or_fetch(&c->request, CHEAT_HEAL, __ATOMIC_ACQ_REL); break;
        case QC_ITEM:
            if (adj || press) c->give_id = quick_next_item(c->give_id, adj ? adj : 1);
            else if (!c->give_id || !c->names[c->give_id][0]) c->give_id = quick_next_item(0, 1);
            break;
        case QC_COUNT: c->give_count = SDL_clamp(c->give_count + adj + (press ? 10 : 0), 1, 999); break;
        case QC_GIVE: if (press && c->give_id) __atomic_or_fetch(&c->request, CHEAT_GIVE, __ATOMIC_ACQ_REL); break;
        }
    } else if (move) {
        int i = quick_sel;
        do i = (i + move + QUICK_N) % QUICK_N; while (!quick_selectable(i));
        quick_sel = i;
    }
    if (quick_tab == 0) {
        const QuickRow* r = &quick_rows[quick_sel];
        const Setting* s = quick_setting(r);
        if (s && !settings_locked(s) && (adj || press)) {
            if (settings_is_bool(s)) settings_set(s, !settings_get(s));
            else settings_step(s, adj ? adj : 1);
            changed = true;
        } else if (press && r->kind == Q_ALL) { quick_visible = false; sw_open = visible = true; }
        else if (press && r->kind == Q_CLOSE) quick_visible = false;
    }
    if (back) quick_visible = false;

    /* the panel: a dark card in the middle, a red bar on top */
    float w = SDL_min(ds.x * 0.62f, ImGui::GetFontSize() * 34.0f), h = ds.y * 0.88f;
    ImGui::SetNextWindowPos(ImVec2(ds.x * 0.5f, ds.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ImGui::GetFontSize() * 1.2f, ImGui::GetFontSize() * 0.9f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.07f, 0.94f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.78f, 0.12f, 0.12f, 0.55f));
    ImGui::Begin("##quick", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav |
                                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    dl->AddRectFilled(wp, ImVec2(wp.x + w, wp.y + 6.0f), accent, 14.0f, ImDrawFlags_RoundCornersTop);

    ImGui::SetWindowFontScale(1.25f);
    ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.97f, 1.0f), "QUICK MENU");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetStyle().WindowPadding.x - ImGui::CalcTextSize("000 fps").x);
    ImGui::TextColored(dim, "%3.0f fps", io.Framerate);
    ImGui::TextColored(dim, "Code: Veronica X  " RECVX_VERSION);
    ImGui::Dummy(ImVec2(0, ImGui::GetFontSize() * 0.3f));
    {   /* the tabs: (L) SETTINGS  CHEATS (R) */
        static const char* const tabs[2] = { "SETTINGS", "CHEATS" };
        ImVec2 p = ImGui::GetCursorScreenPos();
        float x = p.x, th = ImGui::GetTextLineHeight(), gap = ImGui::GetFontSize() * 1.4f;
        dl->AddText(ImVec2(x, p.y), ImGui::GetColorU32(dim), "(L)");
        x += ImGui::CalcTextSize("(L)").x + gap * 0.6f;
        for (int t = 0; t < 2; t++) {
            float tw = ImGui::CalcTextSize(tabs[t]).x;
            dl->AddText(ImVec2(x, p.y), ImGui::GetColorU32(t == quick_tab ? ImVec4(1, 1, 1, 1) : dim), tabs[t]);
            if (t == quick_tab) dl->AddRectFilled(ImVec2(x, p.y + th + 3.0f), ImVec2(x + tw, p.y + th + 6.0f), accent, 1.5f);
            x += tw + gap;
        }
        dl->AddText(ImVec2(x - gap * 0.4f, p.y), ImGui::GetColorU32(dim), "(R)");
        ImGui::Dummy(ImVec2(0, th + ImGui::GetFontSize() * 0.6f));
    }

    float foot = ImGui::GetTextLineHeightWithSpacing() * 4.2f;
    ImGui::BeginChild("##qrows", ImVec2(0, -foot), ImGuiChildFlags_None, ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollbar);
    float rw = ImGui::GetContentRegionAvail().x, lh = ImGui::GetTextLineHeight() * 1.55f;
    const char* help = nullptr;
    if (quick_tab == 1) {   /* the cheats */
        float pad = ImGui::GetFontSize() * 0.7f;
        auto row = [&](int i, const char* label, const char* value, ImVec4 vc) {
            ImDrawList* cl = ImGui::GetWindowDrawList();
            bool sel = i == quick_csel;
            ImVec2 p = ImGui::GetCursorScreenPos();
            if (sel) {
                cl->AddRectFilled(p, ImVec2(p.x + rw, p.y + lh), IM_COL32(200, 30, 30, 70), 6.0f);
                cl->AddRectFilled(p, ImVec2(p.x + 4.0f, p.y + lh), accent, 2.0f);
                help = quick_cheat_help[i];
            }
            float ty = p.y + (lh - ImGui::GetTextLineHeight()) * 0.5f;
            cl->AddText(ImVec2(p.x + pad, ty), ImGui::GetColorU32(sel ? ImVec4(1, 1, 1, 1) : ImVec4(0.82f, 0.82f, 0.85f, 1.0f)), label);
            char shown[96];
            SDL_snprintf(shown, sizeof shown, sel && value[0] != '(' ? "<  %s  >" : "%s", value);
            if (value[0] == '(' && !sel) shown[0] = 0;
            cl->AddText(ImVec2(p.x + rw - pad - ImGui::CalcTextSize(shown).x, ty), ImGui::GetColorU32(vc), shown);
            ImGui::Dummy(ImVec2(rw, lh));
        };
        auto head = [&](const char* t) {
            ImGui::Dummy(ImVec2(0, ImGui::GetFontSize() * 0.35f));
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::TextColored(head_col, "%s", t);
            float tx = ImGui::CalcTextSize(t).x + ImGui::GetFontSize() * 0.6f, my = p.y + ImGui::GetTextLineHeight() * 0.55f;
            ImGui::GetWindowDrawList()->AddLine(ImVec2(p.x + tx, my), ImVec2(p.x + rw, my), accent_dim, 1.0f);
        };
        const ImVec4 num(1.0f, 0.86f, 0.55f, 1.0f);
        head("PLAYER");
        ImGui::TextColored(dim, "  %s   HP %d", c->player ? "Chris" : "Claire", c->hp);
        row(QC_HEALTH, "Infinite health", c->inf_health ? "On" : "Off", c->inf_health ? on_col : dim);
        row(QC_HEAL, "Heal now", "(A)", dim);
        row(QC_AMMO, "Infinite ammo", c->inf_ammo ? "On" : "Off", c->inf_ammo ? on_col : dim);
        head("GIVE ITEM");
        const char* nm = c->give_id > 0 && c->give_id < CHEAT_ITEMS && c->names[c->give_id][0] ? c->names[c->give_id] : "-";
        row(QC_ITEM, "Item", nm, num);
        SDL_snprintf(val, sizeof val, "%d", c->give_count);
        row(QC_COUNT, "Count", val, num);
        row(QC_GIVE, "Add to inventory", "(A)", dim);
        if (c->result && c->result[0]) ImGui::TextColored(on_col, "  %s", c->result);
        head("INVENTORY");
        for (int i = 2; i < 2 + c->slots && i < 12; i++) {
            unsigned id = c->inv[i] >> 16 & 0xFF;
            if (!id) continue;
            ImGui::TextColored(ImVec4(0.82f, 0.82f, 0.85f, 1.0f), "  %s%s  x%u", (unsigned)i == c->inv[0] ? "> " : "",
                               id < CHEAT_ITEMS && c->names[id][0] ? c->names[id] : "?", c->inv[i] & 0xFFFF);
        }
    }
    for (int i = 0; quick_tab == 0 && i < QUICK_N; i++) {
        const QuickRow* r = &quick_rows[i];
        ImDrawList* cl = ImGui::GetWindowDrawList();
        if (r->kind == Q_HEAD) {
            ImGui::Dummy(ImVec2(0, ImGui::GetFontSize() * 0.35f));
            if (r->text[0]) {
                ImVec2 p = ImGui::GetCursorScreenPos();
                ImGui::TextColored(head_col, "%s", r->text);
                float tx = ImGui::CalcTextSize(r->text).x + ImGui::GetFontSize() * 0.6f, my = p.y + ImGui::GetTextLineHeight() * 0.55f;
                cl->AddLine(ImVec2(p.x + tx, my), ImVec2(p.x + rw, my), accent_dim, 1.0f);
            }
            continue;
        }
        const Setting* s = quick_setting(r);
        if (r->kind == Q_SET && !s) continue;
        bool sel = i == quick_sel, lock = s && settings_locked(s);
        ImVec2 p = ImGui::GetCursorScreenPos();
        if (sel) {
            cl->AddRectFilled(p, ImVec2(p.x + rw, p.y + lh), IM_COL32(200, 30, 30, 70), 6.0f);
            cl->AddRectFilled(p, ImVec2(p.x + 4.0f, p.y + lh), accent, 2.0f);
            if (ImGui::GetScrollMaxY() > 0) ImGui::SetScrollHereY(0.5f);
            help = s ? (lock ? settings_locked(s) : s->help) : r->kind == Q_ALL ? "Every setting, page by page (also - and + together)." : "Closes this menu (also B, or L3 and R3 again).";
        }
        float ty = p.y + (lh - ImGui::GetTextLineHeight()) * 0.5f, pad = ImGui::GetFontSize() * 0.7f;
        ImVec4 lc = lock ? ImVec4(0.45f, 0.45f, 0.48f, 1.0f) : sel ? ImVec4(1, 1, 1, 1) : ImVec4(0.82f, 0.82f, 0.85f, 1.0f);
        cl->AddText(ImVec2(p.x + pad, ty), ImGui::GetColorU32(lc), s ? s->label : r->text);
        if (s) {
            settings_value(s, val, sizeof val);
            bool b = settings_is_bool(s);
            ImVec4 vc = lock ? lc : b ? (settings_get(s) ? on_col : dim) : ImVec4(1.0f, 0.86f, 0.55f, 1.0f);
            char shown[112];
            SDL_snprintf(shown, sizeof shown, sel && !lock ? "<  %s  >" : "%s", val);
            float vx = p.x + rw - pad - ImGui::CalcTextSize(shown).x;
            if (!b && !s->names && !s->name && settings_hi(s) > s->lo) {   /* a number: a bar under the value */
                float bw = ImGui::GetFontSize() * 6.0f, bx = p.x + rw - pad - bw, by = p.y + lh - 4.0f;
                float t = (float)(settings_get(s) - s->lo) / (float)(settings_hi(s) - s->lo);
                cl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + 2.5f), IM_COL32(255, 255, 255, 40), 1.0f);
                cl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw * t, by + 2.5f), sel ? accent : IM_COL32(200, 30, 30, 150), 1.0f);
            }
            cl->AddText(ImVec2(vx, ty), ImGui::GetColorU32(vc), shown);
        } else if (sel) {
            const char* a = "(A)";
            cl->AddText(ImVec2(p.x + rw - pad - ImGui::CalcTextSize(a).x, ty), ImGui::GetColorU32(dim), a);
        }
        ImGui::Dummy(ImVec2(rw, lh));
    }
    ImGui::EndChild();

    ImVec2 fp = ImGui::GetCursorScreenPos();
    dl->AddLine(fp, ImVec2(fp.x + ImGui::GetContentRegionAvail().x, fp.y), accent_dim, 1.0f);
    ImGui::Dummy(ImVec2(0, ImGui::GetFontSize() * 0.3f));
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.88f, 1.0f), "%s", help ? help : "");
    ImGui::PopTextWrapPos();
    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - ImGui::GetTextLineHeightWithSpacing() - ImGui::GetStyle().WindowPadding.y);
    ImGui::TextColored(dim, "D-pad  choose / change     (A) change     (L)(R) tab     (B) close");
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    if (changed) settings_save_later();
}
#endif

int overlay_event(const void* e)
{
    const SDL_Event* ev = (const SDL_Event*)e;
    if (!ready) return 0;
    if (tps_key_wait && ev->type == SDL_EVENT_KEY_DOWN && !ev->key.repeat) {
        if (ev->key.scancode != SDL_SCANCODE_ESCAPE) { g_tps.key = ev->key.scancode; settings_save_later(); }
        tps_key_wait = false;
        return 1;
    }
#ifdef __SWITCH__
    if (quick_combo(ev)) return 1;
    if (switch_combo(ev)) { quick_visible = false; return 1; }
#endif
    if (bind_hotkey(ev, B_SETTINGS)) {
        visible = !visible;
        return 1;
    }
    if (bind_hotkey(ev, B_CHEATS)) {
        cheats_visible = !cheats_visible;
        return 1;
    }
    ImGui_ImplSDL3_ProcessEvent(ev);
    if (!visible && !cheats_visible) return 0;
    return (ev->type == SDL_EVENT_KEY_DOWN || ev->type == SDL_EVENT_KEY_UP || ev->type == SDL_EVENT_TEXT_INPUT ||
            ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev->type == SDL_EVENT_MOUSE_BUTTON_UP || ev->type == SDL_EVENT_MOUSE_WHEEL);
}

/* the rows of a page of the shared table (settings.c): all of them, or on the pages that have
   their own windows (EXTRAS: F2) the ones flagged SET_F1. Values are applied by
   settings_poll (next pumped frame) and recvx.ini written once they stop changing. true = changed */
/* the picture calibration test pattern (settings_calib), in the colours the game's picture gets
   (the overlay itself is drawn without the calibration) */
static void calib_pattern(void)
{
    float sq = ImGui::GetFontSize() * 1.5f, gap = sq * 0.5f, w = CALIB_STEPS * (sq + gap) + gap;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto grey = [](int v) { int g = (int)(gs_picture(v / 255.0f) * 255.0f + 0.5f); return IM_COL32(g, g, g, 255); };
    for (int k = 0; k < 2; k++) {
        float x = p.x + k * (w + gap);
        dl->AddRectFilled(ImVec2(x, p.y), ImVec2(x + w, p.y + sq + 2 * gap), grey(k ? 255 : 0));
        for (int i = 0; i < CALIB_STEPS; i++)
            dl->AddRectFilled(ImVec2(x + gap + i * (sq + gap), p.y + gap), ImVec2(x + gap + i * (sq + gap) + sq, p.y + gap + sq), grey(settings_calib[k][i]));
    }
    ImGui::Dummy(ImVec2(2 * w + gap, sq + 2 * gap));
}

static bool settings_imgui(int page, bool all = true)
{
    bool changed = false;
    for (int i = 0; i < g_settings_n; i++) {
        const Setting* s = &g_settings[i];
        if (s->page != page || !(all || (s->flags & SET_F1)) || (s->shown && !s->shown())) continue;
        if (s->group && all) ImGui::SeparatorText(s->group);
        const char* lock = settings_locked(s);
        char label[96], val[96];
        int v = settings_get(s), hi = settings_hi(s), st = s->step > 0 ? s->step : 1;
        SDL_snprintf(label, sizeof label, "%s%s##set%d", s->label, (s->flags & SET_RESTART) ? " (next start)" : "", i);
        ImGui::BeginDisabled(lock != nullptr);
        if (s->flags & SET_ACTION) {
            if (ImGui::Button(label)) settings_act(s);
        } else if (settings_is_bool(s)) {
            bool b = v != 0;
            if (ImGui::Checkbox(label, &b)) { settings_set(s, b); changed = true; }
        } else if (s->names || (s->name && (s->count || (hi - s->lo) / st <= 10))) {   /* a list (tenths: a slider) */
            if (ImGui::BeginCombo(label, settings_value(s, val, sizeof val), ImGuiComboFlags_HeightLarge)) {
                for (int k = s->lo; k <= hi; k += st) {
                    char nm[96];
                    if (ImGui::Selectable(s->name ? s->name(k, nm, sizeof nm) : s->names[k - s->lo], k == v)) { settings_set(s, k); changed = true; }
                    if (k == v) ImGui::SetItemDefaultFocus();   /* the list opens at the current value (a long one: "You appear as") */
                }
                ImGui::EndCombo();
            }
        } else {   /* a number; a value name (tenths) is shown as the slider's text */
            char fmt[96];
            const char* f = s->fmt ? s->fmt : "%d";
            if (s->name) {
                char* o = fmt;
                for (const char* c = s->name(v, val, sizeof val); *c && o < fmt + sizeof fmt - 2; c++) { if (*c == '%') *o++ = '%'; *o++ = *c; }
                *o = 0;
                f = fmt;
            }
            if (ImGui::SliderInt(label, &v, s->lo, hi, f)) {
                settings_set(s, st > 1 ? (v + st / 2) / st * st : v);
                changed = true;
            }
        }
        ImGui::EndDisabled();
#ifdef __SWITCH__
        /* no mouse to hover with: the focused row's help goes to the menu's footer */
        if ((ImGui::IsItemFocused() || ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) && (s->help || lock))
            SDL_snprintf(sw_help, sizeof sw_help, "%s%s%s", lock ? lock : "", lock && s->help ? "\n" : "", s->help ? s->help : "");
        if (false)
#endif
        if ((ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) || ((s->flags & SET_CALIB) && ImGui::IsItemActive())) && (s->help || lock)) {
            ImGui::BeginTooltip();
            ImGui::Text("%s%s%s", lock ? lock : "", lock && s->help ? "\n" : "", s->help ? s->help : "");
            if (s->flags & SET_CALIB) calib_pattern();
            ImGui::EndTooltip();
        }
    }
    return changed;
}

static bool settings_imgui_any(int page)
{
    for (int i = 0; i < g_settings_n; i++) if (g_settings[i].page == page && (g_settings[i].flags & SET_F1)) return true;
    return false;
}

/* Controls: the bindings of both sets; a click on a binding waits for a key (bindings.c) */
static void controls_section(void)
{
    static int dev, cap_row = -1, cap_slot;
    static char msg[128];
    char label[128], nm[64];
    if (cap_row >= 0) {
        int c = bind_capture_poll();
        if (c != -1) {
            if (c != -2) {
                bind_set(cap_row, dev, cap_slot, c == -3 ? 0 : c);
                if (bind_swapped()) SDL_snprintf(msg, sizeof msg, "Swapped with %s", bind_swapped());
                else if (bind_combo(cap_row, dev, cap_slot)) SDL_snprintf(msg, sizeof msg, "Shared: hold aim and press it to reload");
                else msg[0] = 0;
                settings_save_later();
            }
            cap_row = -1;
        }
    }
    settings_imgui(SP_CONTROLS);   /* input source, which gamepad */
#ifdef __SWITCH__
    if (dev != BIND_PAD) { dev = BIND_PAD; cap_row = -1; }   /* no keyboard to bind */
#else
    if (ImGui::BeginTabBar("##dev")) {
        if (ImGui::BeginTabItem("Keyboard / mouse")) { if (dev != BIND_KB) cap_row = -1; dev = BIND_KB; ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Gamepad")) { if (dev != BIND_PAD) cap_row = -1; dev = BIND_PAD; ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
#endif
    char pad_name[96];
    if (dev == BIND_PAD) ImGui::TextDisabled("%s; the left stick also moves, as on the DS2. The third-person rows play in third\n"
                                             "person with Modern gamepad controls, instead of the game rows of the same buttons.", bind_pad_desc(pad_name, sizeof pad_name));
#ifdef __SWITCH__
    ImGui::TextDisabled("Press A on a binding, then the new button. Reload can share a button: then Aim + button reloads.");
#else
    ImGui::TextDisabled("Click a binding, then press the key/button (Esc cancels, Delete clears); right click clears.\n"
                        "Reload can share a button: then Aim + button reloads.");
#endif
    if (msg[0]) ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "%s", msg);
    if (ImGui::BeginTable("##binds", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("action", ImGuiTableColumnFlags_WidthStretch, 2.2f);
        ImGui::TableSetupColumn("1", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("2", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        int group = -1;
        for (int i = 0; i < B_COUNT; i++) {
            int r = bind_order[i];
            if (!bind_row_shown(r, dev)) continue;
            if (bind_rows[r].group != group) {
                group = bind_rows[r].group;
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (group == BG_GAME) ImGui::SeparatorText("Game (as the KEY CONFIG type sets them)");
                else ImGui::SeparatorText(group == BG_TPS ? (dev ? "Third person (gamepad, Modern)" : "Third person (keyboard and mouse)") : "Hotkeys");
                if (group == BG_GAME) { ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("type %c", 'A' + pc_keytype()); }
            }
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            int conflict = bind_conflict(r, dev);
            if (conflict) ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", bind_row_label(r, label, sizeof label));
            else ImGui::TextUnformatted(bind_row_label(r, label, sizeof label));
            if (conflict && ImGui::IsItemHovered()) ImGui::SetTooltip("Also bound to: %s", bind_rows[conflict - 1].name);
            for (int k = 0; k < BIND_SLOTS; k++) {
                ImGui::TableSetColumnIndex(1 + k);
                char b[96];
                if (cap_row == r && cap_slot == k) SDL_snprintf(b, sizeof b, "press... %ds##%d_%d", (bind_capture_left_ms() + 999) / 1000, r, k);
                else SDL_snprintf(b, sizeof b, "%s%s##%d_%d", bind_combo(r, dev, k) ? "Aim + " : "", bind_code_name(dev, bind_get(r, dev, k), nm, sizeof nm), r, k);
                if (ImGui::Button(b, ImVec2(-FLT_MIN, 0))) { cap_row = r; cap_slot = k; msg[0] = 0; bind_capture_start(dev); }
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) { bind_set(r, dev, k, 0); settings_save_later(); }
            }
        }
        ImGui::EndTable();
    }
    if (ImGui::Button(dev ? "Reset gamepad to defaults" : "Reset keyboard / mouse to defaults")) { bind_reset(dev); settings_save_later(); msg[0] = 0; }
    ImGui::TextDisabled("Also in the game's OPTION menu: ENHANCED SETTINGS -> CONTROLS.");
}

static bool tps_section(void);
static void perf_section(void);

static void settings_window(void)
{
    bool changed = false;
    int ww, wh;
    SDL_GetWindowSizeInPixels(win, &ww, &wh);

    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600 * ImGui::GetStyle().FontScaleDpi, 0), ImGuiCond_FirstUseEver);   /* wide enough for the update line and its button; text scales with the window (overlay_render) */
    if (!ImGui::Begin("Resident Evil Code: Veronica X  -  settings (F1)  -  " RECVX_VERSION, &visible)) { ImGui::End(); return; }
    {   /* Check for updates (update.c) */
        char v[32];
        if (update_newer(v, sizeof v)) {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Update available: %s (you have %s)", v, update_current());
            ImGui::SameLine();
            if (ImGui::SmallButton("Open download page")) SDL_OpenURL(UPDATE_RELEASES_URL);
            ImGui::Separator();
        }
    }

    if (ImGui::CollapsingHeader("Display")) {
        changed |= settings_imgui(SP_DISPLAY);
        ImGui::TextDisabled("Output %d x %d, %.1f fps presented", ww, wh, ImGui::GetIO().Framerate);
    }
    if (ImGui::CollapsingHeader("Graphics")) changed |= settings_imgui(SP_GRAPHICS);
    if (ImGui::CollapsingHeader("Gameplay")) changed |= settings_imgui(SP_GAMEPLAY);
    if (ImGui::CollapsingHeader("Controls")) controls_section();
    if (ImGui::CollapsingHeader("Third person")) changed |= tps_section();
    if (ImGui::CollapsingHeader("Audio")) changed |= settings_imgui(SP_AUDIO);
    if (ImGui::CollapsingHeader("Appearance")) changed |= settings_imgui(SP_APPEAR);
    if (settings_imgui_any(SP_EXTRAS) && ImGui::CollapsingHeader("Extras"))   /* a page with its own window (F2): the SET_F1 rows */
        changed |= settings_imgui(SP_EXTRAS, false);
    if (ImGui::CollapsingHeader("Performance")) perf_section();
    ImGui::End();
    if (changed) settings_save_later();
}

static bool tps_section(void)
{
    bool changed = settings_imgui(SP_TPS);
#ifdef __SWITCH__
    ImGui::TextDisabled("Gamepad (Modern): left stick walks (pushed far runs), right stick camera / aim; ZL aim, ZR fire,\n"
                        "A action, X run, B cancel, Y inventory, - map, L knife, R reload, d-pad down quick turn, R3 camera behind.");
#else
    char key[96];
    SDL_snprintf(key, sizeof key, tps_key_wait ? "Press a key (Esc cancels)" : "Toggle key: %s (change)", SDL_GetScancodeName((SDL_Scancode)g_tps.key));
    if (ImGui::Button(key)) tps_key_wait = true;
    ImGui::TextDisabled(g_tps.free_cam
                        ? "W/A/S/D walk toward that screen direction, Shift run, mouse turns the camera; right mouse aim, left fire;\n"
                          "E action, R reload, Q cancel, Tab status, M map (Controls changes them). F1 frees the mouse."
                        : "W/S walk, A/D turn, Shift run, mouse look; right mouse aim, left fire;\n"
                          "E action, R reload, Q cancel, Tab status, M map (Controls changes them). F1 frees the mouse.");
    if (g_tps.pad_modern)
        ImGui::TextDisabled("Gamepad: left stick walks (pushed far runs), right stick camera / aim; LT aim, RT fire, A action,\n"
                            "X run, B cancel, Y inventory, Back map, LB knife, RB reload, d-pad down quick turn, RS camera behind.");
#endif
    return changed;
}

static void perf_section(void)
{
    {
        static ProfFrame h[240];
        bool on = prof_enabled != 0;
        if (ImGui::Checkbox("Measure (small overhead; always on with --profile)", &on)) prof_enabled = on;
        int n = prof_history(h, 240);
        if (n > 0) {
            float total[240], avg[PROF_COUNT] = { 0 }, avg_total = 0, worst = 0;
            int m = n < 60 ? n : 60;
            for (int i = 0; i < n; i++) total[i] = h[i].total;
            for (int i = n - m; i < n; i++) {
                avg_total += h[i].total / m;
                if (h[i].total > worst) worst = h[i].total;
                for (int c = 0; c < PROF_COUNT; c++) avg[c] += h[i].ms[c] / m;
            }
            ImGui::Text("game frame %.2f ms avg, %.2f ms worst (target 33.37 ms = 29.97 Hz)", avg_total, worst);
            ImGui::PlotLines("##ft", total, n, 0, "frame time (ms)", 0.0f, 66.7f, ImVec2(-1, 60));
            for (int c = 0; c < PROF_COUNT; c++) {
                ImGui::Text("%-8s %6.2f ms", prof_names[c], avg[c]);
                ImGui::SameLine(160);
                ImGui::ProgressBar(avg_total > 0 ? avg[c] / avg_total : 0, ImVec2(-1, 0), "");
            }
            ImGui::TextDisabled("'wait' is idle time; run with --profile for a CSV.");
        }
    }
}

#ifdef __SWITCH__
/* the Switch's settings: a full-screen window, one page per tab (L / R), the focused row's
   help in a footer instead of a tooltip, the pad's buttons spelled out; B on a page closes it */
static void switch_settings_window(void)
{
    static const char* const tabs[] = { "Display", "Graphics", "Gameplay", "Controls", "Third person", "Audio", "Appearance", "Extras", "Performance" };
    const int ntabs = (int)(sizeof tabs / sizeof *tabs);
    static int tab;
    static bool busy_last;
    bool changed = false;
    ImVec2 ds = ImGui::GetIO().DisplaySize;
    int want = -1;
    if (sw_tab_step) { want = (tab + sw_tab_step + ntabs) % ntabs; sw_tab_step = 0; }
    if (sw_open) { ImGui::SetNextWindowFocus(); sw_open = false; busy_last = true; }
    sw_help[0] = 0;

    ImGui::SetNextWindowPos(ImVec2(ds.x * 0.04f, ds.y * 0.05f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(ds.x * 0.92f, ds.y * 0.90f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGui::Begin("Code: Veronica X  -  settings  -  " RECVX_VERSION "##sw", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextDisabled("%.0f fps", ImGui::GetIO().Framerate);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("(L) previous tab   next tab (R)").x);
    ImGui::TextDisabled("(L) previous tab   next tab (R)");

    float footer = ImGui::GetTextLineHeightWithSpacing() * 4 + ImGui::GetStyle().ItemSpacing.y * 2;
    if (ImGui::BeginTabBar("##swtabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
        for (int i = 0; i < ntabs; i++) {
            if (i == 7 && !settings_imgui_any(SP_EXTRAS)) continue;
            if (!ImGui::BeginTabItem(tabs[i], nullptr, i == want ? ImGuiTabItemFlags_SetSelected : 0)) continue;
            tab = i;
            ImGui::BeginChild("##page", ImVec2(0, -footer), ImGuiChildFlags_NavFlattened);
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
            switch (i) {
            case 0: {
                int ww, wh;
                SDL_GetWindowSizeInPixels(win, &ww, &wh);
                changed |= settings_imgui(SP_DISPLAY);
                ImGui::TextDisabled("Output %d x %d, %.1f fps presented", ww, wh, ImGui::GetIO().Framerate);
                break;
            }
            case 1: changed |= settings_imgui(SP_GRAPHICS); break;
            case 2: changed |= settings_imgui(SP_GAMEPLAY); break;
            case 3: controls_section(); break;
            case 4: changed |= tps_section(); break;
            case 5: changed |= settings_imgui(SP_AUDIO); break;
            case 6: changed |= settings_imgui(SP_APPEAR); break;
            case 7: changed |= settings_imgui(SP_EXTRAS, false); break;
            case 8: perf_section(); break;
            }
            ImGui::PopItemWidth();
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::Separator();
    ImGui::PushTextWrapPos(0.0f);
    if (sw_help[0]) ImGui::TextUnformatted(sw_help);
    else ImGui::TextDisabled("Move to a setting to see what it does.");
    ImGui::PopTextWrapPos();
    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - ImGui::GetTextLineHeightWithSpacing() - ImGui::GetStyle().WindowPadding.y);
    ImGui::TextDisabled("(A) select   (B) back / close   D-pad / stick move   (L)(R) tab   (-)+(+) close   (-)+(+) with ZR: cheats");

    /* B with nothing open (a list, a slider being changed) closes the menu: last frame's state,
       since the B that closed a list has already closed it by now */
    bool busy = ImGui::IsAnyItemActive() || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
    if (!busy && !busy_last && ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) visible = false;
    busy_last = busy;
    ImGui::End();
    if (changed) settings_save_later();
}
#endif

/* F2 teleport: location, room and entry by name (teleport.c), a search over all rooms, or the raw numbers */
static void teleport_pick(CheatState* c)
{
    static bool raw;
    static char filter[48];
    char b[128], e[160];
    int n = tp_count(), k, cur = tp_find(c->tp_stg, c->tp_rom);
    if (n && !raw) {
        if (ImGui::BeginCombo("Location", tp_stage_name(c->tp_stg), ImGuiComboFlags_HeightLarge)) {
            for (int s = 0; s < 10; s++) {
                int first = tp_stage_first(s, &k);
                if (k && ImGui::Selectable(tp_stage_name(s), s == c->tp_stg)) { c->tp_stg = s; c->tp_rom = tp_room(first)->rom; c->tp_pos = 0; }
            }
            ImGui::EndCombo();
        }
        if (ImGui::BeginCombo("Room", cur >= 0 ? tp_room_name(cur, 1, b, sizeof b) : "-", ImGuiComboFlags_HeightLarge)) {
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##filter", "type to search every location", filter, sizeof filter);
            for (int i = 0; i < n; i++) {
                const TpRoom* r = tp_room(i);
                tp_room_name(i, 1, b, sizeof b);
                if (filter[0] ? !SDL_strcasestr(b, filter) && !SDL_strcasestr(tp_stage_name(r->stg), filter) : r->stg != c->tp_stg) continue;
                if (filter[0]) snprintf(e, sizeof e, "%s   %s##%d", b, tp_stage_name(r->stg), i);
                else snprintf(e, sizeof e, "%s##%d", b, i);
                if (ImGui::Selectable(e, i == cur)) { c->tp_stg = r->stg; c->tp_rom = r->rom; c->tp_pos = 0; }
                if (i == cur && ImGui::IsWindowAppearing()) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (cur >= 0) {
            int m = tp_entries(cur);
            if (c->tp_pos >= m) c->tp_pos = 0;
            if (ImGui::BeginCombo("Entry", tp_entry_name(cur, c->tp_pos, b, sizeof b), ImGuiComboFlags_HeightLarge)) {
                for (int p = 0; p < m; p++) {
                    snprintf(e, sizeof e, "%s##%d", tp_entry_name(cur, p, b, sizeof b), p);
                    if (ImGui::Selectable(e, p == c->tp_pos)) c->tp_pos = p;
                    if (ImGui::IsItemHovered() && tp_entry_sources(cur, p, b, sizeof b)[0]) ImGui::SetTooltip("Doors here from:\n%s", b);
                }
                ImGui::EndCombo();
            }
        }
        ImGui::TextDisabled("A room may not work in every story state.");
    } else {
        if (!n) ImGui::TextDisabled(tp_reading() ? "Reading the room list from the disc..." : "The room list could not be read.");
        ImGui::TextDisabled("Uses the game's door transition; an invalid room or position may crash.");
        ImGui::InputInt("Stage", &c->tp_stg);
        ImGui::InputInt("Room", &c->tp_rom);
        ImGui::InputInt("Position", &c->tp_pos);
        c->tp_stg = SDL_clamp(c->tp_stg, 0, 9); c->tp_rom = SDL_clamp(c->tp_rom, 0, 99); c->tp_pos = SDL_clamp(c->tp_pos, 0, TP_POS - 1);
    }
    ImGui::Checkbox("Raw ids", &raw);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stage, room and entry position as numbers (as RECVX_TELEPORT), also rooms not in the list.");
    ImGui::SameLine();
}

/* surface materials (gs/materials.c): picking a textured 3D surface in the game picture with the
   mouse (overlay_render takes the click), its class, and its surfaces highlighted */
static bool mat_picking, mat_highlight = true, mat_was_open;
static uint32_t mat_sel;
static void material_section(void)
{
    uint32_t h;
    ImGui::SeparatorText("Surface materials");
    if (gs_pick_result(&h)) mat_sel = h;
    ImGui::Checkbox("Pick a surface with the mouse", &mat_picking);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Click a wall, floor or object in the game picture (outside this window) to choose its texture.\n"
                          "Only textured 3D surfaces can be picked. Classes are saved to mods/materials.ini at once\n"
                          "and shine with Material shine (GRAPHICS) and the native 3D renderer.");
    if (mat_sel) {
        int cls = mat_get(mat_sel);
        ImGui::Text("Texture %08X", mat_sel);
        ImGui::SameLine();
        ImGui::Checkbox("Highlight", &mat_highlight);
        if (ImGui::BeginCombo("Material", mat_class_name(cls))) {
            for (int k = 0; k < mat_class_count(); k++)
                if (ImGui::Selectable(mat_class_name(k), k == cls)) mat_set(mat_sel, k);
            ImGui::EndCombo();
        }
    } else {
        ImGui::TextDisabled(mat_picking ? "Click a surface in the game picture." : "No surface chosen.");
    }
    ImGui::TextDisabled("%d textures have a material (mods/materials.ini)", mat_count());
    gs_mat_highlight(mat_sel && mat_highlight && cheats_visible ? mat_sel : 0);
}

static void cheat_window(void)
{
    CheatState* c = &g_cheat;
    ImGui::SetNextWindowPos(ImVec2(500, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420 * ImGui::GetStyle().FontScaleDpi, 0), ImGuiCond_FirstUseEver);   /* text scales with the window */
    if (!ImGui::Begin("Cheats (F2)", &cheats_visible)) { ImGui::End(); return; }
    ImGui::Text("%s  HP %d   stage %d  room %d  pos %d", c->player ? "Chris" : "Claire", c->hp, c->stg, c->rom, c->pos);
    bool b = c->inf_health;
    if (ImGui::Checkbox("Infinite health", &b)) c->inf_health = b;
    ImGui::SameLine();
    if (ImGui::Button("Heal now")) __atomic_or_fetch(&c->request, CHEAT_HEAL, __ATOMIC_ACQ_REL);
    b = c->inf_ammo;
    if (ImGui::Checkbox("Infinite ammo", &b)) c->inf_ammo = b;
    {
        static const char* modes[] = { "Off", "Through walls", "Through walls + fly (R/F, PgUp/PgDn, R2/L2)" };
        int m = c->noclip;
        if (ImGui::Combo("Noclip", &m, modes, IM_ARRAYSIZE(modes))) c->noclip = m;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Walls, objects and enemies no longer block you; doors and triggers still work.\n"
                              "Fly: R / Page Up / R2 up, F / Page Down / L2 down.\n"
                              "Turning it off after flying puts you on the floor level below.");
    }
    b = c->unlock;
    if (ImGui::Checkbox("Unlock all", &b)) c->unlock = b;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Examining a locked door, card reader or key slot uses its key there, as if you carried it.\n"
                          "Nothing is added to the inventory. Doors locked by the story stay locked.\n"
                          "Turned on inside a room, some doors notice it only after you enter the room again.");

    ImGui::SeparatorText("Give item");
    char label[48];
    snprintf(label, sizeof label, "%3d  %s", c->give_id, c->give_id > 0 && c->give_id < CHEAT_ITEMS ? c->names[c->give_id] : "-");
    if (ImGui::BeginCombo("Item", label, ImGuiComboFlags_HeightLarge)) {
        for (int id = 1; id < CHEAT_ITEMS; id++) {
            if (!c->names[id][0]) continue;
            snprintf(label, sizeof label, "%3d  %s", id, c->names[id]);
            if (ImGui::Selectable(label, id == c->give_id)) c->give_id = id;
        }
        ImGui::EndCombo();
    }
    ImGui::InputInt("Count", &c->give_count);
    c->give_count = SDL_clamp(c->give_count, 1, 999);
    if (ImGui::Button("Add to inventory") && c->give_id) __atomic_or_fetch(&c->request, CHEAT_GIVE, __ATOMIC_ACQ_REL);

    ImGui::SeparatorText("Inventory");
    for (int i = 1; i < 2 + c->slots && i < 12; i++) {
        unsigned id = c->inv[i] >> 16 & 0xFF;
        char nm[16];
        if (!id) continue;
        if (id >= CHEAT_ITEMS || !c->names[id][0]) SDL_snprintf(nm, sizeof nm, "item %u", id);
        ImGui::Text("%s %-24s %u", (unsigned)i == c->inv[0] ? "equipped" : "        ", id < CHEAT_ITEMS && c->names[id][0] ? c->names[id] : nm, c->inv[i] & 0xFFFF);
    }

    ImGui::SeparatorText("Spawn enemy");
    if (c->spawn_idx < 0 || c->spawn_idx >= spawn_count()) c->spawn_idx = 0;
    if (ImGui::BeginCombo("Enemy", spawn_name(c->spawn_idx), ImGuiComboFlags_HeightLarge)) {
        for (int i = 0; i < spawn_count(); i++)
            if (ImGui::Selectable(spawn_name(i), i == c->spawn_idx)) c->spawn_idx = i;
        ImGui::EndCombo();
    }
    if (ImGui::Button("Spawn in front of me")) __atomic_or_fetch(&c->request, CHEAT_SPAWN, __ATOMIC_ACQ_REL);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Loads the enemy from another room if this one has no such enemy.\n"
                          "Bosses and scripted enemies may not work outside their room (docs/ENEMIES.md).\n"
                          "Spawned enemies are gone after leaving the room; their kills are not recorded.");

    ImGui::SeparatorText("Teleport");
    teleport_pick(c);
    if (ImGui::Button("Current room")) { c->tp_stg = c->stg; c->tp_rom = c->rom; c->tp_pos = c->pos; }
    ImGui::SameLine();
    if (ImGui::Button("Go")) __atomic_or_fetch(&c->request, CHEAT_TELEPORT, __ATOMIC_ACQ_REL);
    if (c->result) ImGui::TextDisabled("%s", c->result);

    material_section();
    ImGui::End();
}

void overlay_render(void)
{
    if (!ready) return;
#ifdef __SWITCH__
    bool windows = visible || cheats_visible || quick_visible;
#else
    bool windows = visible || cheats_visible;
#endif
    pad_set_game_input_enabled(!windows);
    tps_update_mouse_mode(win, windows);
    menu_mouse_update(win, windows);
    /* without a window ImGui leaves the cursor alone (menu_mouse_update shows/hides it) */
    if (windows) ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
    else ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    bool crosshair = g_tps.on && g_tps.crosshair && g_tps.cam_active && g_tps.aiming;
    char upd[32];
    bool notice = !visible && update_title_up() && update_newer(upd, sizeof upd);   /* the title screen: an update */
    if (!windows && !g_config.show_fps && !crosshair && !notice) return;
    {   /* readable on large displays: scale text with the window height (1080p = 1x) */
        int ww, wh;
        SDL_GetWindowSizeInPixels(win, &ww, &wh);
        ImGui::GetStyle().FontScaleDpi = wh > 1080 ? wh / 1080.0f : 1.0f;
    }
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
#ifdef __SWITCH__
    if (visible) switch_settings_window();
    else if (quick_visible) quick_window();
#else
    if (visible) settings_window();
#endif
    if (cheats_visible) cheat_window();
    else if (mat_was_open) gs_mat_highlight(0);   /* the window closed: no highlight */
    mat_was_open = cheats_visible;
    if (cheats_visible && mat_picking && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::GetIO().WantCaptureMouse) {
        ImVec2 m = ImGui::GetIO().MousePos, k = ImGui::GetIO().DisplayFramebufferScale;   /* to window pixels */
        float x, y;
        gs_window_to_screen(m.x * k.x, m.y * k.y, &x, &y);
        gs_pick_request(x, y);
    }
    if (crosshair) {   /* third person: the screen centre is where the shot goes (tps_glue.c) */
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        ImVec2 c(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
        float s = ImGui::GetIO().DisplaySize.y / 120.0f, g = s * 0.6f, t = s > 8 ? 2.0f : 1.5f;
        ImU32 col = IM_COL32(255, 60, 40, 220), shadow = IM_COL32(0, 0, 0, 160);
        for (int pass = 0; pass < 2; pass++) {
            ImU32 k = pass ? col : shadow;
            float o = pass ? 0.0f : 1.0f;
            dl->AddLine(ImVec2(c.x - g - s + o, c.y + o), ImVec2(c.x - g + o, c.y + o), k, t);
            dl->AddLine(ImVec2(c.x + g + o, c.y + o), ImVec2(c.x + g + s + o, c.y + o), k, t);
            dl->AddLine(ImVec2(c.x + o, c.y - g - s + o), ImVec2(c.x + o, c.y - g + o), k, t);
            dl->AddLine(ImVec2(c.x + o, c.y + g + o), ImVec2(c.x + o, c.y + g + s + o), k, t);
            dl->AddCircleFilled(ImVec2(c.x + o, c.y + o), t, k);
        }
    }
    if (notice) {   /* top left, out of the way of the title's menu and copyright lines */
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.55f);
        ImGui::Begin("##update", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                                          ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Update available: %s (you have %s)", upd, update_current());
        ImGui::TextDisabled("F1 opens the download page");
        ImGui::End();
    }
    if (g_config.show_fps) {
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 10, 10), ImGuiCond_Always, ImVec2(1, 0));
        ImGui::SetNextWindowBgAlpha(0.4f);
        ImGui::Begin("##fps", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                                       ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::Text("%.1f fps", ImGui::GetIO().Framerate);
        ImGui::End();
    }
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
