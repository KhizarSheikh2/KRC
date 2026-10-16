#include "UI.h"
#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include "BoardConfig.h"

namespace {

DisplayLink* gLink = nullptr;
uint32_t bootMs = 0;
bool mainBuilt = false;
uint32_t lastUiUpdate = 0;
uint32_t lastRevision = 0;

lv_obj_t* splash = nullptr;
lv_obj_t* headerDevice = nullptr;
lv_obj_t* headerLink = nullptr;
lv_obj_t* pages[4] = {nullptr, nullptr, nullptr, nullptr};
lv_obj_t* navBtns[4] = {nullptr, nullptr, nullptr, nullptr};
int activePage = 0;

lv_obj_t* homeTemps[4] = {nullptr, nullptr, nullptr, nullptr};
lv_obj_t* homeStatus = nullptr;
lv_obj_t* homeStatusSub = nullptr;
lv_obj_t* homeRun = nullptr;
lv_obj_t* homeSetpoint = nullptr;
lv_obj_t* homeMode = nullptr;
lv_obj_t* powerBtn = nullptr;
lv_obj_t* powerBtnLabel = nullptr;

lv_obj_t* controlValues[4] = {nullptr, nullptr, nullptr, nullptr};
lv_obj_t* safetyPressure[3] = {nullptr, nullptr, nullptr};
lv_obj_t* safetyAmps[3] = {nullptr, nullptr, nullptr};
lv_obj_t* safetySwitch[3] = {nullptr, nullptr, nullptr};

lv_obj_t* setupValues[14] = {};

lv_obj_t* editorOverlay = nullptr;
lv_obj_t* editorValueLabel = nullptr;
int32_t editorValue = 0;
int32_t editorMin = 0;
int32_t editorMax = 0;
int32_t editorStep = 1;
uint8_t editorCommand = AM2Proto::CMD_NONE;
char editorUnit[12] = {0};

constexpr uint32_t C_BG = 0x071019;
constexpr uint32_t C_PANEL = 0x0E1B26;
constexpr uint32_t C_PANEL2 = 0x122535;
constexpr uint32_t C_BORDER = 0x244255;
constexpr uint32_t C_TEXT = 0xECF4F8;
constexpr uint32_t C_MUTED = 0x7892A3;
constexpr uint32_t C_CYAN = 0x32C7F4;
constexpr uint32_t C_GREEN = 0x25D58A;
constexpr uint32_t C_AMBER = 0xF2B84B;
constexpr uint32_t C_RED = 0xFF5E70;

struct Action {
  uint8_t command;
  const char* title;
  const char* unit;
  int32_t minValue;
  int32_t maxValue;
  int32_t step;
  bool toggle;
};

Action actions[] = {
  {AM2Proto::CMD_RETURN_SP, "RETURN SETPOINT", " C", 5, 40, 1, false},
  {AM2Proto::CMD_RETURN_ALERT_SP, "RETURN HIGH ALERT", " C", 10, 100, 1, false},
  {AM2Proto::CMD_SUCTION_ALERT_SP, "SUCTION LOW ALERT", " C", -40, 40, 1, false},
  {AM2Proto::CMD_DISCHARGE_ALERT_SP, "DISCHARGE HIGH ALERT", " C", 10, 150, 1, false},
  {AM2Proto::CMD_MODE, "OPERATING MODE", "", 0, 1, 1, true},
  {AM2Proto::CMD_AUTO_RESET, "AUTO RESET", "", 0, 1, 1, true},
  {AM2Proto::CMD_CT_ENABLE, "CURRENT MODULE", "", 0, 1, 1, true},
  {AM2Proto::CMD_SUCTION_PRESSURE_ENABLE, "SUCTION PRESSURE", "", 0, 1, 1, true},
  {AM2Proto::CMD_DISCHARGE_PRESSURE_ENABLE, "DISCHARGE PRESSURE", "", 0, 1, 1, true},
  {AM2Proto::CMD_OIL_PRESSURE_ENABLE, "OIL PRESSURE", "", 0, 1, 1, true},
  {AM2Proto::CMD_LPS_ENABLE, "LPS PROTECTION", "", 0, 1, 1, true},
  {AM2Proto::CMD_HPS_ENABLE, "HPS PROTECTION", "", 0, 1, 1, true},
  {AM2Proto::CMD_OPS_ENABLE, "OPS PROTECTION", "", 0, 1, 1, true},
  {AM2Proto::CMD_RESTART_DELAY, "RESTART DELAY", " s", 0, 600, 5, false},
  {AM2Proto::CMD_SUCTION_PSI_SP, "SUCTION PSI LIMIT", " psi", 0, 1000, 1, false},
  {AM2Proto::CMD_DISCHARGE_PSI_SP, "DISCHARGE PSI LIMIT", " psi", 0, 1500, 5, false},
  {AM2Proto::CMD_OIL_PSI_SP, "OIL PSI LIMIT", " psi", 0, 1000, 1, false},
  {AM2Proto::CMD_AMPS_LIMIT, "MAX CURRENT", " A", 0, 200, 1, false}
};

lv_color_t color(uint32_t hex) { return lv_color_hex(hex); }

void styleBase(lv_obj_t* obj, uint32_t bg = C_PANEL, int radius = 8) {
  lv_obj_set_style_bg_color(obj, color(bg), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(obj, color(C_BORDER), LV_PART_MAIN);
  lv_obj_set_style_radius(obj, radius, LV_PART_MAIN);
  lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN);
}

lv_obj_t* makeLabel(lv_obj_t* parent, const char* text, int x, int y,
                    const lv_font_t* font = &lv_font_montserrat_12,
                    uint32_t textColor = C_TEXT) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_pos(label, x, y);
  lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
  lv_obj_set_style_text_color(label, color(textColor), LV_PART_MAIN);
  return label;
}

lv_obj_t* makeButton(lv_obj_t* parent, int x, int y, int w, int h, const char* text) {
  lv_obj_t* btn = lv_btn_create(parent);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_size(btn, w, h);
  styleBase(btn, C_PANEL2, 7);
  lv_obj_set_style_bg_color(btn, color(C_PANEL2), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_border_color(btn, color(C_CYAN), LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_t* label = lv_label_create(btn);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_set_style_text_color(label, color(C_TEXT), LV_PART_MAIN);
  lv_obj_center(label);
  return btn;
}

void setButtonStateColor(lv_obj_t* btn, uint32_t bg, uint32_t border) {
  lv_obj_set_style_bg_color(btn, color(bg), LV_PART_MAIN);
  lv_obj_set_style_border_color(btn, color(border), LV_PART_MAIN);
}

const char* statusText(uint8_t s) {
  switch (s) {
    case 0: return "STOPPED";
    case 1: return "RUNNING";
    case 2: return "AUTO STOPPED";
    case 4: return "RESTARTING";
    case 5: return "RETURN TEMP HIGH";
    case 6: return "SUCTION TEMP LOW";
    case 7: return "DISCHARGE TEMP HIGH";
    case 8: return "SUCTION PSI LOW";
    case 9: return "DISCHARGE PSI HIGH";
    case 10: return "OIL PSI LOW";
    case 11: return "HIGH CURRENT";
    case 12: return "LOW PRESSURE SWITCH";
    case 13: return "HIGH PRESSURE SWITCH";
    case 14: return "OIL PRESSURE SWITCH";
    case 15: return "COMPRESSOR FAILED";
    case 16: return "TEMP SENSOR ERROR";
    case 17: return "MC STUCK";
    default: return "UNKNOWN";
  }
}

uint32_t statusColor(uint8_t s) {
  if (s == 1) return C_GREEN;
  if (s == 2) return C_CYAN;
  if (s == 4) return C_AMBER;
  if (s >= 5) return C_RED;
  return C_MUTED;
}

void formatTemp(char* out, size_t len, int16_t value10) {
  if (value10 >= 8870) {
    std::snprintf(out, len, "--.- C");
  } else {
    std::snprintf(out, len, "%.1f C", value10 / 10.0f);
  }
}

void showPage(int index) {
  if (index < 0 || index > 3) return;
  activePage = index;
  for (int i = 0; i < 4; ++i) {
    if (pages[i]) {
      if (i == index) lv_obj_clear_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
      else lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (navBtns[i]) {
      setButtonStateColor(navBtns[i], i == index ? 0x173344 : C_PANEL, i == index ? C_CYAN : C_BORDER);
    }
  }
}

void navEvent(lv_event_t* e) {
  const intptr_t index = reinterpret_cast<intptr_t>(lv_event_get_user_data(e));
  showPage(static_cast<int>(index));
}

int32_t currentValueForCommand(uint8_t cmd) {
  if (!gLink) return 0;
  const auto& s = gLink->state();
  switch (cmd) {
    case AM2Proto::CMD_RETURN_SP: return s.returnSp;
    case AM2Proto::CMD_RETURN_ALERT_SP: return s.returnAlertSp;
    case AM2Proto::CMD_SUCTION_ALERT_SP: return s.suctionAlertSp;
    case AM2Proto::CMD_DISCHARGE_ALERT_SP: return s.dischargeAlertSp;
    case AM2Proto::CMD_SUCTION_PSI_SP: return s.suctionPsiSp;
    case AM2Proto::CMD_DISCHARGE_PSI_SP: return s.dischargePsiSp;
    case AM2Proto::CMD_OIL_PSI_SP: return s.oilPsiSp;
    case AM2Proto::CMD_AMPS_LIMIT: return s.ampsLimit;
    case AM2Proto::CMD_MODE: return s.mode;
    case AM2Proto::CMD_AUTO_RESET: return s.autoReset;
    case AM2Proto::CMD_CT_ENABLE: return s.ctEnabled;
    case AM2Proto::CMD_LPS_ENABLE: return s.lpsEnable;
    case AM2Proto::CMD_HPS_ENABLE: return s.hpsEnable;
    case AM2Proto::CMD_OPS_ENABLE: return s.opsEnable;
    case AM2Proto::CMD_SUCTION_PRESSURE_ENABLE: return s.suctionPressureEnable;
    case AM2Proto::CMD_DISCHARGE_PRESSURE_ENABLE: return s.dischargePressureEnable;
    case AM2Proto::CMD_OIL_PRESSURE_ENABLE: return s.oilPressureEnable;
    case AM2Proto::CMD_RESTART_DELAY: return s.restartDelaySec;
    default: return 0;
  }
}

void closeEditor() {
  if (editorOverlay) {
    lv_obj_del(editorOverlay);
    editorOverlay = nullptr;
    editorValueLabel = nullptr;
  }
}

void refreshEditorValue() {
  if (!editorValueLabel) return;
  char buf[48];
  std::snprintf(buf, sizeof(buf), "%ld%s", static_cast<long>(editorValue), editorUnit);
  lv_label_set_text(editorValueLabel, buf);
}

void editorMinus(lv_event_t*) {
  editorValue -= editorStep;
  if (editorValue < editorMin) editorValue = editorMin;
  refreshEditorValue();
}

void editorPlus(lv_event_t*) {
  editorValue += editorStep;
  if (editorValue > editorMax) editorValue = editorMax;
  refreshEditorValue();
}

void editorCancel(lv_event_t*) { closeEditor(); }

void editorApply(lv_event_t*) {
  if (gLink && gLink->connected() && editorCommand != AM2Proto::CMD_NONE) {
    gLink->queueCommand(editorCommand, editorValue);
  }
  closeEditor();
}

void openEditor(const Action& action) {
  if (!gLink || !gLink->connected()) return;

  closeEditor();
  editorCommand = action.command;
  editorValue = currentValueForCommand(action.command);
  editorMin = action.minValue;
  editorMax = action.maxValue;
  editorStep = action.step;
  std::snprintf(editorUnit, sizeof(editorUnit), "%s", action.unit ? action.unit : "");

  editorOverlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(editorOverlay, AM2_SCREEN_W, AM2_SCREEN_H);
  lv_obj_set_pos(editorOverlay, 0, 0);
  lv_obj_set_style_bg_color(editorOverlay, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(editorOverlay, LV_OPA_70, LV_PART_MAIN);
  lv_obj_set_style_border_width(editorOverlay, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(editorOverlay, 0, LV_PART_MAIN);
  lv_obj_clear_flag(editorOverlay, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* card = lv_obj_create(editorOverlay);
  lv_obj_set_size(card, 278, 154);
  lv_obj_center(card);
  styleBase(card, C_PANEL, 10);
  lv_obj_set_style_border_color(card, color(C_CYAN), LV_PART_MAIN);

  lv_obj_t* title = makeLabel(card, action.title, 0, 12, &lv_font_montserrat_14, C_CYAN);
  lv_obj_set_width(title, 278);
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

  lv_obj_t* minusBtn = makeButton(card, 18, 52, 58, 54, "-");
  lv_obj_t* plusBtn = makeButton(card, 202, 52, 58, 54, "+");
  lv_obj_t* valueBox = lv_obj_create(card);
  lv_obj_set_pos(valueBox, 84, 52);
  lv_obj_set_size(valueBox, 110, 54);
  styleBase(valueBox, 0x09151E, 7);
  editorValueLabel = makeLabel(valueBox, "", 0, 13, &lv_font_montserrat_20, C_TEXT);
  lv_obj_set_width(editorValueLabel, 110);
  lv_obj_set_style_text_align(editorValueLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  refreshEditorValue();

  lv_obj_t* cancelBtn = makeButton(card, 18, 116, 105, 28, "CANCEL");
  lv_obj_t* applyBtn = makeButton(card, 155, 116, 105, 28, "APPLY");
  setButtonStateColor(applyBtn, 0x123C31, C_GREEN);

  lv_obj_add_event_cb(minusBtn, editorMinus, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(plusBtn, editorPlus, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(cancelBtn, editorCancel, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(applyBtn, editorApply, LV_EVENT_CLICKED, nullptr);
}

void actionEvent(lv_event_t* e) {
  auto* action = static_cast<Action*>(lv_event_get_user_data(e));
  if (!action || !gLink || !gLink->connected()) return;
  if (action->toggle) {
    int32_t current = currentValueForCommand(action->command);
    gLink->queueCommand(action->command, current ? 0 : 1);
  } else {
    openEditor(*action);
  }
}

void powerEvent(lv_event_t*) {
  if (!gLink || !gLink->connected()) return;
  const auto& s = gLink->state();
  if (s.mode != 1 || s.alarmLatched) return;
  gLink->queueCommand(s.systemEnabled ? AM2Proto::CMD_STOP : AM2Proto::CMD_START, 0);
}

lv_obj_t* tempCard(lv_obj_t* parent, int x, const char* title, lv_obj_t** valueOut) {
  lv_obj_t* card = lv_obj_create(parent);
  lv_obj_set_pos(card, x, 4);
  lv_obj_set_size(card, 74, 57);
  styleBase(card, C_PANEL, 7);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  makeLabel(card, title, 0, 8, &lv_font_montserrat_10, C_MUTED);
  lv_obj_t* titleLabel = lv_obj_get_child(card, 0);
  lv_obj_set_width(titleLabel, 74);
  lv_obj_set_style_text_align(titleLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  *valueOut = makeLabel(card, "--.- C", 0, 29, &lv_font_montserrat_14, C_TEXT);
  lv_obj_set_width(*valueOut, 74);
  lv_obj_set_style_text_align(*valueOut, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  return card;
}

lv_obj_t* valueCard(lv_obj_t* parent, int x, int y, int w, int h, const char* title, lv_obj_t** valueOut) {
  lv_obj_t* card = lv_obj_create(parent);
  lv_obj_set_pos(card, x, y);
  lv_obj_set_size(card, w, h);
  styleBase(card, C_PANEL, 7);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  makeLabel(card, title, 7, 7, &lv_font_montserrat_10, C_MUTED);
  *valueOut = makeLabel(card, "--", 7, h - 25, &lv_font_montserrat_16, C_TEXT);
  return card;
}

void buildHome(lv_obj_t* page) {
  tempCard(page, 4, "RETURN", &homeTemps[0]);
  tempCard(page, 82, "SUPPLY", &homeTemps[1]);
  tempCard(page, 160, "SUCTION", &homeTemps[2]);
  tempCard(page, 238, "DISCH.", &homeTemps[3]);

  lv_obj_t* statusCard = lv_obj_create(page);
  lv_obj_set_pos(statusCard, 4, 65);
  lv_obj_set_size(statusCard, 151, 101);
  styleBase(statusCard, C_PANEL, 8);
  lv_obj_clear_flag(statusCard, LV_OBJ_FLAG_SCROLLABLE);
  makeLabel(statusCard, "SYSTEM STATUS", 10, 8, &lv_font_montserrat_10, C_MUTED);
  homeStatus = makeLabel(statusCard, "STOPPED", 10, 28, &lv_font_montserrat_16, C_MUTED);
  lv_obj_set_width(homeStatus, 132);
  lv_label_set_long_mode(homeStatus, LV_LABEL_LONG_DOT);
  homeStatusSub = makeLabel(statusCard, "No active alarm", 10, 55, &lv_font_montserrat_10, C_MUTED);
  lv_obj_set_width(homeStatusSub, 130);
  lv_label_set_long_mode(homeStatusSub, LV_LABEL_LONG_DOT);
  homeRun = makeLabel(statusCard, "RUN RELAY: OFF", 10, 79, &lv_font_montserrat_10, C_MUTED);

  lv_obj_t* controlCard = lv_obj_create(page);
  lv_obj_set_pos(controlCard, 160, 65);
  lv_obj_set_size(controlCard, 156, 101);
  styleBase(controlCard, C_PANEL, 8);
  lv_obj_clear_flag(controlCard, LV_OBJ_FLAG_SCROLLABLE);
  makeLabel(controlCard, "RETURN SETPOINT", 10, 8, &lv_font_montserrat_10, C_MUTED);
  homeSetpoint = makeLabel(controlCard, "-- C", 10, 28, &lv_font_montserrat_20, C_CYAN);
  homeMode = makeLabel(controlCard, "CONTROL", 92, 10, &lv_font_montserrat_10, C_GREEN);

  lv_obj_t* spHit = lv_btn_create(controlCard);
  lv_obj_set_pos(spHit, 8, 24);
  lv_obj_set_size(spHit, 75, 34);
  lv_obj_set_style_bg_opa(spHit, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(spHit, 0, LV_PART_MAIN);
  lv_obj_add_event_cb(spHit, actionEvent, LV_EVENT_CLICKED, &actions[0]);

  powerBtn = makeButton(controlCard, 8, 64, 140, 29, "START SYSTEM");
  powerBtnLabel = lv_obj_get_child(powerBtn, 0);
  lv_obj_add_event_cb(powerBtn, powerEvent, LV_EVENT_CLICKED, nullptr);
}

void buildControl(lv_obj_t* page) {
  const char* titles[4] = {"RETURN SETPOINT", "RETURN HIGH ALERT", "SUCTION LOW ALERT", "DISCHARGE HIGH ALERT"};
  const int y[4] = {4, 45, 86, 127};
  for (int i = 0; i < 4; ++i) {
    lv_obj_t* row = makeButton(page, 4, y[i], 312, 37, "");
    makeLabel(row, titles[i], 10, 7, &lv_font_montserrat_12, C_TEXT);
    controlValues[i] = makeLabel(row, "-- C", 218, 7, &lv_font_montserrat_14, C_CYAN);
    lv_obj_set_width(controlValues[i], 78);
    lv_obj_set_style_text_align(controlValues[i], LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_add_event_cb(row, actionEvent, LV_EVENT_CLICKED, &actions[i]);
  }
}

void buildSafety(lv_obj_t* page) {
  const char* pTitles[3] = {"SUCTION PSI", "DISCH. PSI", "OIL PSI"};
  const char* aTitles[3] = {"AMP 1", "AMP 2", "AMP 3"};
  const char* sTitles[3] = {"LPS", "HPS", "OPS"};
  for (int i = 0; i < 3; ++i) {
    valueCard(page, 4 + i * 104, 4, 100, 48, pTitles[i], &safetyPressure[i]);
    valueCard(page, 4 + i * 104, 56, 100, 48, aTitles[i], &safetyAmps[i]);
    lv_obj_t* btn = makeButton(page, 4 + i * 104, 109, 100, 55, "");
    makeLabel(btn, sTitles[i], 8, 7, &lv_font_montserrat_12, C_TEXT);
    safetySwitch[i] = makeLabel(btn, "--", 8, 29, &lv_font_montserrat_12, C_MUTED);
    Action* a = (i == 0) ? &actions[10] : (i == 1 ? &actions[11] : &actions[12]);
    lv_obj_add_event_cb(btn, actionEvent, LV_EVENT_CLICKED, a);
  }
}

void setupRow(lv_obj_t* page, int y, const char* title, Action* action, lv_obj_t** valueOut) {
  lv_obj_t* row = makeButton(page, 4, y, 304, 34, "");
  makeLabel(row, title, 10, 8, &lv_font_montserrat_12, C_TEXT);
  *valueOut = makeLabel(row, "--", 194, 8, &lv_font_montserrat_12, C_CYAN);
  lv_obj_set_width(*valueOut, 96);
  lv_obj_set_style_text_align(*valueOut, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
  lv_obj_add_event_cb(row, actionEvent, LV_EVENT_CLICKED, action);
}

void buildSetup(lv_obj_t* page) {
  lv_obj_set_scroll_dir(page, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(page, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_style_pad_bottom(page, 6, LV_PART_MAIN);

  Action* rows[14] = {
    &actions[4], &actions[5], &actions[6], &actions[7], &actions[8], &actions[9],
    &actions[10], &actions[11], &actions[12], &actions[13],
    &actions[14], &actions[15], &actions[16], &actions[17]
  };
  const char* titles[14] = {
    "Operating Mode", "Auto Reset", "Current Module", "Suction Pressure", "Discharge Pressure", "Oil Pressure",
    "LPS Protection", "HPS Protection", "OPS Protection", "Restart Delay",
    "Suction PSI Limit", "Discharge PSI Limit", "Oil PSI Limit", "Max Current"
  };
  for (int i = 0; i < 14; ++i) setupRow(page, i * 38, titles[i], rows[i], &setupValues[i]);

  lv_obj_set_height(lv_obj_get_child(page, 13), 34);
}

void buildMain() {
  if (mainBuilt) return;
  mainBuilt = true;

  lv_obj_t* scr = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(scr, color(C_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* header = lv_obj_create(scr);
  lv_obj_set_pos(header, 4, 4);
  lv_obj_set_size(header, 312, 29);
  styleBase(header, 0x0A1822, 7);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  makeLabel(header, "AM2", 9, 6, &lv_font_montserrat_14, C_CYAN);
  headerDevice = makeLabel(header, "AM2-444051", 47, 7, &lv_font_montserrat_10, C_TEXT);
  headerLink = makeLabel(header, "RS485 WAIT", 208, 7, &lv_font_montserrat_10, C_AMBER);
  lv_obj_set_width(headerLink, 92);
  lv_obj_set_style_text_align(headerLink, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

  for (int i = 0; i < 4; ++i) {
    pages[i] = lv_obj_create(scr);
    lv_obj_set_pos(pages[i], 0, 35);
    lv_obj_set_size(pages[i], 320, 170);
    lv_obj_set_style_bg_opa(pages[i], LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(pages[i], 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(pages[i], 0, LV_PART_MAIN);
    if (i != 3) lv_obj_clear_flag(pages[i], LV_OBJ_FLAG_SCROLLABLE);
  }

  buildHome(pages[0]);
  buildControl(pages[1]);
  buildSafety(pages[2]);
  buildSetup(pages[3]);

  lv_obj_t* nav = lv_obj_create(scr);
  lv_obj_set_pos(nav, 4, 207);
  lv_obj_set_size(nav, 312, 29);
  styleBase(nav, 0x0A1822, 7);
  lv_obj_clear_flag(nav, LV_OBJ_FLAG_SCROLLABLE);
  const char* names[4] = {"HOME", "CONTROL", "SAFETY", "SETUP"};
  for (int i = 0; i < 4; ++i) {
    navBtns[i] = makeButton(nav, 3 + i * 77, 3, 74, 23, names[i]);
    lv_obj_set_style_text_font(lv_obj_get_child(navBtns[i], 0), &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_add_event_cb(navBtns[i], navEvent, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
  }

  lv_scr_load(scr);
  splash = nullptr;
  showPage(0);
}

void buildSplash() {
  splash = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(splash, color(C_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(splash, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(splash, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(splash, 0, LV_PART_MAIN);
  lv_obj_clear_flag(splash, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* mark = lv_obj_create(splash);
  lv_obj_set_size(mark, 112, 78);
  lv_obj_align(mark, LV_ALIGN_CENTER, 0, -18);
  styleBase(mark, 0x0B1B25, 12);
  lv_obj_set_style_border_width(mark, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(mark, color(C_CYAN), LV_PART_MAIN);
  lv_obj_clear_flag(mark, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* am2 = makeLabel(mark, "AM2", 0, 11, &lv_font_montserrat_28, C_CYAN);
  lv_obj_set_width(am2, 112);
  lv_obj_set_style_text_align(am2, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_t* system = makeLabel(mark, "CONTROL SYSTEM", 0, 52, &lv_font_montserrat_10, C_GREEN);
  lv_obj_set_width(system, 112);
  lv_obj_set_style_text_align(system, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

  lv_obj_t* sub = makeLabel(splash, "Industrial HVAC Controller", 0, 166, &lv_font_montserrat_12, C_TEXT);
  lv_obj_set_width(sub, 320);
  lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_t* id = makeLabel(splash, "AM2-444051  |  RS485 HMI", 0, 190, &lv_font_montserrat_10, C_MUTED);
  lv_obj_set_width(id, 320);
  lv_obj_set_style_text_align(id, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_scr_load(splash);
}

void setTextColor(lv_obj_t* label, uint32_t c) {
  if (label) lv_obj_set_style_text_color(label, color(c), LV_PART_MAIN);
}

void updateUi() {
  if (!mainBuilt || !gLink) return;
  const bool linkOk = gLink->connected();
  const auto& s = gLink->state();

  if (linkOk) {
    lv_label_set_text(headerDevice, s.deviceName[0] ? s.deviceName : "AM2");
    lv_label_set_text(headerLink, "RS485 ONLINE");
    setTextColor(headerLink, C_GREEN);
  } else {
    lv_label_set_text(headerLink, "RS485 LOST");
    setTextColor(headerLink, C_RED);
  }

  char buf[64];
  if (linkOk) {
    formatTemp(buf, sizeof(buf), s.returnTemp10); lv_label_set_text(homeTemps[0], buf);
    formatTemp(buf, sizeof(buf), s.supplyTemp10); lv_label_set_text(homeTemps[1], buf);
    formatTemp(buf, sizeof(buf), s.suctionTemp10); lv_label_set_text(homeTemps[2], buf);
    formatTemp(buf, sizeof(buf), s.dischargeTemp10); lv_label_set_text(homeTemps[3], buf);

    lv_label_set_text(homeStatus, statusText(s.compStatus));
    setTextColor(homeStatus, statusColor(s.compStatus));
    lv_label_set_text(homeStatusSub, s.alarmLatched ? statusText(s.compStatus) : "No active alarm");
    setTextColor(homeStatusSub, s.alarmLatched ? C_RED : C_MUTED);
    std::snprintf(buf, sizeof(buf), "RUN RELAY: %s", s.runOutput ? "ON" : "OFF");
    lv_label_set_text(homeRun, buf);
    setTextColor(homeRun, s.runOutput ? C_GREEN : C_MUTED);

    std::snprintf(buf, sizeof(buf), "%d C", s.returnSp);
    lv_label_set_text(homeSetpoint, buf);
    lv_label_set_text(homeMode, s.mode ? "CONTROL" : "MONITOR");
    setTextColor(homeMode, s.mode ? C_GREEN : C_AMBER);

    if (s.mode != 1) {
      lv_label_set_text(powerBtnLabel, "MONITOR ONLY");
      setButtonStateColor(powerBtn, C_PANEL2, C_BORDER);
      lv_obj_add_state(powerBtn, LV_STATE_DISABLED);
    } else if (s.alarmLatched) {
      lv_label_set_text(powerBtnLabel, "ALARM ACTIVE");
      setButtonStateColor(powerBtn, 0x4A1821, C_RED);
      lv_obj_add_state(powerBtn, LV_STATE_DISABLED);
    } else {
      lv_obj_clear_state(powerBtn, LV_STATE_DISABLED);
      if (s.systemEnabled) {
        lv_label_set_text(powerBtnLabel, "STOP SYSTEM");
        setButtonStateColor(powerBtn, 0x4A1821, C_RED);
      } else {
        lv_label_set_text(powerBtnLabel, "START SYSTEM");
        setButtonStateColor(powerBtn, 0x123C31, C_GREEN);
      }
    }

    const int16_t vals[4] = {s.returnSp, s.returnAlertSp, s.suctionAlertSp, s.dischargeAlertSp};
    for (int i = 0; i < 4; ++i) {
      std::snprintf(buf, sizeof(buf), "%d C", vals[i]);
      lv_label_set_text(controlValues[i], buf);
    }

    const int16_t p[3] = {s.suctionPsi, s.dischargePsi, s.oilPsi};
    const int16_t a[3] = {s.amp1, s.amp2, s.amp3};
    const uint8_t raw[3] = {s.lpsInputActive, s.hpsInputActive, s.opsInputActive};
    const uint8_t en[3] = {s.lpsEnable, s.hpsEnable, s.opsEnable};
    for (int i = 0; i < 3; ++i) {
      if (p[i] >= 887) std::snprintf(buf, sizeof(buf), "-- psi");
      else std::snprintf(buf, sizeof(buf), "%d psi", p[i]);
      lv_label_set_text(safetyPressure[i], buf);
      if (a[i] >= 887) std::snprintf(buf, sizeof(buf), "-- A");
      else std::snprintf(buf, sizeof(buf), "%d A", a[i]);
      lv_label_set_text(safetyAmps[i], buf);
      std::snprintf(buf, sizeof(buf), "%s / %s", en[i] ? "EN" : "OFF", raw[i] ? "ACTIVE" : "NORMAL");
      lv_label_set_text(safetySwitch[i], buf);
      setTextColor(safetySwitch[i], raw[i] ? C_RED : (en[i] ? C_GREEN : C_MUTED));
    }

    const char* on = "ON";
    const char* off = "OFF";
    lv_label_set_text(setupValues[0], s.mode ? "CONTROL" : "MONITOR");
    lv_label_set_text(setupValues[1], s.autoReset ? on : off);
    lv_label_set_text(setupValues[2], s.ctEnabled ? on : off);
    lv_label_set_text(setupValues[3], s.suctionPressureEnable ? on : off);
    lv_label_set_text(setupValues[4], s.dischargePressureEnable ? on : off);
    lv_label_set_text(setupValues[5], s.oilPressureEnable ? on : off);
    lv_label_set_text(setupValues[6], s.lpsEnable ? on : off);
    lv_label_set_text(setupValues[7], s.hpsEnable ? on : off);
    lv_label_set_text(setupValues[8], s.opsEnable ? on : off);
    std::snprintf(buf, sizeof(buf), "%d s", s.restartDelaySec); lv_label_set_text(setupValues[9], buf);
    std::snprintf(buf, sizeof(buf), "%d psi", s.suctionPsiSp); lv_label_set_text(setupValues[10], buf);
    std::snprintf(buf, sizeof(buf), "%d psi", s.dischargePsiSp); lv_label_set_text(setupValues[11], buf);
    std::snprintf(buf, sizeof(buf), "%d psi", s.oilPsiSp); lv_label_set_text(setupValues[12], buf);
    std::snprintf(buf, sizeof(buf), "%d A", s.ampsLimit); lv_label_set_text(setupValues[13], buf);
  } else {
    for (auto* label : homeTemps) if (label) lv_label_set_text(label, "--.- C");
    lv_label_set_text(homeStatus, "OFFLINE");
    setTextColor(homeStatus, C_RED);
    lv_label_set_text(homeStatusSub, "Waiting for AM2 controller");
    lv_label_set_text(homeRun, "RUN RELAY: --");
    lv_label_set_text(homeSetpoint, "-- C");
    lv_label_set_text(homeMode, "--");
    lv_label_set_text(powerBtnLabel, "LINK OFFLINE");
    lv_obj_add_state(powerBtn, LV_STATE_DISABLED);
    setButtonStateColor(powerBtn, C_PANEL2, C_BORDER);
  }
}

}  // namespace

void uiBegin(DisplayLink& link) {
  gLink = &link;
  bootMs = millis();
  buildSplash();
}

void uiLoop() {
  if (!mainBuilt && millis() - bootMs >= 1300UL) buildMain();
  if (!mainBuilt) return;

  const uint32_t now = millis();
  if (now - lastUiUpdate >= 200UL || (gLink && gLink->revision() != lastRevision)) {
    lastUiUpdate = now;
    if (gLink) lastRevision = gLink->revision();
    updateUi();
  }
}
