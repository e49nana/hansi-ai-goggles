/*
 * ═══════════════════════════════════════════════════════════════
 *  HANSI ZOË AI GOGGLES — Firmware v3.0 (Phase 4: Voice)
 *  Target: Seeed XIAO ESP32-S3 Sense
 * ═══════════════════════════════════════════════════════════════
 *
 *  Phase 4 additions:
 *    - Energy-based Voice Activity Detection (VAD)
 *    - Button-to-talk and hands-free voice modes
 *    - Audio buffering with ring buffer for STT
 *    - New BLE characteristic: voice data (chunked WAV)
 *    - Improved audio quality (gain, noise gate)
 *    - HUD status updates for voice state
 *
 *  Voice Architecture:
 *    Mic (PDM) → VAD → Audio Buffer → BLE → Phone
 *    Phone → Whisper STT → LLM → TTS → BLE → Speaker
 *                                         └→ HUD text
 *
 *  Voice Modes:
 *    PUSH_TO_TALK: Hold button to record, release to send
 *    HANDS_FREE:   VAD auto-detects speech, sends on silence
 *    CONTINUOUS:    Stream all audio (for Babel mode)
 * ═══════════════════════════════════════════════════════════════
 */

#include <NimBLEDevice.h>
#include "esp_camera.h"
#include <driver/i2s.h>

// ─── PIN DEFINITIONS ─────────────────────────────────────────
// Camera (XIAO ESP32S3 Sense)
#define PWDN_GPIO_NUM   -1
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM   10
#define SIOD_GPIO_NUM   40
#define SIOC_GPIO_NUM   39
#define Y9_GPIO_NUM     48
#define Y8_GPIO_NUM     11
#define Y7_GPIO_NUM     12
#define Y6_GPIO_NUM     14
#define Y5_GPIO_NUM     16
#define Y4_GPIO_NUM     18
#define Y3_GPIO_NUM     17
#define Y2_GPIO_NUM     15
#define VSYNC_GPIO_NUM  38
#define HREF_GPIO_NUM   47
#define PCLK_GPIO_NUM   13

// I/O
#define LED_PIN         21
#define BUTTON_PIN      0
#define BATTERY_PIN     3
#define MIC_SCK_PIN     42
#define MIC_DATA_PIN    41

// ─── BLE UUIDs ───────────────────────────────────────────────
#define GOGGLES_SVC_UUID    "19B10000-E8F2-537E-4F6C-D104768A1214"
#define CHAR_IMG_DATA_UUID  "19B10001-E8F2-537E-4F6C-D104768A1214"
#define CHAR_IMG_SIZE_UUID  "19B10002-E8F2-537E-4F6C-D104768A1214"
#define CHAR_AUDIO_UUID     "19B10003-E8F2-537E-4F6C-D104768A1214"
#define CHAR_CMD_UUID       "19B10004-E8F2-537E-4F6C-D104768A1214"
#define CHAR_STATUS_UUID    "19B10005-E8F2-537E-4F6C-D104768A1214"

// Phase 4: Voice characteristics
#define CHAR_VOICE_UUID     "19B10006-E8F2-537E-4F6C-D104768A1214"  // Complete voice recording (WAV chunks)
#define CHAR_VOICE_CTL_UUID "19B10007-E8F2-537E-4F6C-D104768A1214"  // Voice control (state notifications)

// HUD relay
#define HUD_SVC_UUID        "19B20000-E8F2-537E-4F6C-D104768A1214"
#define HUD_CHAR_TEXT_UUID  "19B20001-E8F2-537E-4F6C-D104768A1214"
#define HUD_CHAR_MODE_UUID  "19B20002-E8F2-537E-4F6C-D104768A1214"
#define HUD_CHAR_STAT_UUID  "19B20003-E8F2-537E-4F6C-D104768A1214"
#define HUD_CHAR_ALRT_UUID  "19B20004-E8F2-537E-4F6C-D104768A1214"

// ─── ENUMS ───────────────────────────────────────────────────
enum Mode       { MODE_IDLE=0, MODE_SCOUT, MODE_BABEL, MODE_TITAN, MODE_SURVEY, MODE_COMBAT };
enum VoiceMode  { VM_PUSH_TO_TALK=0, VM_HANDS_FREE, VM_CONTINUOUS };
enum VoiceState { VS_IDLE=0, VS_LISTENING, VS_RECORDING, VS_SENDING, VS_PROCESSING, VS_SPEAKING };

// ─── VAD CONFIG ──────────────────────────────────────────────
#define VAD_FRAME_SIZE      512       // samples per VAD frame (32ms at 16kHz)
#define VAD_THRESHOLD       800       // RMS energy threshold for speech
#define VAD_SILENCE_FRAMES  25        // ~800ms of silence to end recording
#define VAD_MIN_SPEECH_MS   300       // minimum speech duration
#define VAD_MAX_SPEECH_MS   15000     // maximum recording duration (15s)

// ─── AUDIO RING BUFFER ──────────────────────────────────────
#define RING_BUFFER_SIZE    (16000 * 16)  // 16 seconds at 16kHz (256KB)
int16_t ringBuffer[RING_BUFFER_SIZE];
volatile uint32_t ringWritePos = 0;
volatile uint32_t speechStartPos = 0;
volatile uint32_t speechSamples = 0;

// ─── GLOBAL STATE ────────────────────────────────────────────
volatile Mode       currentMode    = MODE_IDLE;
volatile VoiceMode  voiceMode      = VM_PUSH_TO_TALK;
volatile VoiceState voiceState     = VS_IDLE;
volatile bool       phoneConnected = false;
volatile bool       isCapturing    = false;
volatile bool       buttonHeld     = false;
uint8_t             batteryPercent = 100;
uint32_t            lastActivity   = 0;

// VAD state
volatile bool       vadSpeechDetected = false;
volatile uint32_t   vadSilenceCount   = 0;
volatile uint32_t   vadSpeechStart    = 0;
float               vadEnergySmooth   = 0;

// BLE characteristics
NimBLECharacteristic* charImgData   = nullptr;
NimBLECharacteristic* charImgSize   = nullptr;
NimBLECharacteristic* charAudio     = nullptr;
NimBLECharacteristic* charCmd       = nullptr;
NimBLECharacteristic* charStatus    = nullptr;
NimBLECharacteristic* charVoice     = nullptr;
NimBLECharacteristic* charVoiceCtl  = nullptr;

// HUD chars
NimBLECharacteristic* hudText   = nullptr;
NimBLECharacteristic* hudMode   = nullptr;
NimBLECharacteristic* hudStat   = nullptr;
NimBLECharacteristic* hudAlert  = nullptr;

// ─── CAMERA INIT ─────────────────────────────────────────────
bool initCamera() {
  camera_config_t cfg;
  cfg.ledc_channel = LEDC_CHANNEL_0;  cfg.ledc_timer = LEDC_TIMER_0;
  cfg.pin_d0 = Y2_GPIO_NUM;  cfg.pin_d1 = Y3_GPIO_NUM;  cfg.pin_d2 = Y4_GPIO_NUM;
  cfg.pin_d3 = Y5_GPIO_NUM;  cfg.pin_d4 = Y6_GPIO_NUM;  cfg.pin_d5 = Y7_GPIO_NUM;
  cfg.pin_d6 = Y8_GPIO_NUM;  cfg.pin_d7 = Y9_GPIO_NUM;
  cfg.pin_xclk = XCLK_GPIO_NUM;  cfg.pin_pclk = PCLK_GPIO_NUM;
  cfg.pin_vsync = VSYNC_GPIO_NUM;  cfg.pin_href = HREF_GPIO_NUM;
  cfg.pin_sccb_sda = SIOD_GPIO_NUM;  cfg.pin_sccb_scl = SIOC_GPIO_NUM;
  cfg.pin_pwdn = PWDN_GPIO_NUM;  cfg.pin_reset = RESET_GPIO_NUM;
  cfg.xclk_freq_hz = 20000000;  cfg.pixel_format = PIXFORMAT_JPEG;
  cfg.grab_mode = CAMERA_GRAB_LATEST;

  if (psramFound()) {
    cfg.frame_size = FRAMESIZE_VGA; cfg.jpeg_quality = 12;
    cfg.fb_count = 2; cfg.fb_location = CAMERA_FB_IN_PSRAM;
  } else {
    cfg.frame_size = FRAMESIZE_QVGA; cfg.jpeg_quality = 15;
    cfg.fb_count = 1; cfg.fb_location = CAMERA_FB_IN_DRAM;
  }

  if (esp_camera_init(&cfg) != ESP_OK) {
    Serial.println("[CAM] Init failed!");
    return false;
  }

  sensor_t* s = esp_camera_sensor_get();
  s->set_whitebal(s, 1); s->set_awb_gain(s, 1);
  s->set_exposure_ctrl(s, 1); s->set_aec2(s, 1);
  s->set_gain_ctrl(s, 1); s->set_lenc(s, 1);

  Serial.println("[CAM] OK");
  return true;
}

// ─── MICROPHONE INIT ─────────────────────────────────────────
bool initMic() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = VAD_FRAME_SIZE,
  };
  i2s_pin_config_t pins = {
    .bck_io_num = MIC_SCK_PIN, .ws_io_num = MIC_SCK_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE, .data_in_num = MIC_DATA_PIN,
  };

  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL) != ESP_OK) return false;
  if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) return false;

  Serial.println("[MIC] OK (16kHz PDM)");
  return true;
}

// ─── VAD (Voice Activity Detection) ──────────────────────────
float computeRMS(int16_t* samples, int count) {
  int64_t sum = 0;
  for (int i = 0; i < count; i++) {
    sum += (int64_t)samples[i] * samples[i];
  }
  return sqrt((float)sum / count);
}

void processVAD(int16_t* frame, int frameSize) {
  float energy = computeRMS(frame, frameSize);

  // Exponential smoothing for noise floor tracking
  vadEnergySmooth = vadEnergySmooth * 0.95 + energy * 0.05;

  // Dynamic threshold: max(fixed threshold, 2x noise floor)
  float threshold = max((float)VAD_THRESHOLD, vadEnergySmooth * 2.5f);

  bool isSpeech = (energy > threshold);

  if (isSpeech) {
    if (!vadSpeechDetected) {
      // Speech onset
      vadSpeechDetected = true;
      vadSpeechStart = millis();
      speechStartPos = (ringWritePos - frameSize + RING_BUFFER_SIZE) % RING_BUFFER_SIZE;
      speechSamples = 0;
      voiceState = VS_RECORDING;
      notifyVoiceState();
      Serial.printf("[VAD] Speech detected (RMS: %.0f > %.0f)\n", energy, threshold);
    }
    vadSilenceCount = 0;
    speechSamples += frameSize;
  } else if (vadSpeechDetected) {
    vadSilenceCount++;

    // Check if silence long enough to end recording
    if (vadSilenceCount >= VAD_SILENCE_FRAMES) {
      uint32_t duration = millis() - vadSpeechStart;

      if (duration >= VAD_MIN_SPEECH_MS) {
        Serial.printf("[VAD] Speech ended (%u ms, %u samples)\n", duration, speechSamples);
        sendVoiceRecording();
      } else {
        Serial.println("[VAD] Too short, discarding");
      }

      vadSpeechDetected = false;
      vadSilenceCount = 0;
      voiceState = VS_IDLE;
      notifyVoiceState();
    }
  }

  // Force-end if too long
  if (vadSpeechDetected && (millis() - vadSpeechStart > VAD_MAX_SPEECH_MS)) {
    Serial.println("[VAD] Max duration reached, sending");
    sendVoiceRecording();
    vadSpeechDetected = false;
    vadSilenceCount = 0;
    voiceState = VS_IDLE;
    notifyVoiceState();
  }
}

// ─── VOICE RECORDING SEND ────────────────────────────────────
void sendVoiceRecording() {
  if (speechSamples == 0 || !charVoice || charVoice->getSubscribedCount() == 0) return;

  voiceState = VS_SENDING;
  notifyVoiceState();

  // Create WAV header
  uint32_t dataSize = speechSamples * 2;  // 16-bit = 2 bytes per sample
  uint32_t fileSize = 44 + dataSize;
  uint8_t wavHeader[44];

  // RIFF header
  memcpy(wavHeader, "RIFF", 4);
  *(uint32_t*)(wavHeader + 4) = fileSize - 8;
  memcpy(wavHeader + 8, "WAVE", 4);

  // fmt chunk
  memcpy(wavHeader + 12, "fmt ", 4);
  *(uint32_t*)(wavHeader + 16) = 16;         // chunk size
  *(uint16_t*)(wavHeader + 20) = 1;          // PCM format
  *(uint16_t*)(wavHeader + 22) = 1;          // mono
  *(uint32_t*)(wavHeader + 24) = 16000;      // sample rate
  *(uint32_t*)(wavHeader + 28) = 32000;      // byte rate
  *(uint16_t*)(wavHeader + 32) = 2;          // block align
  *(uint16_t*)(wavHeader + 34) = 16;         // bits per sample

  // data chunk
  memcpy(wavHeader + 36, "data", 4);
  *(uint32_t*)(wavHeader + 40) = dataSize;

  // Send header first
  // Protocol: [0xFF][0xFF] = WAV header marker, then 44 bytes
  uint8_t headerPacket[46];
  headerPacket[0] = 0xFF;
  headerPacket[1] = 0xFF;
  memcpy(headerPacket + 2, wavHeader, 44);
  charVoice->setValue(headerPacket, 46);
  charVoice->notify();
  delay(10);

  // Send audio data from ring buffer in 510-byte chunks
  // Protocol: [chunkId_hi][chunkId_lo][...PCM data...]
  const uint16_t CHUNK_DATA = 508;
  uint32_t readPos = speechStartPos;
  uint32_t remaining = speechSamples;
  uint16_t chunkId = 0;
  uint8_t chunkBuf[510];

  while (remaining > 0) {
    uint32_t samplesToSend = min(remaining, (uint32_t)(CHUNK_DATA / 2));
    uint16_t bytesToSend = samplesToSend * 2;

    chunkBuf[0] = (chunkId >> 8) & 0xFF;
    chunkBuf[1] = chunkId & 0xFF;

    // Copy from ring buffer (handle wraparound)
    for (uint32_t i = 0; i < samplesToSend; i++) {
      uint32_t pos = (readPos + i) % RING_BUFFER_SIZE;
      memcpy(chunkBuf + 2 + i * 2, &ringBuffer[pos], 2);
    }

    charVoice->setValue(chunkBuf, bytesToSend + 2);
    charVoice->notify();
    delay(6);

    readPos = (readPos + samplesToSend) % RING_BUFFER_SIZE;
    remaining -= samplesToSend;
    chunkId++;
  }

  // Send end marker
  uint8_t endMarker[2] = { 0xFE, 0xFE };
  charVoice->setValue(endMarker, 2);
  charVoice->notify();

  Serial.printf("[VOICE] Sent %u samples (%u chunks, %u bytes)\n",
    speechSamples, chunkId, speechSamples * 2 + 44);

  speechSamples = 0;
  voiceState = VS_PROCESSING;
  notifyVoiceState();
}

// ─── VOICE STATE NOTIFICATION ────────────────────────────────
void notifyVoiceState() {
  if (!charVoiceCtl || charVoiceCtl->getSubscribedCount() == 0) return;

  uint8_t stateBuf[4] = {
    (uint8_t)voiceState,
    (uint8_t)voiceMode,
    vadSpeechDetected ? 1 : 0,
    (uint8_t)min(vadEnergySmooth / 10.0f, 255.0f),  // audio level 0-255
  };
  charVoiceCtl->setValue(stateBuf, 4);
  charVoiceCtl->notify();
}

// ─── AUDIO CAPTURE TASK ──────────────────────────────────────
void captureAudio() {
  int16_t frame[VAD_FRAME_SIZE];
  size_t bytesRead = 0;

  esp_err_t err = i2s_read(I2S_NUM_0, frame, sizeof(frame), &bytesRead, 50);
  if (err != ESP_OK || bytesRead == 0) return;

  int samplesRead = bytesRead / 2;

  // Apply gain (PDM mic is often quiet)
  for (int i = 0; i < samplesRead; i++) {
    int32_t sample = frame[i] * 4;  // 4x gain
    frame[i] = (int16_t)constrain(sample, -32768, 32767);
  }

  // Write to ring buffer
  for (int i = 0; i < samplesRead; i++) {
    ringBuffer[ringWritePos] = frame[i];
    ringWritePos = (ringWritePos + 1) % RING_BUFFER_SIZE;
  }

  // Process based on voice mode
  bool shouldProcess = false;

  switch (voiceMode) {
    case VM_PUSH_TO_TALK:
      if (buttonHeld) {
        if (!vadSpeechDetected) {
          vadSpeechDetected = true;
          vadSpeechStart = millis();
          speechStartPos = (ringWritePos - samplesRead + RING_BUFFER_SIZE) % RING_BUFFER_SIZE;
          speechSamples = 0;
          voiceState = VS_RECORDING;
          notifyVoiceState();
        }
        speechSamples += samplesRead;
      } else if (vadSpeechDetected) {
        // Button released → send
        uint32_t dur = millis() - vadSpeechStart;
        if (dur >= VAD_MIN_SPEECH_MS) {
          sendVoiceRecording();
        }
        vadSpeechDetected = false;
        voiceState = VS_IDLE;
        notifyVoiceState();
      }
      break;

    case VM_HANDS_FREE:
      processVAD(frame, samplesRead);
      break;

    case VM_CONTINUOUS:
      // Stream raw audio for Babel mode
      if (charAudio && charAudio->getSubscribedCount() > 0) {
        charAudio->setValue((uint8_t*)frame, bytesRead);
        charAudio->notify();
      }
      break;
  }

  // Update voice state notification periodically
  static uint32_t lastStateNotify = 0;
  if (millis() - lastStateNotify > 200) {
    notifyVoiceState();
    lastStateNotify = millis();
  }
}

// ─── BLE CALLBACKS ───────────────────────────────────────────
class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* s, NimBLEConnInfo& ci) override {
    phoneConnected = true;
    lastActivity = millis();
    Serial.println("[BLE] Connected");
    s->startAdvertising();
  }
  void onDisconnect(NimBLEServer* s, NimBLEConnInfo& ci, int r) override {
    Serial.printf("[BLE] Disconnected (%d)\n", r);
    s->startAdvertising();
  }
  void onMTUChange(uint16_t mtu, NimBLEConnInfo& ci) override {
    Serial.printf("[BLE] MTU: %u\n", mtu);
  }
};

class CmdCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& ci) override {
    const uint8_t* data = c->getValue().data();
    uint8_t cmd = data[0];
    lastActivity = millis();

    switch (cmd) {
      // Mode commands
      case 0x00: currentMode = MODE_IDLE;   break;
      case 0x01: currentMode = MODE_SCOUT;  break;
      case 0x02: currentMode = MODE_BABEL;  voiceMode = VM_CONTINUOUS; break;
      case 0x03: currentMode = MODE_TITAN;  break;
      case 0x04: currentMode = MODE_SURVEY; break;
      case 0x05: currentMode = MODE_COMBAT; break;
      case 0x10: isCapturing = true;        break;
      case 0xFF: enterDeepSleep();          break;

      // Phase 4: Voice control commands
      case 0x20: voiceMode = VM_PUSH_TO_TALK; break;
      case 0x21: voiceMode = VM_HANDS_FREE;   break;
      case 0x22: voiceMode = VM_CONTINUOUS;    break;
      case 0x30: voiceState = VS_SPEAKING;  notifyVoiceState(); break;  // TTS playing
      case 0x31: voiceState = VS_IDLE;      notifyVoiceState(); break;  // TTS done

      default: break;
    }

    Serial.printf("[CMD] 0x%02X → Mode:%d Voice:%d\n", cmd, currentMode, voiceMode);

    // Relay mode to HUD
    if (hudMode && hudMode->getSubscribedCount() > 0) {
      uint8_t m = (uint8_t)currentMode;
      hudMode->setValue(&m, 1);
      hudMode->notify();
    }
  }
};

class HudTextCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& ci) override {
    c->notify();  // Relay to subscribed T-Glass
  }
};

// ─── BLE SETUP ───────────────────────────────────────────────
void initBLE() {
  NimBLEDevice::init("HansiGoggles");
  NimBLEDevice::setMTU(517);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  NimBLEServer* srv = NimBLEDevice::createServer();
  srv->setCallbacks(new ServerCallbacks());

  // Goggles service
  NimBLEService* svc = srv->createService(GOGGLES_SVC_UUID);
  charImgData  = svc->createCharacteristic(CHAR_IMG_DATA_UUID,  NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  charImgSize  = svc->createCharacteristic(CHAR_IMG_SIZE_UUID,  NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  charAudio    = svc->createCharacteristic(CHAR_AUDIO_UUID,     NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  charCmd      = svc->createCharacteristic(CHAR_CMD_UUID,       NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  charCmd->setCallbacks(new CmdCallbacks());
  charStatus   = svc->createCharacteristic(CHAR_STATUS_UUID,    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  charVoice    = svc->createCharacteristic(CHAR_VOICE_UUID,     NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  charVoiceCtl = svc->createCharacteristic(CHAR_VOICE_CTL_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::WRITE);
  svc->start();

  // HUD relay service
  NimBLEService* hud = srv->createService(HUD_SVC_UUID);
  hudText  = hud->createCharacteristic(HUD_CHAR_TEXT_UUID,  NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::WRITE);
  hudText->setCallbacks(new HudTextCallbacks());
  hudMode  = hud->createCharacteristic(HUD_CHAR_MODE_UUID,  NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  hudStat  = hud->createCharacteristic(HUD_CHAR_STAT_UUID,  NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  hudAlert = hud->createCharacteristic(HUD_CHAR_ALRT_UUID,  NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::WRITE);
  hud->start();

  // Advertise
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(GOGGLES_SVC_UUID);
  adv->addServiceUUID(HUD_SVC_UUID);
  adv->setScanResponse(true);
  adv->start();

  Serial.println("[BLE] Ready (Goggles + HUD + Voice)");
}

// ─── IMAGE CAPTURE ───────────────────────────────────────────
void captureAndSendImage() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) return;

  uint32_t sz = fb->len;
  charImgSize->setValue((uint8_t*)&sz, 4);
  charImgSize->notify();
  delay(20);

  const uint16_t CHUNK = 507;
  uint16_t nChunks = (sz + CHUNK - 1) / CHUNK;
  uint8_t buf[CHUNK + 2];

  for (uint16_t i = 0; i < nChunks; i++) {
    uint16_t off = i * CHUNK;
    uint16_t len = min((uint16_t)(sz - off), CHUNK);
    buf[0] = (i >> 8); buf[1] = i & 0xFF;
    memcpy(buf + 2, fb->buf + off, len);
    charImgData->setValue(buf, len + 2);
    charImgData->notify();
    delay(8);
  }

  esp_camera_fb_return(fb);
}

// ─── STATUS ──────────────────────────────────────────────────
void sendStatus() {
  int raw = analogRead(BATTERY_PIN);
  float v = (raw / 4095.0) * 3.3 * 2.0;
  batteryPercent = constrain(map(v * 100, 330, 420, 0, 100), 0, 100);

  uint8_t buf[12];
  buf[0] = (uint8_t)currentMode;
  buf[1] = batteryPercent;
  buf[2] = isCapturing ? 1 : 0;
  buf[3] = phoneConnected ? 1 : 0;
  uint32_t up = millis() / 1000;
  memcpy(buf + 4, &up, 4);
  uint16_t heap = ESP.getFreeHeap() / 1024;
  memcpy(buf + 8, &heap, 2);
  buf[10] = (uint8_t)voiceState;
  buf[11] = (uint8_t)voiceMode;

  charStatus->setValue(buf, 12);
  charStatus->notify();

  // HUD status
  if (hudStat && hudStat->getSubscribedCount() > 0) {
    uint8_t hs[3] = { batteryPercent, (uint8_t)currentMode, (uint8_t)voiceState };
    hudStat->setValue(hs, 3);
    hudStat->notify();
  }
}

// ─── LED ─────────────────────────────────────────────────────
void updateLED() {
  static uint32_t lastBlink = 0;

  // Voice recording: rapid blink
  if (voiceState == VS_RECORDING) {
    if (millis() - lastBlink >= 100) {
      lastBlink = millis();
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
    return;
  }
  // Voice processing: solid ON
  if (voiceState == VS_PROCESSING || voiceState == VS_SENDING) {
    digitalWrite(LED_PIN, HIGH);
    return;
  }

  // Normal mode blink
  uint16_t interval;
  switch (currentMode) {
    case MODE_IDLE:   interval = 2000; break;
    case MODE_SCOUT:  interval = 500;  break;
    case MODE_BABEL:  interval = 300;  break;
    case MODE_TITAN:  interval = 1000; break;
    case MODE_SURVEY: interval = 3000; break;
    case MODE_COMBAT: interval = 150;  break;
    default:          interval = 2000;
  }

  if (millis() - lastBlink >= interval) {
    lastBlink = millis();
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
}

// ─── BUTTON ──────────────────────────────────────────────────
void handleButton() {
  static uint32_t pressTime = 0;
  static bool wasPressed = false;
  bool pressed = (digitalRead(BUTTON_PIN) == LOW);

  if (pressed && !wasPressed) {
    pressTime = millis();
    wasPressed = true;
    buttonHeld = true;

    if (voiceMode == VM_PUSH_TO_TALK &&
        (currentMode == MODE_TITAN || currentMode == MODE_BABEL)) {
      voiceState = VS_LISTENING;
      notifyVoiceState();
      Serial.println("[BTN] Push-to-talk START");
    }
  }

  if (!pressed && wasPressed) {
    wasPressed = false;
    buttonHeld = false;
    uint32_t dur = millis() - pressTime;
    lastActivity = millis();

    if (voiceMode == VM_PUSH_TO_TALK &&
        (currentMode == MODE_TITAN || currentMode == MODE_BABEL)) {
      Serial.println("[BTN] Push-to-talk RELEASE");
      // VAD will handle the send in captureAudio()
    } else if (dur > 2000) {
      enterDeepSleep();
    } else {
      // Cycle mode
      currentMode = (Mode)(((int)currentMode + 1) % 4);
      if (currentMode == MODE_IDLE) currentMode = MODE_SCOUT;

      // Auto-set voice mode
      if (currentMode == MODE_TITAN) voiceMode = VM_HANDS_FREE;
      else if (currentMode == MODE_BABEL) voiceMode = VM_CONTINUOUS;

      Serial.printf("[BTN] Mode: %d\n", currentMode);
      if (hudMode && hudMode->getSubscribedCount() > 0) {
        uint8_t m = (uint8_t)currentMode;
        hudMode->setValue(&m, 1);
        hudMode->notify();
      }
    }
  }
}

// ─── SLEEP ───────────────────────────────────────────────────
void enterDeepSleep() {
  Serial.println("[PWR] Sleep...");
  digitalWrite(LED_PIN, LOW);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 0);
  esp_deep_sleep_start();
}

// ─── SETUP ───────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("═══════════════════════════════════════════");
  Serial.println("  HANSI ZOE GOGGLES v3.0 — Phase 4: Voice");
  Serial.println("═══════════════════════════════════════════");

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  analogReadResolution(12);

  if (!initCamera()) Serial.println("[FATAL] Camera failed!");
  initMic();
  initBLE();

  Serial.printf("[BOOT] Free: %u KB, PSRAM: %u KB\n",
    ESP.getFreeHeap() / 1024, ESP.getFreePsram() / 1024);
}

// ─── LOOP ────────────────────────────────────────────────────
void loop() {
  handleButton();
  updateLED();

  // Always capture audio when in voice-capable modes
  if (currentMode == MODE_TITAN || currentMode == MODE_BABEL || currentMode == MODE_COMBAT) {
    captureAudio();
  }

  // Status every 5s
  static uint32_t lastStat = 0;
  if (millis() - lastStat > 5000) { sendStatus(); lastStat = millis(); }

  // Image capture by mode
  static uint32_t lastCap = 0;
  switch (currentMode) {
    case MODE_SCOUT:
      if (millis() - lastCap > 2000) { captureAndSendImage(); lastCap = millis(); }
      break;
    case MODE_BABEL:
      if (millis() - lastCap > 3000) { captureAndSendImage(); lastCap = millis(); }
      break;
    case MODE_SURVEY:
      if (millis() - lastCap > 10000) { captureAndSendImage(); lastCap = millis(); }
      break;
    case MODE_COMBAT:
      if (millis() - lastCap > 1000) { captureAndSendImage(); lastCap = millis(); }
      break;
    case MODE_IDLE:
      if (isCapturing) { captureAndSendImage(); isCapturing = false; }
      break;
    default: break;
  }

  // Auto-sleep
  if (!phoneConnected && (millis() - lastActivity > 300000)) enterDeepSleep();

  delay(5);
}
