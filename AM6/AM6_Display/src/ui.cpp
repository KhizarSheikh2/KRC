#include "ui.h"

#include <Arduino.h>
#include <lvgl.h>

#include "config.h"
#include "display_state.h"
#include "wireless_link.h"

namespace {

// ============================================================
// AM6 LIGHT INDUSTRIAL UI
// High-contrast palette for a white/light background so text,
// state colors, and header status remain clearly visible.
// ============================================================
constexpr uint32_t C_BG            = 0xF3F7FA;
constexpr uint32_t C_BG_2          = 0xE8EFF5;
constexpr uint32_t C_HEADER        = 0x0E2A43;
constexpr uint32_t C_HEADER_BORDER = 0x173A59;
constexpr uint32_t C_HEADER_ACCENT = 0x12B5EA;

constexpr uint32_t C_CARD          = 0xFFFFFF;
constexpr uint32_t C_SURFACE       = 0xF7FAFC;
constexpr uint32_t C_SURFACE_2     = 0xEDF3F8;
constexpr uint32_t C_BORDER        = 0xC8D6E5;
constexpr uint32_t C_BORDER_SOFT   = 0xDCE6EE;

constexpr uint32_t C_TEXT          = 0x102A43;
constexpr uint32_t C_TEXT_2        = 0x334E68;
constexpr uint32_t C_TEXT_MUTED    = 0x627D98;

constexpr uint32_t C_FAN           = 0x0B8F8C;
constexpr uint32_t C_FAN_SOFT      = 0xE6F8F7;
constexpr uint32_t C_ON            = 0x1F9D63;
constexpr uint32_t C_ON_SOFT       = 0xE9F8EF;
constexpr uint32_t C_OFF           = 0xD64545;
constexpr uint32_t C_OFF_SOFT      = 0xFDECEC;
constexpr uint32_t C_COOL          = 0x1976F3;
constexpr uint32_t C_COOL_SOFT     = 0xEAF3FF;
constexpr uint32_t C_HEAT          = 0xD38A12;
constexpr uint32_t C_HEAT_SOFT     = 0xFFF4DF;
constexpr uint32_t C_WAIT          = 0xB7791F;
constexpr uint32_t C_WAIT_SOFT     = 0xFCF3E1;
constexpr uint32_t C_SYNC          = 0x0E86C8;
constexpr uint32_t C_SYNC_SOFT     = 0xE8F4FB;

lv_obj_t* splashScreen = nullptr;
lv_obj_t* splashBar = nullptr;
lv_obj_t* dashboardScreen = nullptr;

lv_obj_t* statusChip = nullptr;
lv_obj_t* statusDot = nullptr;
lv_obj_t* statusLabel = nullptr;

lv_obj_t* powerButton = nullptr;
lv_obj_t* powerIcon = nullptr;
lv_obj_t* powerStatePill = nullptr;
lv_obj_t* powerStateLabel = nullptr;
lv_obj_t* powerHintLabel = nullptr;

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

uint32_t splashStartedMs = 0;
uint32_t lastModelRefreshMs = 0;
uint32_t lastRevision = 0;
bool dashboardLoaded = false;
AM6WirelessUiState lastLinkState = AM6_LINK_OFFLINE;

lv_color_t color(uint32_t hex) {
    return lv_color_hex(hex);
}

void noScroll(lv_obj_t* obj) {
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_outline_width(obj, 0, LV_PART_MAIN);
}

void baseObject(lv_obj_t* obj) {
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(obj, 0, LV_PART_MAIN);
    noScroll(obj);
}

void styleCard(lv_obj_t* obj) {
    baseObject(obj);
    lv_obj_set_style_radius(obj, 12, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, color(C_CARD), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_NONE, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, color(C_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);
}

void styleSectionTitle(lv_obj_t* label) {
    lv_obj_set_style_text_color(label, color(C_TEXT_2), LV_PART_MAIN);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(label, 1, LV_PART_MAIN);
}

lv_obj_t* createButtonText(lv_obj_t* btn, const char* text) {
    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(label, lv_pct(100));
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color(C_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_center(label);
    return label;
}

void styleControlButton(lv_obj_t* btn) {
    baseObject(btn);
    lv_obj_set_style_radius(btn, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, color(C_SURFACE), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_NONE, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, color(C_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
}

void setButtonSelected(lv_obj_t* btn,
                       lv_obj_t* label,
                       bool selected,
                       uint32_t accent,
                       uint32_t softFill) {
    if (!btn || !label) return;

    if (selected) {
        lv_obj_set_style_bg_color(btn, color(softFill), LV_PART_MAIN);
        lv_obj_set_style_border_color(btn, color(accent), LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
        lv_obj_set_style_text_color(label, color(accent), LV_PART_MAIN);
    } else {
        lv_obj_set_style_bg_color(btn, color(C_SURFACE), LV_PART_MAIN);
        lv_obj_set_style_border_color(btn, color(C_BORDER), LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
        lv_obj_set_style_text_color(label, color(C_TEXT), LV_PART_MAIN);
    }

    lv_obj_set_style_text_opa(label, LV_OPA_COVER, LV_PART_MAIN);
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

lv_obj_t* createCard(lv_obj_t* parent, int x, int y, int w, int h) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    styleCard(card);
    return card;
}

void createSplash() {
    splashScreen = lv_obj_create(nullptr);
    baseObject(splashScreen);
    lv_obj_set_style_bg_color(splashScreen, color(C_BG_2), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(splashScreen, color(C_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(splashScreen, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_border_width(splashScreen, 0, LV_PART_MAIN);

    lv_obj_t* badge = lv_obj_create(splashScreen);
    lv_obj_set_size(badge, 84, 84);
    lv_obj_align(badge, LV_ALIGN_CENTER, 0, -34);
    baseObject(badge);
    lv_obj_set_style_radius(badge, 18, LV_PART_MAIN);
    lv_obj_set_style_bg_color(badge, color(0xE7F6FB), LV_PART_MAIN);
    lv_obj_set_style_border_color(badge, color(C_HEADER_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_border_width(badge, 2, LV_PART_MAIN);

    lv_obj_t* title = lv_label_create(badge);
    lv_label_set_text(title, "AM6");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, color(C_HEADER_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_text_opa(title, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_center(title);

    lv_obj_t* subtitle = lv_label_create(splashScreen);
    lv_label_set_text(subtitle, "ALERT MASTER 6");
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(subtitle, color(C_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_opa(subtitle, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, 30);

    lv_obj_t* caption = lv_label_create(splashScreen);
    lv_label_set_text(caption, "SYSTEM CONTROL DISPLAY");
    lv_obj_set_style_text_font(caption, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(caption, color(C_TEXT_MUTED), LV_PART_MAIN);
    lv_obj_set_style_text_opa(caption, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(caption, LV_ALIGN_CENTER, 0, 52);

    splashBar = lv_bar_create(splashScreen);
    lv_obj_set_size(splashBar, 148, 6);
    lv_obj_align(splashBar, LV_ALIGN_CENTER, 0, 81);
    lv_bar_set_range(splashBar, 0, 100);
    lv_bar_set_value(splashBar, 0, LV_ANIM_OFF);
    lv_obj_set_style_radius(splashBar, 3, LV_PART_MAIN);
    lv_obj_set_style_bg_color(splashBar, color(C_SURFACE_2), LV_PART_MAIN);
    lv_obj_set_style_radius(splashBar, 3, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(splashBar, color(C_HEADER_ACCENT), LV_PART_INDICATOR);
}

void createHeader(lv_obj_t* parent) {
    lv_obj_t* header = lv_obj_create(parent);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, 320, 42);
    baseObject(header);
    lv_obj_set_style_bg_color(header, color(C_HEADER), LV_PART_MAIN);
    lv_obj_set_style_border_color(header, color(C_HEADER_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 1, LV_PART_MAIN);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);

    lv_obj_t* logo = lv_label_create(header);
    lv_label_set_text(logo, "AM6");
    lv_obj_set_style_text_font(logo, &lv_font_montserrat_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(logo, color(C_HEADER_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_text_opa(logo, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(logo, LV_ALIGN_LEFT_MID, 12, -1);

    lv_obj_t* accent = lv_obj_create(header);
    lv_obj_set_size(accent, 30, 3);
    lv_obj_align(accent, LV_ALIGN_LEFT_MID, 14, 12);
    baseObject(accent);
    lv_obj_set_style_radius(accent, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(accent, color(C_HEADER_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_border_width(accent, 0, LV_PART_MAIN);

    statusChip = lv_obj_create(header);
    lv_obj_set_size(statusChip, 98, 26);
    lv_obj_align(statusChip, LV_ALIGN_RIGHT_MID, -8, 0);
    baseObject(statusChip);
    lv_obj_set_style_radius(statusChip, 13, LV_PART_MAIN);
    lv_obj_set_style_bg_color(statusChip, color(C_SYNC_SOFT), LV_PART_MAIN);
    lv_obj_set_style_border_color(statusChip, color(C_SYNC), LV_PART_MAIN);
    lv_obj_set_style_border_width(statusChip, 1, LV_PART_MAIN);

    statusDot = lv_obj_create(statusChip);
    lv_obj_set_size(statusDot, 8, 8);
    lv_obj_align(statusDot, LV_ALIGN_LEFT_MID, 10, 0);
    baseObject(statusDot);
    lv_obj_set_style_radius(statusDot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(statusDot, color(C_SYNC), LV_PART_MAIN);
    lv_obj_set_style_border_width(statusDot, 0, LV_PART_MAIN);

    statusLabel = lv_label_create(statusChip);
    lv_label_set_text(statusLabel, "SYNC");
    lv_obj_set_style_text_font(statusLabel, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(statusLabel, color(C_SYNC), LV_PART_MAIN);
    lv_obj_set_style_text_opa(statusLabel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(statusLabel, LV_ALIGN_LEFT_MID, 24, 0);
}

void createPowerCard(lv_obj_t* parent) {
    lv_obj_t* card = createCard(parent, 8, 48, 104, 184);

    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, "POWER");
    styleSectionTitle(title);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    powerButton = lv_btn_create(card);
    lv_obj_set_size(powerButton, 74, 74);
    lv_obj_align(powerButton, LV_ALIGN_TOP_MID, 0, 34);
    baseObject(powerButton);
    lv_obj_set_style_radius(powerButton, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(powerButton, color(C_OFF_SOFT), LV_PART_MAIN);
    lv_obj_set_style_border_color(powerButton, color(C_OFF), LV_PART_MAIN);
    lv_obj_set_style_border_width(powerButton, 2, LV_PART_MAIN);
    lv_obj_add_event_cb(powerButton, powerEvent, LV_EVENT_PRESSED, nullptr);

    powerIcon = lv_label_create(powerButton);
    lv_label_set_text(powerIcon, LV_SYMBOL_POWER);
    lv_obj_set_style_text_font(powerIcon, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(powerIcon, color(C_OFF), LV_PART_MAIN);
    lv_obj_set_style_text_opa(powerIcon, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_center(powerIcon);

    powerStatePill = lv_obj_create(card);
    lv_obj_set_size(powerStatePill, 72, 26);
    lv_obj_align(powerStatePill, LV_ALIGN_TOP_MID, 0, 116);
    baseObject(powerStatePill);
    lv_obj_set_style_radius(powerStatePill, 13, LV_PART_MAIN);
    lv_obj_set_style_bg_color(powerStatePill, color(C_OFF_SOFT), LV_PART_MAIN);
    lv_obj_set_style_border_color(powerStatePill, color(C_OFF), LV_PART_MAIN);
    lv_obj_set_style_border_width(powerStatePill, 1, LV_PART_MAIN);

    powerStateLabel = lv_label_create(powerStatePill);
    lv_label_set_text(powerStateLabel, "OFF");
    lv_obj_set_style_text_font(powerStateLabel, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(powerStateLabel, color(C_OFF), LV_PART_MAIN);
    lv_obj_set_style_text_opa(powerStateLabel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_center(powerStateLabel);

    powerHintLabel = lv_label_create(card);
    lv_label_set_text(powerHintLabel, "SYSTEM OFF");
    lv_obj_set_width(powerHintLabel, 90);
    lv_obj_set_style_text_align(powerHintLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(powerHintLabel, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(powerHintLabel, color(C_OFF), LV_PART_MAIN);
    lv_obj_set_style_text_opa(powerHintLabel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(powerHintLabel, LV_ALIGN_BOTTOM_MID, 0, -12);
}

void createModeCard(lv_obj_t* parent) {
    lv_obj_t* card = createCard(parent, 120, 48, 192, 82);

    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, "MODE");
    styleSectionTitle(title);
    lv_obj_set_pos(title, 10, 8);

    coolButton = lv_btn_create(card);
    lv_obj_set_pos(coolButton, 8, 30);
    lv_obj_set_size(coolButton, 84, 44);
    styleControlButton(coolButton);
    coolLabel = createButtonText(coolButton, "COOL");
    lv_obj_add_event_cb(coolButton, modeEvent, LV_EVENT_PRESSED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(AM6_MODE_COOL)));

    heatButton = lv_btn_create(card);
    lv_obj_set_pos(heatButton, 100, 30);
    lv_obj_set_size(heatButton, 84, 44);
    styleControlButton(heatButton);
    heatLabel = createButtonText(heatButton, "HEAT");
    lv_obj_add_event_cb(heatButton, modeEvent, LV_EVENT_PRESSED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(AM6_MODE_HEAT)));
}

void createFanCard(lv_obj_t* parent) {
    lv_obj_t* card = createCard(parent, 120, 136, 192, 96);

    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, "FAN SPEED");
    styleSectionTitle(title);
    lv_obj_set_pos(title, 10, 8);

    struct FanDef {
        lv_obj_t** button;
        lv_obj_t** label;
        int x;
        const char* text;
        uint8_t value;
    } defs[] = {
        {&lowButton, &lowLabel, 8, "LOW", AM6_FAN_LOW},
        {&mediumButton, &mediumLabel, 68, "MED", AM6_FAN_MEDIUM},
        {&highButton, &highLabel, 128, "HIGH", AM6_FAN_HIGH}
    };

    for (const FanDef& def : defs) {
        *def.button = lv_btn_create(card);
        lv_obj_set_pos(*def.button, def.x, 39);
        lv_obj_set_size(*def.button, 56, 46);
        styleControlButton(*def.button);
        *def.label = createButtonText(*def.button, def.text);
        lv_obj_add_event_cb(*def.button, fanEvent, LV_EVENT_PRESSED,
                            reinterpret_cast<void*>(static_cast<uintptr_t>(def.value)));
    }
}

void setControlEnabled(lv_obj_t* obj, bool enabled) {
    if (!obj) return;
    if (enabled) lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    else lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(obj, enabled ? LV_OPA_COVER : LV_OPA_60, LV_PART_MAIN);
}

void updateLinkStatus(AM6WirelessUiState linkState) {
    const char* text = "OFFLINE";
    uint32_t dot = C_OFF;
    uint32_t fill = C_OFF_SOFT;
    uint32_t border = C_OFF;
    uint32_t textColor = C_OFF;

    if (linkState == AM6_LINK_WAITING) {
        text = "WAIT";
        dot = C_WAIT;
        fill = C_WAIT_SOFT;
        border = C_WAIT;
        textColor = C_WAIT;
    } else if (linkState == AM6_LINK_ONLINE) {
        text = "LINK";
        dot = C_ON;
        fill = C_ON_SOFT;
        border = C_ON;
        textColor = C_ON;
    } else if (linkState == AM6_LINK_SYNCING) {
        text = "SYNC";
        dot = C_SYNC;
        fill = C_SYNC_SOFT;
        border = C_SYNC;
        textColor = C_SYNC;
    }

    lv_label_set_text(statusLabel, text);
    lv_obj_set_style_bg_color(statusDot, color(dot), LV_PART_MAIN);
    lv_obj_set_style_bg_color(statusChip, color(fill), LV_PART_MAIN);
    lv_obj_set_style_border_color(statusChip, color(border), LV_PART_MAIN);
    lv_obj_set_style_text_color(statusLabel, color(textColor), LV_PART_MAIN);

    const bool controlsEnabled = (linkState == AM6_LINK_ONLINE ||
                                  linkState == AM6_LINK_SYNCING);
    setControlEnabled(powerButton, controlsEnabled);
    setControlEnabled(coolButton, controlsEnabled);
    setControlEnabled(heatButton, controlsEnabled);
    setControlEnabled(lowButton, controlsEnabled);
    setControlEnabled(mediumButton, controlsEnabled);
    setControlEnabled(highButton, controlsEnabled);
}

void createDashboard() {
    dashboardScreen = lv_obj_create(nullptr);
    baseObject(dashboardScreen);
    lv_obj_set_style_bg_color(dashboardScreen, color(C_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(dashboardScreen, color(C_BG_2), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(dashboardScreen, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_border_width(dashboardScreen, 0, LV_PART_MAIN);

    createHeader(dashboardScreen);
    createPowerCard(dashboardScreen);
    createModeCard(dashboardScreen);
    createFanCard(dashboardScreen);
}

void updatePower(const AM6DisplayState& state, AM6WirelessUiState linkState) {
    const bool on = state.systemPower == 1;
    const uint32_t accent = on ? C_ON : C_OFF;
    const uint32_t fill = on ? C_ON_SOFT : C_OFF_SOFT;

    lv_label_set_text(powerStateLabel, on ? "ON" : "OFF");
    lv_obj_set_style_text_color(powerStateLabel, color(accent), LV_PART_MAIN);

    lv_obj_set_style_bg_color(powerStatePill, color(fill), LV_PART_MAIN);
    lv_obj_set_style_border_color(powerStatePill, color(accent), LV_PART_MAIN);

    lv_obj_set_style_text_color(powerIcon, color(accent), LV_PART_MAIN);
    lv_obj_set_style_bg_color(powerButton, color(fill), LV_PART_MAIN);
    lv_obj_set_style_border_color(powerButton, color(accent), LV_PART_MAIN);

    const char* hint = on ? "SYSTEM ON" : "SYSTEM OFF";
    uint32_t hintColor = accent;

    if (linkState == AM6_LINK_WAITING) {
        hint = "WAIT LINK";
        hintColor = C_WAIT;
    } else if (linkState == AM6_LINK_OFFLINE) {
        hint = "OFFLINE";
        hintColor = C_OFF;
    } else if (linkState == AM6_LINK_SYNCING) {
        hint = "SYNCING";
        hintColor = C_SYNC;
    }

    lv_label_set_text(powerHintLabel, hint);
    lv_obj_set_style_text_color(powerHintLabel, color(hintColor), LV_PART_MAIN);
}

void renderModel(bool force) {
    const uint32_t revision = displayStateRevision();
    const AM6WirelessUiState linkState = wirelessLinkUiState();
    if (!force && revision == lastRevision && linkState == lastLinkState) return;
    lastRevision = revision;
    lastLinkState = linkState;

    const AM6DisplayState state = displayStateGet();
    updatePower(state, linkState);
    updateLinkStatus(linkState);

    setButtonSelected(coolButton, coolLabel,
                      state.mode == AM6_MODE_COOL,
                      C_COOL, C_COOL_SOFT);
    setButtonSelected(heatButton, heatLabel,
                      state.mode == AM6_MODE_HEAT,
                      C_HEAT, C_HEAT_SOFT);
    setButtonSelected(lowButton, lowLabel,
                      state.fanSpeed == AM6_FAN_LOW,
                      C_FAN, C_FAN_SOFT);
    setButtonSelected(mediumButton, mediumLabel,
                      state.fanSpeed == AM6_FAN_MEDIUM,
                      C_FAN, C_FAN_SOFT);
    setButtonSelected(highButton, highLabel,
                      state.fanSpeed == AM6_FAN_HIGH,
                      C_FAN, C_FAN_SOFT);
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
        lv_scr_load_anim(dashboardScreen,
                         LV_SCR_LOAD_ANIM_FADE_ON,
                         180,
                         0,
                         false);
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
