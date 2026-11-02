#include "HwConfig.h"
#include <string.h>
#include <stdio.h>

#if defined(NRF52_PLATFORM) || defined(ARDUINO_ARCH_NRF52) || defined(NRF52840_XXAA) || defined(NRF52832_XXAA)
  #include <InternalFileSystem.h>
  using namespace Adafruit_LittleFS_Namespace;
  #define HW_FS_NRF 1
#elif defined(ESP32)
  #include <SPIFFS.h>
  #define HW_FS_ESP 1
#endif

#ifndef AUTO_OFF_MILLIS
  #define AUTO_OFF_MILLIS 15000
#endif

#define HW_CFG_PATH    "/hwcfg.bin"
#define HW_CFG_MAGIC   0xA5
#define HW_CFG_VERSION 1

#ifndef HW_DEFAULT_BUZZER_PIN
  #define HW_DEFAULT_BUZZER_PIN 29        // P0.29
#endif

HwConfig hw_config;

static const uint32_t TIMEOUTS[] = {5000, 10000, 15000, 30000, 60000, 120000, 300000};
static const uint8_t  N_TIMEOUTS = 7;

static const HwNote SEQ_MSG[]      = {{2400, 120}};
static const HwNote SEQ_ACK[]      = {{1800, 30}};
static const HwNote SEQ_TEST[]     = {{2000, 100}};
static const HwNote SEQ_START[]    = {{1500, 80}, {0, 30}, {2200, 100}};
static const HwNote SEQ_SHUTDOWN[] = {{2200, 80}, {0, 30}, {1500, 100}};

static inline bool timeReached(uint32_t now, uint32_t target) {
  return (int32_t)(now - target) >= 0;
}

// --------------------------------------------------

HwConfig::HwConfig()
  : _bz_pin(-1), _vib_pin(-1), _seq(NULL), _seq_len(0), _seq_idx(0),
    _playing(false), _note_end(0), _vib_on(false), _vib_end(0),
    _dirty(false), _dirty_since(0) {
  setDefaults();
}

void HwConfig::setDefaults() {
  memset(&_d, 0, sizeof(_d));
  _d.magic = HW_CFG_MAGIC;
  _d.version = HW_CFG_VERSION;

  _d.sensor_mask = 1 << (uint8_t)HwSensor::Buzzer;
#if ENV_INCLUDE_GPS == 1
  _d.sensor_mask |= 1 << (uint8_t)HwSensor::Gps;
#endif

  for (uint8_t i = 0; i < 6; i++) _d.pins[i] = -1;

#ifdef PIN_BUZZER
  _d.pins[(uint8_t)HwPin::Buzzer] = (int8_t)PIN_BUZZER;
#else
  _d.pins[(uint8_t)HwPin::Buzzer] = HW_DEFAULT_BUZZER_PIN;
#endif
#ifdef PIN_VIBRATION
  _d.pins[(uint8_t)HwPin::Vibra] = (int8_t)PIN_VIBRATION;
  _d.sensor_mask |= 1 << (uint8_t)HwSensor::Vibra;
#endif
#ifdef PIN_BOARD_SDA
  _d.pins[(uint8_t)HwPin::I2cSda] = (int8_t)PIN_BOARD_SDA;
#endif
#ifdef PIN_BOARD_SCL
  _d.pins[(uint8_t)HwPin::I2cScl] = (int8_t)PIN_BOARD_SCL;
#endif

  // képernyő-időzítés: a legközelebbi érték az AUTO_OFF_MILLIS-hez
  if (AUTO_OFF_MILLIS == 0) {
    _d.timeout_idx = N_TIMEOUTS;   // "Never"
  } else {
    uint8_t best = 0;
    uint32_t best_diff = 0xFFFFFFFF;
    for (uint8_t i = 0; i < N_TIMEOUTS; i++) {
      uint32_t diff = TIMEOUTS[i] > AUTO_OFF_MILLIS ? TIMEOUTS[i] - AUTO_OFF_MILLIS
                                                    : AUTO_OFF_MILLIS - TIMEOUTS[i];
      if (diff < best_diff) { best_diff = diff; best = i; }
    }
    _d.timeout_idx = best;
  }

#ifdef KEEP_DISPLAY_ON_USB
  _d.keep_usb = 1;
#endif
  _d.msg_wake = 1;
}

// --------------------------------------------------
// Fájlkezelés
// --------------------------------------------------

bool HwConfig::load() {
  Data tmp;
  memset(&tmp, 0, sizeof(tmp));
  bool ok = false;

#if defined(HW_FS_NRF)
  File f = InternalFS.open(HW_CFG_PATH, FILE_O_READ);
  if (f) {
    ok = f.read((uint8_t*)&tmp, sizeof(tmp)) == (int)sizeof(tmp);
    f.close();
  }
#elif defined(HW_FS_ESP)
  File f = SPIFFS.open(HW_CFG_PATH, "r");
  if (f) {
    ok = f.read((uint8_t*)&tmp, sizeof(tmp)) == sizeof(tmp);
    f.close();
  }
#endif

  if (!ok || tmp.magic != HW_CFG_MAGIC || tmp.version != HW_CFG_VERSION) return false;
  if (tmp.timeout_idx >= timeoutCount()) return false;
  for (uint8_t i = 0; i < 6; i++) {
    if (tmp.pins[i] < -1 || tmp.pins[i] > HW_MAX_PIN) return false;
  }
  _d = tmp;
  return true;
}

void HwConfig::save() {
  _dirty = false;
#if defined(HW_FS_NRF)
  InternalFS.remove(HW_CFG_PATH);
  File f = InternalFS.open(HW_CFG_PATH, FILE_O_WRITE);
  if (f) {
    f.write((uint8_t*)&_d, sizeof(_d));
    f.close();
  }
#elif defined(HW_FS_ESP)
  File f = SPIFFS.open(HW_CFG_PATH, "w");
  if (f) {
    f.write((uint8_t*)&_d, sizeof(_d));
    f.close();
  }
#endif
}

void HwConfig::markDirty() {
  _dirty = true;
  _dirty_since = millis();
}

void HwConfig::begin() {
  if (!load()) setDefaults();
  applyPins();
}

void HwConfig::loop() {
  uint32_t now = millis();

  if (_playing && timeReached(now, _note_end)) {
    _seq_idx++;
    if (_seq_idx < _seq_len) playNote();
    else stopBeep();
  }

  if (_vib_on && timeReached(now, _vib_end)) {
    if (_vib_pin >= 0) digitalWrite(_vib_pin, LOW);
    _vib_on = false;
  }

  if (_dirty && (now - _dirty_since) > 2000) save();
}

// --------------------------------------------------
// Szenzorok / pinek
// --------------------------------------------------

bool HwConfig::sensorEnabled(HwSensor s) const {
  return (_d.sensor_mask >> (uint8_t)s) & 1;
}

void HwConfig::setSensor(HwSensor s, bool on) {
  if (on) _d.sensor_mask |= (1 << (uint8_t)s);
  else    _d.sensor_mask &= ~(1 << (uint8_t)s);
  markDirty();
  applyPins();
}

int8_t HwConfig::pin(HwPin p) const { return _d.pins[(uint8_t)p]; }

void HwConfig::setPin(HwPin p, int8_t value) {
  _d.pins[(uint8_t)p] = value;
  markDirty();
  applyPins();
}

HwSensor HwConfig::sensorOfPin(HwPin p) {
  switch (p) {
    case HwPin::Buzzer: return HwSensor::Buzzer;
    case HwPin::Vibra:  return HwSensor::Vibra;

    default:            return HwSensor::Env;
  }
}

// A rádió, a gomb és a LED pinjei nem választhatók – a rádió lefagyna.
bool HwConfig::isReserved(int p) {
#ifdef P_LORA_NSS
  if (p == (int)P_LORA_NSS) return true;
#endif
#ifdef P_LORA_SCLK
  if (p == (int)P_LORA_SCLK) return true;
#endif
#ifdef P_LORA_MISO
  if (p == (int)P_LORA_MISO) return true;
#endif
#ifdef P_LORA_MOSI
  if (p == (int)P_LORA_MOSI) return true;
#endif
#ifdef P_LORA_DIO_1
  if (p == (int)P_LORA_DIO_1) return true;
#endif
#ifdef P_LORA_RESET
  if (p == (int)P_LORA_RESET) return true;
#endif
#ifdef P_LORA_BUSY
  if (p == (int)P_LORA_BUSY) return true;
#endif
#ifdef PIN_USER_BTN
  if (p == (int)PIN_USER_BTN) return true;
#endif
#ifdef PIN_GPS_RX
  if (p == (int)PIN_GPS_RX) return true;
#endif
#ifdef PIN_GPS_TX
  if (p == (int)PIN_GPS_TX) return true;
#endif
#ifdef PIN_GPS_EN
  if (p == (int)PIN_GPS_EN) return true;
#endif
#ifdef PIN_GPS_RESET
  if (p == (int)PIN_GPS_RESET) return true;
#endif
#ifdef PIN_TFT_SCL
  if (p == (int)PIN_TFT_SCL) return true;
#endif
#ifdef PIN_TFT_SDA
  if (p == (int)PIN_TFT_SDA) return true;
#endif
#ifdef PIN_TFT_RST
  if (p == (int)PIN_TFT_RST) return true;
#endif
#ifdef PIN_TFT_CS
  if (p == (int)PIN_TFT_CS) return true;
#endif
#ifdef PIN_TFT_DC
  if (p == (int)PIN_TFT_DC) return true;
#endif
#ifdef PIN_TFT_LEDA_CTL
  if (p == (int)PIN_TFT_LEDA_CTL) return true;
#endif

#ifdef PIN_BAT_CTL
  if (p == (int)PIN_BAT_CTL) return true;
#endif
#ifdef PIN_VEXT_EN
  if (p == (int)PIN_VEXT_EN) return true;
#endif
#ifdef PIN_STATUS_LED
  if (p == (int)PIN_STATUS_LED) return true;
#endif
#ifdef PIN_VBAT_READ
  if (p == (int)PIN_VBAT_READ) return true;
#endif
  (void)p;
  return false;
}

bool HwConfig::usedByOther(HwPin self, int p) const {
  for (uint8_t i = 0; i < (uint8_t)HwPin::COUNT; i++) {
    if (i != (uint8_t)self && _d.pins[i] == p) return true;
  }
  return false;
}

int8_t HwConfig::nextPin(HwPin p, int8_t dir) const {
  const int range = HW_MAX_PIN + 2;          // -1 .. HW_MAX_PIN
  int idx = _d.pins[(uint8_t)p] + 1;

  for (int k = 0; k < range; k++) {
    idx = (idx + (dir < 0 ? -1 : 1) + range) % range;
    int cand = idx - 1;
    if (cand < 0) return -1;
    if (!isReserved(cand) && !usedByOther(p, cand)) return (int8_t)cand;
  }
  return _d.pins[(uint8_t)p];
}

void HwConfig::pinName(int8_t pin, char* out, size_t n) {
  if (pin < 0) {
    snprintf(out, n, "None");
    return;
  }
#if defined(HW_FS_NRF)
  snprintf(out, n, "P%d.%02d", pin / 32, pin % 32);
#else
  snprintf(out, n, "GPIO%d", pin);
#endif
}

// --------------------------------------------------
// UI beállítások
// --------------------------------------------------

uint8_t HwConfig::timeoutIdx() const { return _d.timeout_idx; }

void HwConfig::setTimeoutIdx(uint8_t idx) {
  if (idx >= timeoutCount()) idx = 0;
  _d.timeout_idx = idx;
  markDirty();
}

uint8_t HwConfig::timeoutCount() const {
  return N_TIMEOUTS + (AUTO_OFF_MILLIS == 0 ? 1 : 0);
}

uint32_t HwConfig::timeoutMillis(uint8_t idx) const {
  return idx < N_TIMEOUTS ? TIMEOUTS[idx] : 0;
}

void HwConfig::timeoutLabel(uint8_t idx, char* out, size_t n) const {
  uint32_t ms = timeoutMillis(idx);
  if (ms == 0)          snprintf(out, n, "Never");
  else if (ms < 60000)  snprintf(out, n, "%ds", (int)(ms / 1000));
  else                  snprintf(out, n, "%dmin", (int)(ms / 60000));
}

bool HwConfig::keepUsb() const { return _d.keep_usb; }
void HwConfig::setKeepUsb(bool v) { _d.keep_usb = v ? 1 : 0; markDirty(); }
bool HwConfig::msgWake() const { return _d.msg_wake; }
void HwConfig::setMsgWake(bool v) { _d.msg_wake = v ? 1 : 0; markDirty(); }

// --------------------------------------------------
// Hangjelzés / rezgés
// --------------------------------------------------

bool HwConfig::buzzerActive() const {
  return sensorEnabled(HwSensor::Buzzer) && _d.pins[(uint8_t)HwPin::Buzzer] >= 0;
}

bool HwConfig::vibraActive() const {
  return sensorEnabled(HwSensor::Vibra) && _d.pins[(uint8_t)HwPin::Vibra] >= 0;
}

bool HwConfig::buzzerMuted() const {
  return !sensorEnabled(HwSensor::Buzzer) && _d.pins[(uint8_t)HwPin::Buzzer] >= 0;
}

void HwConfig::releasePin(int8_t p) {
  if (p < 0) return;
  digitalWrite(p, LOW);
  pinMode(p, INPUT);
}

void HwConfig::applyPins() {
  int8_t want_bz  = buzzerActive() ? _d.pins[(uint8_t)HwPin::Buzzer] : -1;
  int8_t want_vib = vibraActive()  ? _d.pins[(uint8_t)HwPin::Vibra]  : -1;

  if (want_bz != _bz_pin) {
    stopBeep();
    releasePin(_bz_pin);
    _bz_pin = want_bz;
    if (_bz_pin >= 0) {
      pinMode(_bz_pin, OUTPUT);
      digitalWrite(_bz_pin, LOW);
    }
  }

  if (want_vib != _vib_pin) {
    if (_vib_pin >= 0) digitalWrite(_vib_pin, LOW);
    _vib_on = false;
    releasePin(_vib_pin);
    _vib_pin = want_vib;
    if (_vib_pin >= 0) {
      pinMode(_vib_pin, OUTPUT);
      digitalWrite(_vib_pin, LOW);
    }
  }
}

void HwConfig::startSeq(const HwNote* seq, uint8_t len) {
  if (_bz_pin < 0) return;
  _seq = seq;
  _seq_len = len;
  _seq_idx = 0;
  playNote();
}

void HwConfig::playNote() {
  const HwNote& n = _seq[_seq_idx];
  if (n.freq) tone(_bz_pin, n.freq);
  else        noTone(_bz_pin);
  _note_end = millis() + n.ms;
  _playing = true;
}

void HwConfig::stopBeep() {
  if (_bz_pin >= 0) {
    noTone(_bz_pin);
    digitalWrite(_bz_pin, LOW);
  }
  _playing = false;
}

void HwConfig::vibrate(uint16_t ms) {
  if (_vib_pin < 0) return;
  digitalWrite(_vib_pin, HIGH);
  _vib_on = true;
  _vib_end = millis() + ms;
}

void HwConfig::messageAlert() {
  startSeq(SEQ_MSG, 1);
  vibrate(200);
}

void HwConfig::ackTick()       { startSeq(SEQ_ACK, 1); }
void HwConfig::startupBeep()   { startSeq(SEQ_START, 3); }
void HwConfig::shutdownBeep()  { startSeq(SEQ_SHUTDOWN, 3); }
void HwConfig::testBuzzer()    { startSeq(SEQ_TEST, 1); }
void HwConfig::testVibra()     { vibrate(150); }

bool HwConfig::busy() const { return _playing || _vib_on; }
