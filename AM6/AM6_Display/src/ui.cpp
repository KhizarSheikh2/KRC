#include "ui.h"

#include <Arduino.h>
#include <lvgl.h>

#include "config.h"
#include "display_state.h"
#include "wireless_link.h"

namespace {

// ============================================================
// AM6 UI - reference-matched dark industrial dashboard
// 320x240 landscape, based on the supplied AM5 reference image.
// ============================================================
constexpr uint32_t C_BLACK         = 0x000000;
constexpr uint32_t C_BG_TOP        = 0x020A13;
constexpr uint32_t C_BG_BOTTOM     = 0x00101F;
constexpr uint32_t C_PANEL_TOP     = 0x041322;
constexpr uint32_t C_PANEL_BOTTOM  = 0x00101E;
constexpr uint32_t C_BLUE          = 0x129DFF;
constexpr uint32_t C_CYAN          = 0x21D8FF;
constexpr uint32_t C_CYAN_SOFT     = 0x8DE8FF;
constexpr uint32_t C_WHITE         = 0xF7FBFF;
constexpr uint32_t C_MUTED         = 0xB4C7DC;
constexpr uint32_t C_OFFLINE       = 0x1A2533;
constexpr uint32_t C_OFFLINE_EDGE  = 0x385C84;
constexpr uint32_t C_COOL_1        = 0x159EFF;
constexpr uint32_t C_COOL_2        = 0x075BC9;
constexpr uint32_t C_GREEN_1       = 0x29E581;
constexpr uint32_t C_GREEN_2       = 0x0A8549;
constexpr uint32_t C_RED_1         = 0xFF6666;
constexpr uint32_t C_RED_2         = 0x7A2028;
constexpr uint32_t C_AMBER         = 0xF5C34D;

lv_obj_t* splashScreen = nullptr;
lv_obj_t* splashBar = nullptr;
lv_obj_t* dashboardScreen = nullptr;

lv_obj_t* statusChip = nullptr;
lv_obj_t* statusLabel = nullptr;
lv_obj_t* sideStatusChip = nullptr;
lv_obj_t* sideStatusLabel = nullptr;

lv_obj_t* powerButton = nullptr;
lv_obj_t* powerIcon = nullptr;

lv_obj_t* coolButton = nullptr;
lv_obj_t* heatButton = nullptr;
lv_obj_t* lowButton = nullptr;
lv_obj_t* mediumButton = nullptr;
lv_obj_t* highButton = nullptr;

lv_obj_t* coolLabel = nullptr;
lv_obj_t* heatLabel = nullptr;
lv_obj_t* lowLabel = nullptr;
lv_obj_t* mediumLabel = nullptr;
lv_obj_t* highLabel = nullptr;

lv_obj_t* coolIconHost = nullptr;
lv_obj_t* heatIconHost = nullptr;
lv_obj_t* lowIconHost = nullptr;
lv_obj_t* mediumIconHost = nullptr;
lv_obj_t* highIconHost = nullptr;

uint32_t splashStartedMs = 0;
uint32_t lastModelRefreshMs = 0;
uint32_t lastRevision = 0;
bool dashboardLoaded = false;
AM6WirelessUiState lastLinkState = AM6_LINK_OFFLINE;

lv_color_t color(uint32_t hex) {
    // IMPORTANT: do not complement colors here. The ST7789 inversion command
    // is a panel-drive setting on this hardware, not a request to XOR RGB data.
    return lv_color_hex(hex);
}

void noScroll(lv_obj_t* obj) {
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_outline_width(obj, 0, LV_PART_MAIN);
}

void resetObject(lv_obj_t* obj) {
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(obj, 0, LV_PART_MAIN);
    noScroll(obj);
}

void setOpaqueBg(lv_obj_t* obj, uint32_t top, uint32_t bottom, lv_grad_dir_t dir) {
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, color(top), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(obj, color(bottom), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(obj, dir, LV_PART_MAIN);
}

void stylePanel(lv_obj_t* obj, int radius) {
    resetObject(obj);
    setOpaqueBg(obj, C_PANEL_TOP, C_PANEL_BOTTOM, LV_GRAD_DIR_VER);
    lv_obj_set_style_radius(obj, radius, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, color(C_BLUE), LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN);
}

void styleButton(lv_obj_t* btn) {
    resetObject(btn);
    setOpaqueBg(btn, 0x07203A, 0x031425, LV_GRAD_DIR_HOR);
    lv_obj_set_style_radius(btn, 11, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, color(C_BLUE), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 6, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(btn, color(C_BLUE), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_20, LV_PART_MAIN);
}

void styleLabel(lv_obj_t* label, const lv_font_t* font, uint32_t c) {
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color(c), LV_PART_MAIN);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, LV_PART_MAIN);
}

lv_obj_t* makeLabel(lv_obj_t* parent, const char* text, const lv_font_t* font, uint32_t c) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    styleLabel(label, font, c);
    return label;
}

void makeDivider(lv_obj_t* parent, int x, int y, int w, uint32_t c = C_BLUE) {
    lv_obj_t* line = lv_obj_create(parent);
    lv_obj_set_pos(line, x, y);
    lv_obj_set_size(line, w, 2);
    resetObject(line);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(line, color(c), LV_PART_MAIN);
}

void drawSnowflake(lv_obj_t* parent, uint32_t c) {
    // Cleaner 24x24 snowflake for the real panel.
    static lv_point_t h[]  = {{2, 12}, {22, 12}};
    static lv_point_t v[]  = {{12, 2}, {12, 22}};
    static lv_point_t d1[] = {{4, 4}, {20, 20}};
    static lv_point_t d2[] = {{20, 4}, {4, 20}};
    static lv_point_t b1[] = {{5, 10}, {8, 7}};
    static lv_point_t b2[] = {{16, 17}, {19, 14}};
    static lv_point_t b3[] = {{16, 7}, {19, 10}};
    static lv_point_t b4[] = {{5, 14}, {8, 17}};
    static lv_point_t b5[] = {{10, 5}, {7, 8}};
    static lv_point_t b6[] = {{14, 5}, {17, 8}};
    static lv_point_t b7[] = {{10, 19}, {7, 16}};
    static lv_point_t b8[] = {{14, 19}, {17, 16}};
    lv_point_t* sets[] = {h, v, d1, d2, b1, b2, b3, b4, b5, b6, b7, b8};
    for (auto* points : sets) {
        lv_obj_t* line = lv_line_create(parent);
        lv_line_set_points(line, points, 2);
        lv_obj_set_size(line, 24, 24);
        lv_obj_set_pos(line, 0, 0);
        lv_obj_set_style_line_width(line, (points == h || points == v || points == d1 || points == d2) ? 2 : 1, LV_PART_MAIN);
        lv_obj_set_style_line_color(line, color(c), LV_PART_MAIN);
        lv_obj_set_style_line_rounded(line, true, LV_PART_MAIN);
    }
}

void drawHeat(lv_obj_t* parent, uint32_t c) {
    static lv_point_t wave1[] = {{4, 21}, {2, 16}, {6, 11}, {3, 6}, {6, 2}};
    static lv_point_t wave2[] = {{11, 21}, {9, 16}, {13, 11}, {10, 6}, {13, 2}};
    static lv_point_t wave3[] = {{18, 21}, {16, 16}, {20, 11}, {17, 6}, {20, 2}};
    lv_point_t* sets[] = {wave1, wave2, wave3};
    for (auto* points : sets) {
        lv_obj_t* line = lv_line_create(parent);
        lv_line_set_points(line, points, 5);
        lv_obj_set_size(line, 24, 24);
        lv_obj_set_pos(line, 0, 0);
        lv_obj_set_style_line_width(line, 3, LV_PART_MAIN);
        lv_obj_set_style_line_color(line, color(c), LV_PART_MAIN);
        lv_obj_set_style_line_rounded(line, true, LV_PART_MAIN);
    }
}

void drawFan(lv_obj_t* parent, uint32_t c) {
    // Three-blade fan icon that reads clearly at 20x20.
    struct Blade { int x; int y; int w; int h; int r; };
    const Blade blades[] = {
        {7, 1, 6, 8, 3},
        {11, 9, 8, 6, 3},
        {1, 9, 8, 6, 3}
    };
    for (const Blade& b : blades) {
        lv_obj_t* blade = lv_obj_create(parent);
        lv_obj_set_pos(blade, b.x, b.y);
        lv_obj_set_size(blade, b.w, b.h);
        resetObject(blade);
        lv_obj_set_style_bg_opa(blade, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(blade, color(c), LV_PART_MAIN);
        lv_obj_set_style_radius(blade, b.r, LV_PART_MAIN);
    }
    lv_obj_t* hub = lv_obj_create(parent);
    lv_obj_set_pos(hub, 8, 8);
    lv_obj_set_size(hub, 4, 4);
    resetObject(hub);
    lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(hub, color(C_PANEL_TOP), LV_PART_MAIN);
    lv_obj_set_style_border_color(hub, color(c), LV_PART_MAIN);
    lv_obj_set_style_border_width(hub, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, LV_PART_MAIN);
}

void recolorFan(lv_obj_t* host, uint32_t c) {
    if (!host) return;
    const uint32_t count = lv_obj_get_child_cnt(host);
    for (uint32_t i = 0; i < count; ++i) {
        lv_obj_t* child = lv_obj_get_child(host, i);
        if (i < count - 1) lv_obj_set_style_bg_color(child, color(c), LV_PART_MAIN);
        else lv_obj_set_style_border_color(child, color(c), LV_PART_MAIN);
    }
}

void recolorLineIcon(lv_obj_t* host, uint32_t c) {
    if (!host) return;
    const uint32_t count = lv_obj_get_child_cnt(host);
    for (uint32_t i = 0; i < count; ++i) {
        lv_obj_t* child = lv_obj_get_child(host, i);
        lv_obj_set_style_line_color(child, color(c), LV_PART_MAIN);
    }
}

void setButtonState(lv_obj_t* btn,
                    lv_obj_t* label,
                    lv_obj_t* iconHost,
                    bool selected,
                    uint32_t accent1,
                    uint32_t accent2,
                    bool fanIcon) {
    if (!btn || !label) return;

    if (selected) {
        setOpaqueBg(btn, accent1, accent2, LV_GRAD_DIR_HOR);
        lv_obj_set_style_border_color(btn, color(C_CYAN), LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(btn, 9, LV_PART_MAIN);
        lv_obj_set_style_shadow_color(btn, color(accent1), LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(btn, LV_OPA_30, LV_PART_MAIN);
        lv_obj_set_style_text_color(label, color(C_WHITE), LV_PART_MAIN);
    } else {
        setOpaqueBg(btn, 0x07203A, 0x031425, LV_GRAD_DIR_HOR);
        lv_obj_set_style_border_color(btn, color(C_BLUE), LV_PART_MAIN);
        lv_obj_set_style_shadow_width(btn, 5, LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(btn, LV_OPA_20, LV_PART_MAIN);
        lv_obj_set_style_text_color(label, color(C_WHITE), LV_PART_MAIN);
    }

    if (fanIcon) {
        recolorFan(iconHost, selected ? C_WHITE : C_CYAN_SOFT);
    } else if (iconHost == coolIconHost) {
        recolorLineIcon(iconHost, selected ? C_WHITE : C_CYAN_SOFT);
    } else if (iconHost == heatIconHost) {
        recolorLineIcon(iconHost, selected ? C_WHITE : C_GREEN_1);
    }
}

void powerEvent(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_PRESSED) return;
    const AM6DisplayState state = displayStateGet();
    wirelessLinkRequestPower(state.systemPower == 1 ? 0 : 1);
}

void modeEvent(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_PRESSED) return;
    const uintptr_t value = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
    wirelessLinkRequestMode(static_cast<uint8_t>(value));
}

void fanEvent(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_PRESSED) return;
    const uintptr_t value = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
    wirelessLinkRequestFan(static_cast<uint8_t>(value));
}

void createSplash() {
    splashScreen = lv_obj_create(nullptr);
    resetObject(splashScreen);
    setOpaqueBg(splashScreen, C_BLACK, C_BG_BOTTOM, LV_GRAD_DIR_VER);

    lv_obj_t* frame = lv_obj_create(splashScreen);
    lv_obj_set_pos(frame, 24, 50);
    lv_obj_set_size(frame, 272, 132);
    stylePanel(frame, 14);

    lv_obj_t* title = makeLabel(frame, "AM6", &lv_font_montserrat_32, C_WHITE);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    lv_obj_t* subtitle = makeLabel(frame, "ALERT MASTER 6", &lv_font_montserrat_16, C_CYAN_SOFT);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 58);

    splashBar = lv_bar_create(frame);
    lv_obj_set_size(splashBar, 164, 7);
    lv_obj_align(splashBar, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_bar_set_range(splashBar, 0, 100);
    lv_bar_set_value(splashBar, 0, LV_ANIM_OFF);
    lv_obj_set_style_radius(splashBar, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(splashBar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(splashBar, color(0x102439), LV_PART_MAIN);
    lv_obj_set_style_radius(splashBar, 4, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(splashBar, color(C_BLUE), LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(splashBar, color(C_CYAN), LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_dir(splashBar, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
}

void createHeader(lv_obj_t* parent) {
    lv_obj_t* header = lv_obj_create(parent);
    lv_obj_set_pos(header, 4, 4);
    lv_obj_set_size(header, 312, 40);
    stylePanel(header, 11);

    lv_obj_t* logo = makeLabel(header, "AM6", &lv_font_montserrat_32, C_WHITE);
    lv_obj_align(logo, LV_ALIGN_LEFT_MID, 10, -1);

    statusChip = lv_obj_create(header);
    lv_obj_set_size(statusChip, 78, 26);
    lv_obj_align(statusChip, LV_ALIGN_RIGHT_MID, -6, 0);
    resetObject(statusChip);
    lv_obj_set_style_bg_opa(statusChip, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(statusChip, color(C_OFFLINE), LV_PART_MAIN);
    lv_obj_set_style_radius(statusChip, 13, LV_PART_MAIN);
    lv_obj_set_style_border_color(statusChip, color(C_BLUE), LV_PART_MAIN);
    lv_obj_set_style_border_width(statusChip, 2, LV_PART_MAIN);

    statusLabel = makeLabel(statusChip, "OFFLINE", &lv_font_montserrat_14, C_WHITE);
    lv_obj_center(statusLabel);
}

void createPowerPanel(lv_obj_t* parent) {
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_set_pos(panel, 4, 48);
    lv_obj_set_size(panel, 64, 188);
    stylePanel(panel, 11);

    lv_obj_t* title = makeLabel(panel, "Power", &lv_font_montserrat_16, C_WHITE);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    powerButton = lv_btn_create(panel);
    lv_obj_set_size(powerButton, 50, 50);
    lv_obj_align(powerButton, LV_ALIGN_TOP_MID, 0, 54);
    resetObject(powerButton);
    setOpaqueBg(powerButton, 0x0B3A67, 0x04162A, LV_GRAD_DIR_VER);
    lv_obj_set_style_radius(powerButton, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_color(powerButton, color(C_CYAN), LV_PART_MAIN);
    lv_obj_set_style_border_width(powerButton, 3, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(powerButton, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(powerButton, color(C_BLUE), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(powerButton, LV_OPA_30, LV_PART_MAIN);
    lv_obj_add_event_cb(powerButton, powerEvent, LV_EVENT_PRESSED, nullptr);

    powerIcon = makeLabel(powerButton, LV_SYMBOL_POWER, &lv_font_montserrat_24, C_CYAN);
    lv_obj_center(powerIcon);

    makeDivider(panel, 8, 120, 48, C_BLUE);

    lv_obj_t* statusTitle = makeLabel(panel, "Status", &lv_font_montserrat_14, C_WHITE);
    lv_obj_align(statusTitle, LV_ALIGN_TOP_MID, 0, 128);

    sideStatusChip = lv_obj_create(panel);
    lv_obj_set_size(sideStatusChip, 54, 26);
    lv_obj_align(sideStatusChip, LV_ALIGN_BOTTOM_MID, 0, -8);
    resetObject(sideStatusChip);
    lv_obj_set_style_bg_opa(sideStatusChip, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sideStatusChip, color(C_OFFLINE), LV_PART_MAIN);
    lv_obj_set_style_radius(sideStatusChip, 8, LV_PART_MAIN);
    lv_obj_set_style_border_color(sideStatusChip, color(C_OFFLINE_EDGE), LV_PART_MAIN);
    lv_obj_set_style_border_width(sideStatusChip, 2, LV_PART_MAIN);

    sideStatusLabel = makeLabel(sideStatusChip, "OFFLINE", &lv_font_montserrat_12, C_WHITE);
    lv_obj_center(sideStatusLabel);
}

void makeSectionTitle(lv_obj_t* panel, const char* text, int panelW, int lineW) {
    lv_obj_t* title = makeLabel(panel, text, &lv_font_montserrat_20, C_CYAN_SOFT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    makeDivider(panel, 10, 18, lineW, C_BLUE);
    makeDivider(panel, panelW - 10 - lineW, 18, lineW, C_BLUE);
}

lv_obj_t* makeIconHost(lv_obj_t* btn, int x, int y, int w = 34, int h = 34) {
    lv_obj_t* host = lv_obj_create(btn);
    lv_obj_set_pos(host, x, y);
    lv_obj_set_size(host, w, h);
    resetObject(host);
    lv_obj_set_style_bg_opa(host, LV_OPA_TRANSP, LV_PART_MAIN);
    return host;
}

void createModePanel(lv_obj_t* parent) {
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_set_pos(panel, 72, 48);
    lv_obj_set_size(panel, 244, 92);
    stylePanel(panel, 11);
    makeSectionTitle(panel, "MODE", 244, 55);

    coolButton = lv_btn_create(panel);
    lv_obj_set_pos(coolButton, 8, 34);
    lv_obj_set_size(coolButton, 112, 50);
    styleButton(coolButton);
    lv_obj_add_event_cb(coolButton, modeEvent, LV_EVENT_PRESSED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(AM6_MODE_COOL)));
    coolIconHost = makeIconHost(coolButton, 12, 12, 24, 24);
    drawSnowflake(coolIconHost, C_CYAN_SOFT);
    coolLabel = makeLabel(coolButton, "COOL", &lv_font_montserrat_16, C_WHITE);
    lv_obj_align(coolLabel, LV_ALIGN_CENTER, 19, 0);

    heatButton = lv_btn_create(panel);
    lv_obj_set_pos(heatButton, 124, 34);
    lv_obj_set_size(heatButton, 112, 50);
    styleButton(heatButton);
    lv_obj_add_event_cb(heatButton, modeEvent, LV_EVENT_PRESSED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(AM6_MODE_HEAT)));
    heatIconHost = makeIconHost(heatButton, 14, 12, 24, 24);
    drawHeat(heatIconHost, C_GREEN_1);
    heatLabel = makeLabel(heatButton, "HEAT", &lv_font_montserrat_16, C_WHITE);
    lv_obj_align(heatLabel, LV_ALIGN_CENTER, 19, 0);
}

void createFanButton(lv_obj_t* panel,
                     lv_obj_t** button,
                     lv_obj_t** iconHost,
                     lv_obj_t** label,
                     int x,
                     int w,
                     const char* text,
                     const lv_font_t* font,
                     uint8_t value) {
    *button = lv_btn_create(panel);
    lv_obj_set_pos(*button, x, 34);
    lv_obj_set_size(*button, w, 50);
    styleButton(*button);
    lv_obj_add_event_cb(*button, fanEvent, LV_EVENT_PRESSED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(value)));

    *iconHost = makeIconHost(*button, 6, 14, 20, 20);
    drawFan(*iconHost, C_CYAN_SOFT);

    *label = makeLabel(*button, text, font, C_WHITE);
    lv_obj_align(*label, LV_ALIGN_CENTER, 13, 0);
}

void createFanPanel(lv_obj_t* parent) {
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_set_pos(panel, 72, 144);
    lv_obj_set_size(panel, 244, 92);
    stylePanel(panel, 11);
    makeSectionTitle(panel, "FAN SPEED", 244, 38);

    createFanButton(panel, &lowButton, &lowIconHost, &lowLabel,
                    6, 70, "LOW", &lv_font_montserrat_12, AM6_FAN_LOW);
    createFanButton(panel, &mediumButton, &mediumIconHost, &mediumLabel,
                    80, 84, "MEDIUM", &lv_font_montserrat_12, AM6_FAN_MEDIUM);
    createFanButton(panel, &highButton, &highIconHost, &highLabel,
                    168, 70, "HIGH", &lv_font_montserrat_12, AM6_FAN_HIGH);
}

void setControlEnabled(lv_obj_t* obj, bool enabled) {
    if (!obj) return;
    if (enabled) lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    else lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(obj, enabled ? LV_OPA_COVER : LV_OPA_60, LV_PART_MAIN);
}

void applyStatusChip(lv_obj_t* chip, lv_obj_t* label, const char* text,
                     uint32_t fill, uint32_t border, uint32_t txt) {
    if (!chip || !label) return;
    lv_label_set_text(label, text);
    lv_obj_set_style_bg_color(chip, color(fill), LV_PART_MAIN);
    lv_obj_set_style_border_color(chip, color(border), LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color(txt), LV_PART_MAIN);
}

void updateLinkStatus(AM6WirelessUiState state) {
    const char* text = "OFFLINE";
    uint32_t fill = C_OFFLINE;
    uint32_t border = C_BLUE;
    uint32_t txt = C_WHITE;

    if (state == AM6_LINK_WAITING) {
        text = "WAIT";
        fill = 0x40330B;
        border = C_AMBER;
    } else if (state == AM6_LINK_ONLINE) {
        text = "ONLINE";
        fill = 0x083A22;
        border = C_GREEN_1;
    } else if (state == AM6_LINK_SYNCING) {
        text = "SYNC";
        fill = 0x073449;
        border = C_CYAN;
    }

    applyStatusChip(statusChip, statusLabel, text, fill, border, txt);
    applyStatusChip(sideStatusChip, sideStatusLabel, text, fill,
                    state == AM6_LINK_OFFLINE ? C_OFFLINE_EDGE : border, txt);

    const bool enabled = (state == AM6_LINK_ONLINE || state == AM6_LINK_SYNCING);
    setControlEnabled(powerButton, enabled);
    setControlEnabled(coolButton, enabled);
    setControlEnabled(heatButton, enabled);
    setControlEnabled(lowButton, enabled);
    setControlEnabled(mediumButton, enabled);
    setControlEnabled(highButton, enabled);
}

void updatePower(const AM6DisplayState& state) {
    const bool on = state.systemPower == 1;
    if (on) {
        setOpaqueBg(powerButton, 0x0B4B7E, 0x061B32, LV_GRAD_DIR_VER);
        lv_obj_set_style_border_color(powerButton, color(C_CYAN), LV_PART_MAIN);
        lv_obj_set_style_text_color(powerIcon, color(C_CYAN), LV_PART_MAIN);
        lv_obj_set_style_shadow_color(powerButton, color(C_BLUE), LV_PART_MAIN);
    } else {
        setOpaqueBg(powerButton, 0x301014, 0x120609, LV_GRAD_DIR_VER);
        lv_obj_set_style_border_color(powerButton, color(C_RED_1), LV_PART_MAIN);
        lv_obj_set_style_text_color(powerIcon, color(C_RED_1), LV_PART_MAIN);
        lv_obj_set_style_shadow_color(powerButton, color(C_RED_2), LV_PART_MAIN);
    }
}

void createDashboard() {
    dashboardScreen = lv_obj_create(nullptr);
    resetObject(dashboardScreen);
    setOpaqueBg(dashboardScreen, C_BLACK, C_BLACK, LV_GRAD_DIR_NONE);

    createHeader(dashboardScreen);
    createPowerPanel(dashboardScreen);
    createModePanel(dashboardScreen);
    createFanPanel(dashboardScreen);
}

void renderModel(bool force) {
    const uint32_t revision = displayStateRevision();
    const AM6WirelessUiState linkState = wirelessLinkUiState();
    if (!force && revision == lastRevision && linkState == lastLinkState) return;

    lastRevision = revision;
    lastLinkState = linkState;

    const AM6DisplayState state = displayStateGet();
    updatePower(state);
    updateLinkStatus(linkState);

    setButtonState(coolButton, coolLabel, coolIconHost,
                   state.mode == AM6_MODE_COOL,
                   C_COOL_1, C_COOL_2, false);
    setButtonState(heatButton, heatLabel, heatIconHost,
                   state.mode == AM6_MODE_HEAT,
                   C_GREEN_1, C_GREEN_2, false);
    setButtonState(lowButton, lowLabel, lowIconHost,
                   state.fanSpeed == AM6_FAN_LOW,
                   C_COOL_1, C_COOL_2, true);
    setButtonState(mediumButton, mediumLabel, mediumIconHost,
                   state.fanSpeed == AM6_FAN_MEDIUM,
                   C_GREEN_1, C_GREEN_2, true);
    setButtonState(highButton, highLabel, highIconHost,
                   state.fanSpeed == AM6_FAN_HIGH,
                   C_COOL_1, C_COOL_2, true);
}

void serviceSplash() {
    if (dashboardLoaded || !splashScreen) return;

    const uint32_t elapsed = millis() - splashStartedMs;
    const uint32_t bounded = elapsed > AM6_SPLASH_DURATION_MS
                                 ? AM6_SPLASH_DURATION_MS
                                 : elapsed;
    const int value = static_cast<int>((bounded * 100UL) / AM6_SPLASH_DURATION_MS);
    lv_bar_set_value(splashBar, value, LV_ANIM_OFF);

    if (elapsed >= AM6_SPLASH_DURATION_MS) {
        dashboardLoaded = true;
        renderModel(true);
        lv_scr_load_anim(dashboardScreen, LV_SCR_LOAD_ANIM_FADE_ON, 180, 0, false);
    }
}

} // namespace

void uiBegin() {
    createSplash();
    createDashboard();
    splashStartedMs = millis();
    lastModelRefreshMs = 0;
    lastRevision = 0;
    dashboardLoaded = false;
    lv_scr_load(splashScreen);
}

void uiLoop() {
    serviceSplash();

    if (dashboardLoaded &&
        millis() - lastModelRefreshMs >= AM6_UI_MODEL_REFRESH_MS) {
        lastModelRefreshMs = millis();
        renderModel(false);
    }
}
