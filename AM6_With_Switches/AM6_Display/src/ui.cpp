#include "ui.h"

#include <Arduino.h>
#include <stdio.h>
#include <lvgl.h>

#include "config.h"
#include "display_state.h"
#include "wireless_link.h"

namespace {

constexpr uint32_t C_BG_TOP        = 0x051019;
constexpr uint32_t C_BG_BOTTOM     = 0x000406;
constexpr uint32_t C_PANEL_TOP     = 0x102131;
constexpr uint32_t C_PANEL_BOTTOM  = 0x08131E;
constexpr uint32_t C_PANEL_ALT_TOP = 0x0A1A28;
constexpr uint32_t C_PANEL_ALT_BOT = 0x05111A;
constexpr uint32_t C_BORDER        = 0x1AA9FF;
constexpr uint32_t C_BORDER_SOFT   = 0x0D5F95;
constexpr uint32_t C_CYAN          = 0x4CE4FF;
constexpr uint32_t C_CYAN_SOFT     = 0xA8F4FF;
constexpr uint32_t C_TEXT          = 0xF5FAFF;
constexpr uint32_t C_MUTED         = 0x87A6BE;
constexpr uint32_t C_GREEN         = 0x3DD884;
constexpr uint32_t C_GREEN_SOFT    = 0xC3FFE0;
constexpr uint32_t C_RED           = 0xFF6D7A;
constexpr uint32_t C_RED_SOFT      = 0xFFD0D5;
constexpr uint32_t C_AMBER         = 0xFFC657;
constexpr uint32_t C_BLUE          = 0x62B8FF;
constexpr uint32_t C_GRAY          = 0x6E8397;

lv_obj_t* splashScreen = nullptr;
lv_obj_t* splashBar = nullptr;
lv_obj_t* splashPercentLabel = nullptr;
lv_obj_t* dashboardScreen = nullptr;
lv_obj_t* linkChip = nullptr;
lv_obj_t* linkLabel = nullptr;

lv_obj_t* pageHome = nullptr;
lv_obj_t* pageOutdoor = nullptr;
lv_obj_t* pageAlerts = nullptr;
lv_obj_t* navHome = nullptr;
lv_obj_t* navOutdoor = nullptr;
lv_obj_t* navAlerts = nullptr;

lv_obj_t* homePowerValue = nullptr;
lv_obj_t* homeModeValue = nullptr;
lv_obj_t* homeFanValue = nullptr;
lv_obj_t* homeStatusA = nullptr;
lv_obj_t* homeStatusB = nullptr;
lv_obj_t* homeRelayA = nullptr;
lv_obj_t* homeRelayB = nullptr;

lv_obj_t* switchStateLabel[8] = {};
lv_obj_t* relayStateLabel[4] = {};
lv_obj_t* alertStatusA = nullptr;
lv_obj_t* alertStatusB = nullptr;
lv_obj_t* alertDetailA = nullptr;
lv_obj_t* alertDetailB = nullptr;

uint8_t activePage = 0;
uint32_t splashStartedMs = 0;
uint32_t lastModelRefreshMs = 0;
uint32_t lastRevision = 0;
bool dashboardLoaded = false;
AM6WirelessUiState lastLinkState = AM6_LINK_OFFLINE;

lv_color_t color(uint32_t hex) {
    return lv_color_hex(hex);
}

void baseObject(lv_obj_t* obj) {
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(obj, 0, LV_PART_MAIN);
}

void setBg(lv_obj_t* obj, uint32_t top, uint32_t bottom, lv_grad_dir_t dir = LV_GRAD_DIR_VER) {
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, color(top), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(obj, color(bottom), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(obj, dir, LV_PART_MAIN);
}

void stylePanel(lv_obj_t* obj, int radius = 12, bool alt = false) {
    baseObject(obj);
    setBg(obj,
          alt ? C_PANEL_ALT_TOP : C_PANEL_TOP,
          alt ? C_PANEL_ALT_BOT : C_PANEL_BOTTOM,
          LV_GRAD_DIR_VER);
    lv_obj_set_style_radius(obj, radius, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, color(C_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(obj, 10, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(obj, color(C_BORDER_SOFT), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_shadow_ofs_y(obj, 1, LV_PART_MAIN);
}

lv_obj_t* label(lv_obj_t* parent, const char* text, const lv_font_t* font,
                uint32_t col = C_TEXT) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, color(col), LV_PART_MAIN);
    return l;
}

void applyValueColor(lv_obj_t* lbl, uint32_t col) {
    lv_obj_set_style_text_color(lbl, color(col), LV_PART_MAIN);
}

void addPanelHeaderAccent(lv_obj_t* parent, int width, const char* title) {
    lv_obj_t* tag = label(parent, title, &lv_font_montserrat_12, C_CYAN);
    lv_obj_align(tag, LV_ALIGN_TOP_LEFT, 10, 8);

    lv_obj_t* line = lv_obj_create(parent);
    lv_obj_set_pos(line, 10, 28);
    lv_obj_set_size(line, width - 20, 1);
    baseObject(line);
    lv_obj_set_style_bg_color(line, color(C_BORDER_SOFT), LV_PART_MAIN);
}

lv_obj_t* createValueCard(lv_obj_t* parent, int x, int y, int w,
                          const char* title, lv_obj_t** valueOut,
                          const lv_font_t* valueFont = &lv_font_montserrat_20) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, 54);
    stylePanel(card, 10, true);

    lv_obj_t* t = label(card, title, &lv_font_montserrat_12, C_MUTED);
    lv_obj_set_width(t, w - 16);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 6);

    lv_obj_t* accent = lv_obj_create(card);
    lv_obj_set_pos(accent, 10, 24);
    lv_obj_set_size(accent, w - 20, 1);
    baseObject(accent);
    lv_obj_set_style_bg_color(accent, color(C_BORDER_SOFT), LV_PART_MAIN);

    *valueOut = label(card, "---", valueFont, C_TEXT);
    lv_obj_set_width(*valueOut, w - 12);
    lv_label_set_long_mode(*valueOut, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(*valueOut, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(*valueOut, LV_ALIGN_BOTTOM_MID, 0, -5);
    return card;
}

const char* fanText(uint8_t fan) {
    if (fan == AM6_FAN_MEDIUM) return "MEDIUM";
    if (fan == AM6_FAN_HIGH) return "HIGH";
    return "LOW";
}

const char* statusText(uint8_t status) {
    switch (status) {
        case AM6_OUT_RUNNING: return "RUNNING";
        case AM6_OUT_HIGH_PSI: return "HIGH PSI";
        case AM6_OUT_LOW_PSI: return "LOW PSI";
        case AM6_OUT_POWER_FAULT: return "POWER FAULT";
        case AM6_OUT_OVERLOAD: return "OVERLOAD";
        case AM6_OUT_STOPPED:
        default: return "STOPPED";
    }
}

uint32_t statusColor(uint8_t status) {
    switch (status) {
        case AM6_OUT_RUNNING: return C_GREEN;
        case AM6_OUT_HIGH_PSI:
        case AM6_OUT_LOW_PSI:
        case AM6_OUT_POWER_FAULT:
        case AM6_OUT_OVERLOAD: return C_RED;
        case AM6_OUT_STOPPED:
        default: return C_GRAY;
    }
}

const char* statusDetail(uint8_t status) {
    switch (status) {
        case AM6_OUT_RUNNING: return "All protection inputs healthy";
        case AM6_OUT_HIGH_PSI: return "High pressure protection open";
        case AM6_OUT_LOW_PSI: return "Low pressure protection open";
        case AM6_OUT_POWER_FAULT: return "Outdoor power or relay fault";
        case AM6_OUT_OVERLOAD: return "Overload / compressor tripped";
        case AM6_OUT_STOPPED:
        default: return "Circuit stopped / system disabled";
    }
}

void navEvent(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_PRESSED) return;
    activePage = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));

    if (activePage == 0) {
        lv_obj_clear_flag(pageHome, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pageOutdoor, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pageAlerts, LV_OBJ_FLAG_HIDDEN);
    } else if (activePage == 1) {
        lv_obj_add_flag(pageHome, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(pageOutdoor, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pageAlerts, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(pageHome, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pageOutdoor, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(pageAlerts, LV_OBJ_FLAG_HIDDEN);
    }

    auto styleNav = [](lv_obj_t* btn, bool selected) {
        setBg(btn, selected ? C_PANEL_TOP : C_PANEL_ALT_TOP,
                 selected ? 0x143349 : C_PANEL_ALT_BOT, LV_GRAD_DIR_VER);
        lv_obj_set_style_border_color(btn, color(selected ? C_CYAN : C_BORDER), LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(btn, selected ? LV_OPA_30 : LV_OPA_0, LV_PART_MAIN);
    };

    styleNav(navHome, activePage == 0);
    styleNav(navOutdoor, activePage == 1);
    styleNav(navAlerts, activePage == 2);
}

lv_obj_t* createNavButton(lv_obj_t* parent, int x, int w, const char* text, uint8_t page) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_pos(btn, x, 205);
    lv_obj_set_size(btn, w, 31);
    stylePanel(btn, 8, true);
    lv_obj_add_event_cb(btn, navEvent, LV_EVENT_PRESSED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(page)));
    lv_obj_t* t = label(btn, text, &lv_font_montserrat_14, C_TEXT);
    lv_obj_center(t);
    return btn;
}

void createHeader(lv_obj_t* parent) {
    lv_obj_t* header = lv_obj_create(parent);
    lv_obj_set_pos(header, 4, 4);
    lv_obj_set_size(header, 312, 36);
    stylePanel(header, 12);

    lv_obj_t* logo = label(header, "AM6", &lv_font_montserrat_24, C_TEXT);
    lv_obj_align(logo, LV_ALIGN_LEFT_MID, 8, -2);

    lv_obj_t* sub = label(header, "MONITORING", &lv_font_montserrat_12, C_MUTED);
    lv_obj_align(sub, LV_ALIGN_LEFT_MID, 63, 3);

    lv_obj_t* accent = lv_obj_create(header);
    lv_obj_set_pos(accent, 62, 22);
    lv_obj_set_size(accent, 68, 2);
    baseObject(accent);
    lv_obj_set_style_bg_color(accent, color(C_BORDER), LV_PART_MAIN);

    linkChip = lv_obj_create(header);
    lv_obj_set_size(linkChip, 82, 24);
    lv_obj_align(linkChip, LV_ALIGN_RIGHT_MID, -6, 0);
    stylePanel(linkChip, 12, true);

    linkLabel = label(linkChip, "OFFLINE", &lv_font_montserrat_14, C_TEXT);
    lv_obj_center(linkLabel);
}

void createHomePage(lv_obj_t* parent) {
    pageHome = lv_obj_create(parent);
    lv_obj_set_pos(pageHome, 0, 42);
    lv_obj_set_size(pageHome, 320, 160);
    baseObject(pageHome);
    lv_obj_set_style_bg_opa(pageHome, LV_OPA_TRANSP, LV_PART_MAIN);

    createValueCard(pageHome, 4, 2, 100, "POWER", &homePowerValue);
    createValueCard(pageHome, 110, 2, 100, "MODE", &homeModeValue);
    createValueCard(pageHome, 216, 2, 100, "FAN SPEED", &homeFanValue, &lv_font_montserrat_16);

    lv_obj_t* a = lv_obj_create(pageHome);
    lv_obj_set_pos(a, 4, 62);
    lv_obj_set_size(a, 153, 94);
    stylePanel(a, 12);
    addPanelHeaderAccent(a, 153, "CIRCUIT A");
    homeStatusA = label(a, "STOPPED", &lv_font_montserrat_20, C_GRAY);
    lv_obj_align(homeStatusA, LV_ALIGN_CENTER, 0, -4);
    homeRelayA = label(a, "R1 OFF   R3 OFF", &lv_font_montserrat_12, C_MUTED);
    lv_obj_align(homeRelayA, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_obj_t* b = lv_obj_create(pageHome);
    lv_obj_set_pos(b, 163, 62);
    lv_obj_set_size(b, 153, 94);
    stylePanel(b, 12);
    addPanelHeaderAccent(b, 153, "CIRCUIT B");
    homeStatusB = label(b, "STOPPED", &lv_font_montserrat_20, C_GRAY);
    lv_obj_align(homeStatusB, LV_ALIGN_CENTER, 0, -4);
    homeRelayB = label(b, "R2 OFF   R4 OFF", &lv_font_montserrat_12, C_MUTED);
    lv_obj_align(homeRelayB, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void createSwitchRow(lv_obj_t* parent, int y, const char* name, uint8_t index) {
    lv_obj_t* n = label(parent, name, &lv_font_montserrat_12, C_MUTED);
    lv_obj_set_pos(n, 10, y);
    switchStateLabel[index] = label(parent, "---", &lv_font_montserrat_12, C_GRAY);
    lv_obj_align(switchStateLabel[index], LV_ALIGN_TOP_RIGHT, -10, y);
}

void createOutdoorCircuitPanel(lv_obj_t* parent, int x, const char* title,
                               uint8_t baseSwitch, uint8_t relayA, uint8_t relayB) {
    lv_obj_t* p = lv_obj_create(parent);
    lv_obj_set_pos(p, x, 2);
    lv_obj_set_size(p, 153, 156);
    stylePanel(p, 12);

    addPanelHeaderAccent(p, 153, title);

    createSwitchRow(p, 36, "HIGH PSI", baseSwitch + 0);
    createSwitchRow(p, 58, "LOW PSI", baseSwitch + 1);
    createSwitchRow(p, 80, "OVERLOAD", baseSwitch + 2);
    createSwitchRow(p, 102, "POWER", baseSwitch + 3);

    relayStateLabel[relayA] = label(p, "R? OFF", &lv_font_montserrat_12, C_MUTED);
    lv_obj_set_pos(relayStateLabel[relayA], 12, 132);
    relayStateLabel[relayB] = label(p, "R? OFF", &lv_font_montserrat_12, C_MUTED);
    lv_obj_align(relayStateLabel[relayB], LV_ALIGN_TOP_RIGHT, -12, 132);
}

void createOutdoorPage(lv_obj_t* parent) {
    pageOutdoor = lv_obj_create(parent);
    lv_obj_set_pos(pageOutdoor, 0, 42);
    lv_obj_set_size(pageOutdoor, 320, 160);
    baseObject(pageOutdoor);
    lv_obj_set_style_bg_opa(pageOutdoor, LV_OPA_TRANSP, LV_PART_MAIN);

    createOutdoorCircuitPanel(pageOutdoor, 4, "CIRCUIT A", 0, 0, 2);
    createOutdoorCircuitPanel(pageOutdoor, 163, "CIRCUIT B", 4, 1, 3);
    lv_obj_add_flag(pageOutdoor, LV_OBJ_FLAG_HIDDEN);
}

void createAlertCard(lv_obj_t* parent, int y, const char* title,
                     lv_obj_t** statusOut, lv_obj_t** detailOut) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_pos(card, 4, y);
    lv_obj_set_size(card, 312, 74);
    stylePanel(card, 12);

    lv_obj_t* t = label(card, title, &lv_font_montserrat_12, C_CYAN);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 12, 7);

    // Clean horizontal accent: avoids the clipped/uneven vertical line that
    // was visible against the rounded left edge on the real panel.
    lv_obj_t* accent = lv_obj_create(card);
    lv_obj_set_pos(accent, 12, 25);
    lv_obj_set_size(accent, 64, 2);
    baseObject(accent);
    setBg(accent, C_BORDER, C_CYAN, LV_GRAD_DIR_HOR);
    lv_obj_set_style_radius(accent, 2, LV_PART_MAIN);

    *statusOut = label(card, "STOPPED", &lv_font_montserrat_20, C_GRAY);
    lv_obj_align(*statusOut, LV_ALIGN_LEFT_MID, 12, 5);
    *detailOut = label(card, "Circuit stopped", &lv_font_montserrat_12, C_MUTED);
    lv_obj_set_width(*detailOut, 286);
    lv_label_set_long_mode(*detailOut, LV_LABEL_LONG_CLIP);
    lv_obj_align(*detailOut, LV_ALIGN_BOTTOM_LEFT, 12, -7);
}

void createAlertsPage(lv_obj_t* parent) {
    pageAlerts = lv_obj_create(parent);
    lv_obj_set_pos(pageAlerts, 0, 42);
    lv_obj_set_size(pageAlerts, 320, 160);
    baseObject(pageAlerts);
    lv_obj_set_style_bg_opa(pageAlerts, LV_OPA_TRANSP, LV_PART_MAIN);

    createAlertCard(pageAlerts, 2, "CIRCUIT A ALERT", &alertStatusA, &alertDetailA);
    createAlertCard(pageAlerts, 82, "CIRCUIT B ALERT", &alertStatusB, &alertDetailB);
    lv_obj_add_flag(pageAlerts, LV_OBJ_FLAG_HIDDEN);
}

void createDashboard() {
    dashboardScreen = lv_obj_create(nullptr);
    baseObject(dashboardScreen);
    setBg(dashboardScreen, C_BG_TOP, C_BG_BOTTOM);

    createHeader(dashboardScreen);
    createHomePage(dashboardScreen);
    createOutdoorPage(dashboardScreen);
    createAlertsPage(dashboardScreen);

    navHome = createNavButton(dashboardScreen, 4, 100, "HOME", 0);
    navOutdoor = createNavButton(dashboardScreen, 110, 100, "OUTDOOR", 1);
    navAlerts = createNavButton(dashboardScreen, 216, 100, "ALERTS", 2);
    lv_obj_set_style_border_color(navHome, color(C_CYAN), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(navHome, LV_OPA_30, LV_PART_MAIN);
}

void createSplash() {
    splashScreen = lv_obj_create(nullptr);
    baseObject(splashScreen);
    setBg(splashScreen, C_BG_TOP, C_BG_BOTTOM);

    lv_obj_t* frame = lv_obj_create(splashScreen);
    lv_obj_set_pos(frame, 20, 30);
    lv_obj_set_size(frame, 280, 176);
    stylePanel(frame, 18);

    lv_obj_t* title = label(frame, "AM6", &lv_font_montserrat_32, C_TEXT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    lv_obj_t* accent = lv_obj_create(frame);
    lv_obj_set_size(accent, 96, 3);
    lv_obj_align(accent, LV_ALIGN_TOP_MID, 0, 55);
    baseObject(accent);
    setBg(accent, C_BORDER, C_CYAN, LV_GRAD_DIR_HOR);
    lv_obj_set_style_radius(accent, 3, LV_PART_MAIN);

    lv_obj_t* sub1 = label(frame, "ALERT MASTER 6", &lv_font_montserrat_14, C_CYAN_SOFT);
    lv_obj_align(sub1, LV_ALIGN_TOP_MID, 0, 68);

    lv_obj_t* sub2 = label(frame, "MONITORING DISPLAY", &lv_font_montserrat_12, C_MUTED);
    lv_obj_align(sub2, LV_ALIGN_TOP_MID, 0, 89);

    lv_obj_t* boot = label(frame, "INITIALIZING SYSTEM", &lv_font_montserrat_12, C_MUTED);
    lv_obj_align(boot, LV_ALIGN_TOP_MID, 0, 112);

    splashBar = lv_bar_create(frame);
    lv_obj_set_size(splashBar, 190, 9);
    lv_obj_align(splashBar, LV_ALIGN_TOP_MID, 0, 133);
    lv_bar_set_range(splashBar, 0, 100);
    lv_bar_set_value(splashBar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_opa(splashBar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(splashBar, color(C_PANEL_ALT_TOP), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(splashBar, color(C_PANEL_ALT_BOT), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(splashBar, LV_GRAD_DIR_HOR, LV_PART_MAIN);
    lv_obj_set_style_border_color(splashBar, color(C_BORDER_SOFT), LV_PART_MAIN);
    lv_obj_set_style_border_width(splashBar, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(splashBar, 5, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(splashBar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(splashBar, color(C_BORDER), LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(splashBar, color(C_CYAN), LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_dir(splashBar, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
    lv_obj_set_style_radius(splashBar, 5, LV_PART_INDICATOR);

    splashPercentLabel = label(frame, "0%", &lv_font_montserrat_12, C_CYAN_SOFT);
    lv_obj_align(splashPercentLabel, LV_ALIGN_TOP_MID, 0, 148);
}

void updateLink(AM6WirelessUiState linkState) {
    const char* text = "OFFLINE";
    uint32_t textCol = C_RED_SOFT;
    uint32_t borderCol = C_RED;
    uint32_t bgTop = 0x31141A;
    uint32_t bgBottom = 0x1B0B0F;

    if (linkState == AM6_LINK_WAITING) {
        text = "WAIT";
        textCol = C_AMBER;
        borderCol = C_AMBER;
        bgTop = 0x35250A;
        bgBottom = 0x1A1204;
    } else if (linkState == AM6_LINK_ONLINE) {
        text = "ONLINE";
        textCol = C_GREEN_SOFT;
        borderCol = C_GREEN;
        bgTop = 0x10301F;
        bgBottom = 0x08150E;
    } else if (linkState == AM6_LINK_SYNCING) {
        text = "SYNC";
        textCol = C_CYAN_SOFT;
        borderCol = C_CYAN;
        bgTop = 0x0A2B3B;
        bgBottom = 0x05151D;
    }

    lv_label_set_text(linkLabel, text);
    applyValueColor(linkLabel, textCol);
    setBg(linkChip, bgTop, bgBottom, LV_GRAD_DIR_VER);
    lv_obj_set_style_border_color(linkChip, color(borderCol), LV_PART_MAIN);
}

void updateCircuitLabels(const AM6DisplayState& state) {
    lv_label_set_text(homeStatusA, statusText(state.statusA));
    applyValueColor(homeStatusA, statusColor(state.statusA));
    lv_label_set_text(homeStatusB, statusText(state.statusB));
    applyValueColor(homeStatusB, statusColor(state.statusB));

    char buf[40];
    snprintf(buf, sizeof(buf), "R1 %s   R3 %s", state.outdoorRelay[0] ? "ON" : "OFF",
             state.outdoorRelay[2] ? "ON" : "OFF");
    lv_label_set_text(homeRelayA, buf);
    applyValueColor(homeRelayA, (state.outdoorRelay[0] || state.outdoorRelay[2]) ? C_GREEN : C_MUTED);

    snprintf(buf, sizeof(buf), "R2 %s   R4 %s", state.outdoorRelay[1] ? "ON" : "OFF",
             state.outdoorRelay[3] ? "ON" : "OFF");
    lv_label_set_text(homeRelayB, buf);
    applyValueColor(homeRelayB, (state.outdoorRelay[1] || state.outdoorRelay[3]) ? C_GREEN : C_MUTED);
}

void updateOutdoorPage(const AM6DisplayState& state, AM6WirelessUiState linkState) {
    const bool haveLiveData = (linkState == AM6_LINK_ONLINE || linkState == AM6_LINK_SYNCING);

    for (uint8_t i = 0; i < 8; ++i) {
        if (!haveLiveData) {
            lv_label_set_text(switchStateLabel[i], "--");
            applyValueColor(switchStateLabel[i], C_MUTED);
        } else {
            lv_label_set_text(switchStateLabel[i], state.outdoorSwitch[i] ? "OK" : "FAULT");
            applyValueColor(switchStateLabel[i], state.outdoorSwitch[i] ? C_GREEN : C_RED);
        }
    }

    static const char* relayNames[4] = {"R1", "R2", "R3", "R4"};
    char buf[16];
    for (uint8_t i = 0; i < 4; ++i) {
        if (!haveLiveData) {
            snprintf(buf, sizeof(buf), "%s --", relayNames[i]);
            lv_label_set_text(relayStateLabel[i], buf);
            applyValueColor(relayStateLabel[i], C_MUTED);
        } else {
            snprintf(buf, sizeof(buf), "%s %s", relayNames[i], state.outdoorRelay[i] ? "ON" : "OFF");
            lv_label_set_text(relayStateLabel[i], buf);
            applyValueColor(relayStateLabel[i], state.outdoorRelay[i] ? C_GREEN : C_MUTED);
        }
    }
}

void updateAlerts(const AM6DisplayState& state, AM6WirelessUiState linkState) {
    const bool haveLiveData = (linkState == AM6_LINK_ONLINE || linkState == AM6_LINK_SYNCING);
    if (!haveLiveData) {
        lv_label_set_text(alertStatusA, "NO DATA");
        applyValueColor(alertStatusA, C_GRAY);
        lv_label_set_text(alertDetailA, "Waiting for Indoor monitoring link");
        lv_label_set_text(alertStatusB, "NO DATA");
        applyValueColor(alertStatusB, C_GRAY);
        lv_label_set_text(alertDetailB, "Waiting for Indoor monitoring link");
        return;
    }

    lv_label_set_text(alertStatusA, statusText(state.statusA));
    applyValueColor(alertStatusA, statusColor(state.statusA));
    lv_label_set_text(alertDetailA, statusDetail(state.statusA));

    lv_label_set_text(alertStatusB, statusText(state.statusB));
    applyValueColor(alertStatusB, statusColor(state.statusB));
    lv_label_set_text(alertDetailB, statusDetail(state.statusB));

    if (!state.outdoorOnline && state.systemPower == 1 && state.outdoorEnabled) {
        lv_label_set_text(alertDetailA, "Outdoor RS485 link unavailable");
        lv_label_set_text(alertDetailB, "Outdoor RS485 link unavailable");
    }
}

void renderModel(bool force) {
    const uint32_t revision = displayStateRevision();
    const AM6WirelessUiState linkState = wirelessLinkUiState();
    if (!force && revision == lastRevision && linkState == lastLinkState) return;

    lastRevision = revision;
    lastLinkState = linkState;
    const AM6DisplayState state = displayStateGet();

    updateLink(linkState);

    const bool haveLiveData = (linkState == AM6_LINK_ONLINE || linkState == AM6_LINK_SYNCING);
    if (haveLiveData) {
        lv_label_set_text(homePowerValue, state.systemPower ? "ON" : "OFF");
        applyValueColor(homePowerValue, state.systemPower ? C_GREEN : C_RED);

        lv_label_set_text(homeModeValue, state.mode == AM6_MODE_HEAT ? "HEAT" : "COOL");
        applyValueColor(homeModeValue, state.mode == AM6_MODE_HEAT ? C_AMBER : C_BLUE);

        lv_label_set_text(homeFanValue, fanText(state.fanSpeed));
        applyValueColor(homeFanValue, C_CYAN);
    } else {
        lv_label_set_text(homePowerValue, "---");
        lv_label_set_text(homeModeValue, "---");
        lv_label_set_text(homeFanValue, "---");
        applyValueColor(homePowerValue, C_MUTED);
        applyValueColor(homeModeValue, C_MUTED);
        applyValueColor(homeFanValue, C_MUTED);
    }

    if (haveLiveData) {
        updateCircuitLabels(state);
    } else {
        lv_label_set_text(homeStatusA, "NO DATA");
        applyValueColor(homeStatusA, C_GRAY);
        lv_label_set_text(homeStatusB, "NO DATA");
        applyValueColor(homeStatusB, C_GRAY);
        lv_label_set_text(homeRelayA, "R1 --   R3 --");
        lv_label_set_text(homeRelayB, "R2 --   R4 --");
        applyValueColor(homeRelayA, C_MUTED);
        applyValueColor(homeRelayB, C_MUTED);
    }
    updateOutdoorPage(state, linkState);
    updateAlerts(state, linkState);
}

void serviceSplash() {
    if (dashboardLoaded || !splashScreen) return;

    const uint32_t elapsed = millis() - splashStartedMs;
    const uint32_t bounded = elapsed > AM6_SPLASH_DURATION_MS
                                 ? AM6_SPLASH_DURATION_MS
                                 : elapsed;
    const int progress = static_cast<int>((bounded * 100UL) / AM6_SPLASH_DURATION_MS);
    lv_bar_set_value(splashBar, progress, LV_ANIM_OFF);
    if (splashPercentLabel) {
        char progressText[8];
        snprintf(progressText, sizeof(progressText), "%d%%", progress);
        lv_label_set_text(splashPercentLabel, progressText);
    }

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
    activePage = 0;
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
