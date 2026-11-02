#include "SpectrumScreen.h"
#include "UITask.h"

#include <stdio.h>
#include <math.h>
#include "I18n.h"
//#include "MenuUi.h"   // ha az UITask.h nem húzza be, vedd ki a kommentet

// --------------------------------------------------
// Spectrum képernyő – 128x64
// --------------------------------------------------

static const int GRAPH_X      = 2;
static const int GRAPH_Y      = 23;
static const int GRAPH_H      = 39;
static const int GRAPH_BORDER = 1;

static const int LABEL_Y = 12;

static const float RSSI_MIN = -120.0f;
static const float RSSI_MAX = -40.0f;

// --------------------------------------------------
// RSSI érték átalakítása képernyő-koordinátává
// --------------------------------------------------

int SpectrumScreen::rssiToY(float rssi, int graph_y, int graph_h) {
  if (rssi < RSSI_MIN) rssi = RSSI_MIN;
  if (rssi > RSSI_MAX) rssi = RSSI_MAX;

  float normalized = (rssi - RSSI_MIN) / (RSSI_MAX - RSSI_MIN);

  return graph_y + graph_h - (int)lroundf(normalized * graph_h);
}

// --------------------------------------------------
// Konstruktor
// --------------------------------------------------

SpectrumScreen::SpectrumScreen(
  UITask* task,
  SpectrumRSSIReader reader,
  void* context
)
  : _task(task),
    _reader(reader),
    _context(context),
    _current_rssi(RSSI_MIN),
    _write_pos(0),
    _sample_count(0),
    _last_sample(0) {

  for (uint8_t i = 0; i < SAMPLE_COUNT; i++) {
    _samples[i] = RSSI_MIN;
  }
}

// --------------------------------------------------
// Reset
// --------------------------------------------------

void SpectrumScreen::reset() {
  _write_pos     = 0;
  _sample_count  = 0;
  _current_rssi  = RSSI_MIN;
  _last_sample   = 0;

  for (uint8_t i = 0; i < SAMPLE_COUNT; i++) {
    _samples[i] = RSSI_MIN;
  }
}

// --------------------------------------------------
// Körpuffer elérése: 0 = legrégebbi minta
// --------------------------------------------------

float SpectrumScreen::sampleAt(uint8_t i) const {
  uint8_t index =
    (_write_pos + SAMPLE_COUNT - _sample_count + i) % SAMPLE_COUNT;
  return _samples[index];
}

// --------------------------------------------------
// RSSI mintavételezés
// --------------------------------------------------

void SpectrumScreen::poll() {
  if (!_reader) return;

  uint32_t now = millis();

  if ((now - _last_sample) < SAMPLE_INTERVAL_MS) {
    return;
  }

  _last_sample = now;

  float rssi = _reader(_context);

  // Érvénytelen mérés esetén megtartjuk az előző adatokat.
  if (isnan(rssi) || rssi > 0.0f || rssi < -150.0f) {
    return;
  }

  _current_rssi = rssi;

  _samples[_write_pos] = rssi;
  _write_pos = (_write_pos + 1) % SAMPLE_COUNT;

  if (_sample_count < SAMPLE_COUNT) {
    _sample_count++;
  }
}

// --------------------------------------------------
// Vonalak
// --------------------------------------------------

static void drawDashedLine(
  DisplayDriver& display,
  int x,
  int y,
  int width
) {
  display.setColor(UIColor::warning_txt);

  const int dash_width = 3;
  const int dash_gap   = 2;

  for (int pos = 0; pos < width; pos += dash_width + dash_gap) {
    int remaining = width - pos;
    int current_width = remaining < dash_width ? remaining : dash_width;

    display.fillRect(x + pos, y, current_width, 1);
  }
}

static void drawSolidLine(
  DisplayDriver& display,
  int x,
  int y,
  int width
) {
  display.setColor(UIColor::primary_txt);
  display.fillRect(x, y, width, 1);
}

// --------------------------------------------------
// Grafikon kirajzolása
// --------------------------------------------------

int SpectrumScreen::render(DisplayDriver& display) {
  const int width = display.width();

  const int graph_w = width - (2 * GRAPH_X);

  const int inner_x = GRAPH_X + GRAPH_BORDER;
  const int inner_y = GRAPH_Y + GRAPH_BORDER;
  const int inner_w = graph_w - (2 * GRAPH_BORDER);
  const int inner_h = GRAPH_H - (2 * GRAPH_BORDER);

  // ----------------------------------------------
  // Fejléc
  // ----------------------------------------------

  MenuUi::header(
    display,
    TR("Spectrum"),
    "",
    false
  );

  // ----------------------------------------------
  // NF (minimum) és PK (maximum) kiszámítása
  // ----------------------------------------------

  const bool has_data = _sample_count > 0;

  float noise_floor = RSSI_MIN;
  float peak        = RSSI_MIN;

  if (has_data) {
    noise_floor = sampleAt(0);
    peak        = noise_floor;

    for (uint8_t i = 1; i < _sample_count; i++) {
      float value = sampleAt(i);

      if (value < noise_floor) noise_floor = value;
      if (value > peak)        peak = value;
    }
  }

  // ----------------------------------------------
  // NF / NOW / PK feliratok
  // ----------------------------------------------

  char nf_text[14];
  char now_text[14];
  char pk_text[14];

  if (has_data) {
    snprintf(nf_text,  sizeof(nf_text),  "NF%d",  (int)lroundf(noise_floor));
    snprintf(now_text, sizeof(now_text), "NOW%d", (int)lroundf(_current_rssi));
    snprintf(pk_text,  sizeof(pk_text),  "PK%d",  (int)lroundf(peak));
  } else {
    snprintf(nf_text,  sizeof(nf_text),  "NF --");
    snprintf(now_text, sizeof(now_text), "NOW --");
    snprintf(pk_text,  sizeof(pk_text),  "PK --");
  }

  display.setTextSize(1);
  display.setColor(UIColor::primary_txt);

  // NF balra, NOW középre, PK jobbra igazítva – nem lapolódnak át.
  display.setCursor(GRAPH_X, LABEL_Y);
  display.print(nf_text);

  int now_w = (int)display.getTextWidth(now_text);
  display.setCursor((width - now_w) / 2, LABEL_Y);
  display.print(now_text);

  int pk_w = (int)display.getTextWidth(pk_text);
  display.setCursor(width - GRAPH_X - pk_w, LABEL_Y);
  display.print(pk_text);

  // ----------------------------------------------
  // Keret – minden képkockánál kirajzoljuk, mert a
  // kijelző puffere frame-enként törlődik.
  // ----------------------------------------------

  display.setColor(UIColor::secondary_txt);

  display.fillRect(GRAPH_X, GRAPH_Y, graph_w, 1);                    // felső
  display.fillRect(GRAPH_X, GRAPH_Y + GRAPH_H - 1, graph_w, 1);      // alsó
  display.fillRect(GRAPH_X, GRAPH_Y, 1, GRAPH_H);                    // bal
  display.fillRect(GRAPH_X + graph_w - 1, GRAPH_Y, 1, GRAPH_H);      // jobb

  // ----------------------------------------------
  // A grafikon belsejének törlése
  // ----------------------------------------------

  display.setColor(UIColor::window_bkg);
  display.fillRect(inner_x, inner_y, inner_w, inner_h);

  if (!has_data) {
    return SAMPLE_INTERVAL_MS;
  }

  // ----------------------------------------------
  // RSSI oszlopok – a teljes belső szélességen
  // egyenletesen elosztva (egész osztás-hiba nélkül)
  // ----------------------------------------------

  display.setColor(UIColor::corp_blue);

  for (uint8_t i = 0; i < _sample_count; i++) {
    int x0 = inner_x + (i * inner_w) / SAMPLE_COUNT;
    int x1 = inner_x + ((i + 1) * inner_w) / SAMPLE_COUNT;

    int bar_w = x1 - x0 - 1;     // 1 px rés az oszlopok között
    if (bar_w < 1) bar_w = 1;

    int y = rssiToY(sampleAt(i), inner_y, inner_h);

    int bar_height = inner_y + inner_h - y;
    if (bar_height < 1) bar_height = 1;

    display.fillRect(x0, y, bar_w, bar_height);
  }

  // ----------------------------------------------
  // Referencia-vonalak
  // ----------------------------------------------

  int nf_y  = rssiToY(noise_floor,   inner_y, inner_h);
  int pk_y  = rssiToY(peak,          inner_y, inner_h);
  int now_y = rssiToY(_current_rssi, inner_y, inner_h);

  drawDashedLine(display, inner_x, nf_y, inner_w);   // NF – szaggatott
  drawDashedLine(display, inner_x, pk_y, inner_w);   // PK – szaggatott
  drawSolidLine (display, inner_x, now_y, inner_w);  // NOW – folytonos

  return SAMPLE_INTERVAL_MS;
}

// --------------------------------------------------
// Bemenetkezelés
// --------------------------------------------------

bool SpectrumScreen::handleInput(char c) {
  if (c == KEY_CANCEL) {
    if (_task) {
      _task->gotoHomeScreen();
    }
    return true;
  }

  return false;
}
