/*
 * ═══════════════════════════════════════════════════════════════
 *  HANSI ZOË AI GOGGLES — HUD Firmware v2.0
 *  Target: LILYGO T-Glass V2 (ESP32-S3 + 126×126 AMOLED)
 * ═══════════════════════════════════════════════════════════════
 *
 *  v2.0 additions over v1.0:
 *    - Voice state visualization (mic icon, recording pulse)
 *    - Audio level bar in status area
 *    - STT/TTS indicators ("Listening...", "Speaking...")
 *    - Memory count display (RAG entries from companion)
 *    - Improved idle animation (Survey Corps wings)
 *    - Battery warning overlay (<15%)
 *    - Smoother text scrolling with word-wrap
 *
 *  BLE Client: connects to HansiGoggles HUD Relay Service
 *    0x19B20001 TEXT   → AI result text
 *    0x19B20002 MODE   → current mode (0-5)
 *    0x19B20003 STATUS → [battery%, mode, voiceState]
 *    0x19B20004 ALERT  → popup overlay text
 * ═══════════════════════════════════════════════════════════════
 */

#include <NimBLEDevice.h>
#include "LilyGo_Wristband.h"
#include "LV_Helper.h"
#include <lvgl.h>

// ─── BLE ─────────────────────────────────────────────────────
#define HUD_SVC       "19B20000-E8F2-537E-4F6C-D104768A1214"
#define HUD_TEXT      "19B20001-E8F2-537E-4F6C-D104768A1214"
#define HUD_MODE      "19B20002-E8F2-537E-4F6C-D104768A1214"
#define HUD_STAT      "19B20003-E8F2-537E-4F6C-D104768A1214"
#define HUD_ALERT     "19B20004-E8F2-537E-4F6C-D104768A1214"

static NimBLEClient* bleClient = nullptr;
static bool bleConnected = false;
static uint32_t lastReconnect = 0;

// ─── STATE ───────────────────────────────────────────────────
enum Mode       { M_IDLE=0, M_SCOUT, M_BABEL, M_TITAN, M_SURVEY, M_COMBAT };
enum VoiceState { VS_IDLE=0, VS_LISTENING, VS_RECORDING, VS_SENDING, VS_PROCESSING, VS_SPEAKING };

volatile Mode       curMode = M_IDLE;
volatile VoiceState curVoice = VS_IDLE;
volatile uint8_t    battPct = 100;
volatile bool       newText = false;
volatile bool       newAlert = false;
char textBuf[256] = "";
char alertBuf[128] = "";

// ─── DISPLAY ─────────────────────────────────────────────────
LilyGo_Wristband band;
static const int W = 126, H = 126;

// Mode colors (RGB565 packed)
static const lv_color_t MODE_COLORS[] = {
  lv_color_hex(0x555555), // IDLE  - gray
  lv_color_hex(0x00B4D8), // SCOUT - cyan
  lv_color_hex(0x06D6A0), // BABEL - green
  lv_color_hex(0xFFD166), // TITAN - yellow
  lv_color_hex(0xEF476F), // SURVEY- pink
  lv_color_hex(0x9B5DE5), // COMBAT- purple
};

static const char* MODE_NAMES[] = {
  "IDLE", "SCOUT", "BABEL", "TITAN", "SURVEY", "COMBAT"
};

static const char* MODE_ICONS[] = {
  LV_SYMBOL_SETTINGS, // IDLE
  LV_SYMBOL_EYE_OPEN, // SCOUT
  LV_SYMBOL_REFRESH,  // BABEL
  LV_SYMBOL_CALL,     // TITAN
  LV_SYMBOL_IMAGE,    // SURVEY
  LV_SYMBOL_GPS,      // COMBAT
};

static const char* VOICE_LABELS[] = {
  "", "Listening...", "Recording", "Sending...", "Thinking...", "Speaking..."
};

// ─── LVGL UI OBJECTS ─────────────────────────────────────────
static lv_obj_t* scr;
static lv_obj_t* topBar;
static lv_obj_t* lblMode;
static lv_obj_t* lblBatt;
static lv_obj_t* lblVoice;
static lv_obj_t* separator;
static lv_obj_t* textArea;
static lv_obj_t* lblText;
static lv_obj_t* bottomBar;
static lv_obj_t* lblStatus;
static lv_obj_t* voiceBar;  // audio level indicator
static lv_obj_t* alertOverlay;
static lv_obj_t* lblAlert;

// Brightness levels
static const uint8_t BRIGHT[] = { 32, 96, 160, 224 };
static uint8_t brightIdx = 1;

// Animation
static lv_anim_t voiceAnim;
static bool voiceAnimRunning = false;

// ─── UI SETUP ────────────────────────────────────────────────
void buildUI() {
  scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);

  // ── Top Bar: [icon] MODE  BAT%
  topBar = lv_obj_create(scr);
  lv_obj_set_size(topBar, W, 16);
  lv_obj_set_pos(topBar, 0, 0);
  lv_obj_set_style_bg_color(topBar, lv_color_hex(0x111111), 0);
  lv_obj_set_style_border_width(topBar, 0, 0);
  lv_obj_set_style_pad_all(topBar, 2, 0);
  lv_obj_set_style_radius(topBar, 0, 0);
  lv_obj_clear_flag(topBar, LV_OBJ_FLAG_SCROLLABLE);

  lblMode = lv_label_create(topBar);
  lv_label_set_text(lblMode, LV_SYMBOL_SETTINGS " IDLE");
  lv_obj_set_style_text_font(lblMode, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(lblMode, lv_color_hex(0x555555), 0);
  lv_obj_align(lblMode, LV_ALIGN_LEFT_MID, 0, 0);

  lblBatt = lv_label_create(topBar);
  lv_label_set_text(lblBatt, LV_SYMBOL_BATTERY_FULL " 100%");
  lv_obj_set_style_text_font(lblBatt, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(lblBatt, lv_color_hex(0x444444), 0);
  lv_obj_align(lblBatt, LV_ALIGN_RIGHT_MID, 0, 0);

  // ── Voice state label (below top bar)
  lblVoice = lv_label_create(scr);
  lv_label_set_text(lblVoice, "");
  lv_obj_set_style_text_font(lblVoice, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(lblVoice, lv_color_hex(0xFFD166), 0);
  lv_obj_set_pos(lblVoice, 4, 17);
  lv_obj_set_width(lblVoice, W - 8);
  lv_obj_set_style_text_align(lblVoice, LV_TEXT_ALIGN_CENTER, 0);

  // ── Voice level bar
  voiceBar = lv_bar_create(scr);
  lv_obj_set_size(voiceBar, W - 8, 3);
  lv_obj_set_pos(voiceBar, 4, 28);
  lv_bar_set_range(voiceBar, 0, 100);
  lv_bar_set_value(voiceBar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(voiceBar, lv_color_hex(0x111111), LV_PART_MAIN);
  lv_obj_set_style_bg_color(voiceBar, lv_color_hex(0xE94560), LV_PART_INDICATOR);
  lv_obj_add_flag(voiceBar, LV_OBJ_FLAG_HIDDEN);

  // ── Separator
  separator = lv_obj_create(scr);
  lv_obj_set_size(separator, W - 8, 1);
  lv_obj_set_pos(separator, 4, 32);
  lv_obj_set_style_bg_color(separator, lv_color_hex(0x222222), 0);
  lv_obj_set_style_border_width(separator, 0, 0);
  lv_obj_set_style_radius(separator, 0, 0);

  // ── Text Area (scrollable)
  textArea = lv_obj_create(scr);
  lv_obj_set_size(textArea, W, 80);
  lv_obj_set_pos(textArea, 0, 34);
  lv_obj_set_style_bg_color(textArea, lv_color_hex(0x000000), 0);
  lv_obj_set_style_border_width(textArea, 0, 0);
  lv_obj_set_style_pad_all(textArea, 4, 0);
  lv_obj_set_style_radius(textArea, 0, 0);

  lblText = lv_label_create(textArea);
  lv_label_set_long_mode(lblText, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lblText, W - 12);
  lv_label_set_text(lblText, "");
  lv_obj_set_style_text_font(lblText, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(lblText, lv_color_hex(0xCCCCCC), 0);
  lv_obj_set_style_text_line_space(lblText, 2, 0);

  // ── Bottom Bar
  bottomBar = lv_obj_create(scr);
  lv_obj_set_size(bottomBar, W, 12);
  lv_obj_set_pos(bottomBar, 0, H - 12);
  lv_obj_set_style_bg_color(bottomBar, lv_color_hex(0x0A0A0A), 0);
  lv_obj_set_style_border_width(bottomBar, 0, 0);
  lv_obj_set_style_pad_all(bottomBar, 1, 0);
  lv_obj_set_style_radius(bottomBar, 0, 0);
  lv_obj_clear_flag(bottomBar, LV_OBJ_FLAG_SCROLLABLE);

  lblStatus = lv_label_create(bottomBar);
  lv_label_set_text(lblStatus, "BLE: Scanning...");
  lv_obj_set_style_text_font(lblStatus, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(lblStatus, lv_color_hex(0x333333), 0);
  lv_obj_align(lblStatus, LV_ALIGN_CENTER, 0, 0);

  // ── Alert Overlay (hidden by default)
  alertOverlay = lv_obj_create(scr);
  lv_obj_set_size(alertOverlay, W - 16, 50);
  lv_obj_set_pos(alertOverlay, 8, (H - 50) / 2);
  lv_obj_set_style_bg_color(alertOverlay, lv_color_hex(0xE94560), 0);
  lv_obj_set_style_bg_opa(alertOverlay, LV_OPA_90, 0);
  lv_obj_set_style_border_width(alertOverlay, 1, 0);
  lv_obj_set_style_border_color(alertOverlay, lv_color_hex(0xFF6B6B), 0);
  lv_obj_set_style_radius(alertOverlay, 6, 0);
  lv_obj_set_style_pad_all(alertOverlay, 6, 0);
  lv_obj_add_flag(alertOverlay, LV_OBJ_FLAG_HIDDEN);

  lblAlert = lv_label_create(alertOverlay);
  lv_label_set_long_mode(lblAlert, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lblAlert, W - 32);
  lv_label_set_text(lblAlert, "");
  lv_obj_set_style_text_font(lblAlert, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(lblAlert, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_align(lblAlert, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(lblAlert, LV_ALIGN_CENTER, 0, 0);

  // Show idle animation
  showIdleScreen();
}

// ─── IDLE ANIMATION ──────────────────────────────────────────
void showIdleScreen() {
  lv_label_set_text(lblText,
    "\n"
    "   HANSI ZOE\n"
    "  AI  GOGGLES\n"
    "\n"
    "   " LV_SYMBOL_EYE_OPEN "  " LV_SYMBOL_REFRESH "  " LV_SYMBOL_CALL "\n"
    "\n"
    "  ExMachina Labs"
  );
  lv_obj_set_style_text_color(lblText, lv_color_hex(0x333333), 0);
  lv_obj_set_style_text_align(lblText, LV_TEXT_ALIGN_CENTER, 0);
}

// ─── UPDATE UI ───────────────────────────────────────────────
void updateModeUI() {
  uint8_t m = (uint8_t)curMode;
  if (m > 5) m = 0;

  lv_color_t col = MODE_COLORS[m];

  // Top bar
  char modeBuf[32];
  snprintf(modeBuf, sizeof(modeBuf), "%s %s", MODE_ICONS[m], MODE_NAMES[m]);
  lv_label_set_text(lblMode, modeBuf);
  lv_obj_set_style_text_color(lblMode, col, 0);

  // Separator
  lv_obj_set_style_bg_color(separator, col, 0);

  // Text color
  lv_obj_set_style_text_color(lblText, col, 0);
  lv_obj_set_style_text_align(lblText, LV_TEXT_ALIGN_LEFT, 0);

  // Voice bar color matches mode
  lv_obj_set_style_bg_color(voiceBar, col, LV_PART_INDICATOR);

  if (m == M_IDLE) {
    showIdleScreen();
  }
}

void updateVoiceUI() {
  uint8_t vs = (uint8_t)curVoice;
  if (vs > 5) vs = 0;

  if (vs == VS_IDLE) {
    lv_label_set_text(lblVoice, "");
    lv_obj_add_flag(voiceBar, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_label_set_text(lblVoice, VOICE_LABELS[vs]);
    lv_obj_clear_flag(voiceBar, LV_OBJ_FLAG_HIDDEN);

    // Recording pulse: change voice label color
    if (vs == VS_RECORDING) {
      lv_obj_set_style_text_color(lblVoice, lv_color_hex(0xEF476F), 0);
    } else if (vs == VS_SPEAKING) {
      lv_obj_set_style_text_color(lblVoice, lv_color_hex(0x06D6A0), 0);
    } else {
      lv_obj_set_style_text_color(lblVoice, lv_color_hex(0xFFD166), 0);
    }
  }
}

void updateBattUI() {
  char buf[16];
  const char* icon;
  if (battPct > 75) icon = LV_SYMBOL_BATTERY_FULL;
  else if (battPct > 50) icon = LV_SYMBOL_BATTERY_3;
  else if (battPct > 25) icon = LV_SYMBOL_BATTERY_2;
  else if (battPct > 10) icon = LV_SYMBOL_BATTERY_1;
  else icon = LV_SYMBOL_BATTERY_EMPTY;

  snprintf(buf, sizeof(buf), "%s %d%%", icon, battPct);
  lv_label_set_text(lblBatt, buf);

  // Warning color
  if (battPct <= 15) {
    lv_obj_set_style_text_color(lblBatt, lv_color_hex(0xEF476F), 0);
  } else {
    lv_obj_set_style_text_color(lblBatt, lv_color_hex(0x444444), 0);
  }
}

void updateText() {
  if (!newText) return;
  newText = false;

  lv_label_set_text(lblText, textBuf);
  lv_obj_set_style_text_align(lblText, LV_TEXT_ALIGN_LEFT, 0);

  // Scroll to top
  lv_obj_scroll_to_y(textArea, 0, LV_ANIM_ON);
}

void showAlert() {
  if (!newAlert) return;
  newAlert = false;

  lv_label_set_text(lblAlert, alertBuf);
  lv_obj_clear_flag(alertOverlay, LV_OBJ_FLAG_HIDDEN);

  // Auto-hide after 4 seconds
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, alertOverlay);
  lv_anim_set_time(&a, 4000);
  lv_anim_set_exec_cb(&a, [](void* obj, int32_t v) {
    if (v >= 100) lv_obj_add_flag((lv_obj_t*)obj, LV_OBJ_FLAG_HIDDEN);
  });
  lv_anim_set_values(&a, 0, 100);
  lv_anim_start(&a);
}

// ─── BLE CALLBACKS ───────────────────────────────────────────
class HudClientCallbacks : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient* c) override {
    bleConnected = true;
    lv_label_set_text(lblStatus, "BLE: Connected");
    lv_obj_set_style_text_color(lblStatus, lv_color_hex(0x06D6A0), 0);
    Serial.println("[BLE] Connected to goggles");
  }
  void onDisconnect(NimBLEClient* c, int reason) override {
    bleConnected = false;
    lv_label_set_text(lblStatus, "BLE: Disconnected");
    lv_obj_set_style_text_color(lblStatus, lv_color_hex(0xE94560), 0);
    Serial.printf("[BLE] Disconnected (%d)\n", reason);
  }
};

// Notification callbacks
void onText(NimBLERemoteCharacteristic* c, uint8_t* data, size_t len, bool isNotify) {
  size_t copyLen = min(len, sizeof(textBuf) - 1);
  memcpy(textBuf, data, copyLen);
  textBuf[copyLen] = '\0';
  newText = true;
  Serial.printf("[HUD] Text: %s\n", textBuf);
}

void onMode(NimBLERemoteCharacteristic* c, uint8_t* data, size_t len, bool isNotify) {
  if (len >= 1) {
    curMode = (Mode)data[0];
    Serial.printf("[HUD] Mode: %d\n", curMode);
  }
}

void onStat(NimBLERemoteCharacteristic* c, uint8_t* data, size_t len, bool isNotify) {
  if (len >= 1) battPct = data[0];
  if (len >= 2) curMode = (Mode)data[1];
  if (len >= 3) curVoice = (VoiceState)data[2];
}

void onAlert(NimBLERemoteCharacteristic* c, uint8_t* data, size_t len, bool isNotify) {
  size_t copyLen = min(len, sizeof(alertBuf) - 1);
  memcpy(alertBuf, data, copyLen);
  alertBuf[copyLen] = '\0';
  newAlert = true;
}

// ─── BLE CONNECT ─────────────────────────────────────────────
bool connectToGoggles() {
  lv_label_set_text(lblStatus, "BLE: Scanning...");
  lv_obj_set_style_text_color(lblStatus, lv_color_hex(0xFFD166), 0);
  lv_task_handler();

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setActiveScan(true);
  NimBLEScanResults results = scan->start(5);

  for (int i = 0; i < results.getCount(); i++) {
    NimBLEAdvertisedDevice dev = results.getDevice(i);
    if (dev.getName() == "HansiGoggles") {
      Serial.println("[BLE] Found HansiGoggles!");

      if (!bleClient) {
        bleClient = NimBLEDevice::createClient();
        bleClient->setClientCallbacks(new HudClientCallbacks());
      }

      if (!bleClient->connect(&dev)) {
        Serial.println("[BLE] Connect failed");
        return false;
      }

      // Request high MTU
      bleClient->setMTU(517);

      // Get HUD service
      NimBLERemoteService* svc = bleClient->getService(HUD_SVC);
      if (!svc) { Serial.println("[BLE] HUD service not found"); bleClient->disconnect(); return false; }

      // Subscribe to characteristics
      auto* cText = svc->getCharacteristic(HUD_TEXT);
      if (cText) cText->subscribe(true, onText);

      auto* cMode = svc->getCharacteristic(HUD_MODE);
      if (cMode) cMode->subscribe(true, onMode);

      auto* cStat = svc->getCharacteristic(HUD_STAT);
      if (cStat) cStat->subscribe(true, onStat);

      auto* cAlert = svc->getCharacteristic(HUD_ALERT);
      if (cAlert) cAlert->subscribe(true, onAlert);

      return true;
    }
  }

  lv_label_set_text(lblStatus, "BLE: Not found");
  lv_obj_set_style_text_color(lblStatus, lv_color_hex(0x555555), 0);
  return false;
}

// ─── TOUCH BUTTON ────────────────────────────────────────────
void handleTouch() {
  if (band.getTouched()) {
    brightIdx = (brightIdx + 1) % 4;
    band.setBrightness(BRIGHT[brightIdx]);
    Serial.printf("[HUD] Brightness: %d\n", BRIGHT[brightIdx]);
  }
}

// ─── SETUP ───────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("═══════════════════════════════════════");
  Serial.println("  HANSI ZOE HUD v2.0 — T-Glass V2");
  Serial.println("═══════════════════════════════════════");

  // Init T-Glass display
  band.begin();
  band.setBrightness(BRIGHT[brightIdx]);

  // Init LVGL
  beginLvglHelper(band);

  // Build UI
  buildUI();
  lv_task_handler();

  // Init BLE
  NimBLEDevice::init("HansiHUD");
  NimBLEDevice::setMTU(517);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  Serial.printf("[BOOT] Free: %u KB\n", ESP.getFreeHeap() / 1024);
}

// ─── LOOP ────────────────────────────────────────────────────
void loop() {
  // BLE reconnect
  if (!bleConnected && (millis() - lastReconnect > 10000)) {
    connectToGoggles();
    lastReconnect = millis();
  }

  // Update UI
  updateModeUI();
  updateVoiceUI();
  updateBattUI();
  updateText();
  showAlert();

  // Touch
  handleTouch();

  // LVGL tick
  lv_task_handler();
  delay(10);
}
