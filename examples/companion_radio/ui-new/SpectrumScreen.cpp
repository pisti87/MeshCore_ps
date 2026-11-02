
#include "SpectrumScreen.h"
#include "UITask.h"

#include <stdio.h>
#include "I18n.h"
//#include "MenuUi.h"

// --------------------------------------------------
// Spectrum képernyő – 128x64
// --------------------------------------------------

static const int GRAPH_X = 2;
static const int GRAPH_Y = 23;
static const int GRAPH_H = 39;

static const int GRAPH_BORDER = 1;
static const float NOISE_FLOOR_DEFAULT = -95.0f;

// A keret kirajzolásának állapota.
// Egyetlen SpectrumScreen példányhoz.
static bool graph_frame_drawn = false;

// --------------------------------------------------
// RSSI érték átalakítása képernyő-koordinátává
// --------------------------------------------------

int SpectrumScreen::rssiToY(float rssi, int graph_y, int graph_h) {
  if (rssi < -120.0f) rssi = -120.0f;
  if (rssi > -40.0f) rssi = -40.0f;

  float normalized = (rssi + 120.0f) / 80.0f;

  return graph_y + graph_h -
         (int)(normalized * graph_h);
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
    _current_rssi(-120.0f),
    _write_pos(0),
    _sample_count(0),
    _last_sample(0) {

  for (uint8_t i = 0; i < SAMPLE_COUNT; i++) {
    _samples[i] = -120.0f;
  }
}

// --------------------------------------------------
// Reset
// --------------------------------------------------

void SpectrumScreen::reset() {
  _write_pos = 0;
  _sample_count = 0;
  _current_rssi = -120.0f;
  _last_sample = 0;

  graph_frame_drawn = false;

  for (uint8_t i = 0; i < SAMPLE_COUNT; i++) {
    _samples[i] = -120.0f;
  }
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
  if (rssi > 0.0f || rssi < -150.0f) {
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
// Szaggatott referencia-vonal
// --------------------------------------------------

static void drawDashedLine(
  DisplayDriver& display,
  int x,
  int y,
  int width
) {
  display.setColor(UIColor::warning_txt);

  const int dash_width = 3;
  const int dash_gap = 2;

  for (int pos = 0; pos < width; pos += dash_width + dash_gap) {
    int remaining = width - pos;
    int current_width =
      remaining < dash_width ? remaining : dash_width;

    display.fillRect(
      x + pos,
      y,
      current_width,
      1
    );
  }
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
  // NF és PK kiszámítása
  // ----------------------------------------------

  float noise_floor = NOISE_FLOOR_DEFAULT;
  float peak = -120.0f;

  if (_sample_count > 0) {
    noise_floor = _samples[
      (_write_pos + SAMPLE_COUNT - _sample_count) % SAMPLE_COUNT
    ];

    peak = noise_floor;

    for (uint8_t i = 0; i < _sample_count; i++) {
      uint8_t index =
        (_write_pos + SAMPLE_COUNT - _sample_count + i)
        % SAMPLE_COUNT;

      float value = _samples[index];

      if (value < noise_floor) {
        noise_floor = value;
      }

      if (value > peak) {
        peak = value;
      }
    }
  }

  // ----------------------------------------------
  // NF / NOW / PK értékek
  // ----------------------------------------------

  char nf_text[12];
  char now_text[12];
  char pk_text[12];

  snprintf(nf_text, sizeof(nf_text),
           "NF %d", (int)(noise_floor - 0.5f));

  snprintf(now_text, sizeof(now_text),
           "NOW %d", (int)(_current_rssi - 0.5f));

  snprintf(pk_text, sizeof(pk_text),
           "PK %d", (int)(peak - 0.5f));

  display.setTextSize(1);
  display.setColor(UIColor::primary_txt);

  display.setCursor(2, 12);
  display.print(nf_text);

  display.setCursor(width / 3 + 2, 12);
  display.print(now_text);

  display.setCursor((width * 2) / 3 + 2, 12);
  display.print(pk_text);

  // ----------------------------------------------
  // A keret csak egyszer rajzolódik ki.
  // ----------------------------------------------

  if (!graph_frame_drawn) {
    display.setColor(UIColor::secondary_txt);

    // Felső és alsó keret.
    display.fillRect(GRAPH_X, GRAPH_Y, graph_w, 1);
    display.fillRect(
      GRAPH_X,
      GRAPH_Y + GRAPH_H - 1,
      graph_w,
      1
    );

    // Bal és jobb keret.
    display.fillRect(GRAPH_X, GRAPH_Y, 1, GRAPH_H);
    display.fillRect(
      GRAPH_X + graph_w - 1,
      GRAPH_Y,
      1,
      GRAPH_H
    );

    graph_frame_drawn = true;
  }

  // ----------------------------------------------
  // Csak a grafikon belsejét töröljük.
  // A kerethez nem nyúlunk.
  // ----------------------------------------------

  display.setColor(UIColor::window_bkg);

  display.fillRect(
    inner_x,
    inner_y,
    inner_w,
    inner_h
  );

  // ----------------------------------------------
  // RSSI oszlopok
  // ----------------------------------------------

  if (_sample_count > 0) {
    const int bar_w = inner_w / SAMPLE_COUNT;

    for (uint8_t i = 0; i < _sample_count; i++) {
      uint8_t index =
        (_write_pos + SAMPLE_COUNT - _sample_count + i)
        % SAMPLE_COUNT;

      float value = _samples[index];

      int y = rssiToY(
        value,
        inner_y,
        inner_h
      );

      int bar_height = inner_y + inner_h - y;

      if (bar_height < 1) {
        bar_height = 1;
      }

      int x = inner_x + i * bar_w;

      display.setColor(UIColor::corp_blue);

      display.fillRect(
        x,
        y,
        bar_w > 1 ? bar_w - 1 : 1,
        bar_height
      );
    }
  }

  // ----------------------------------------------
  // NF referencia-vonal
  // ----------------------------------------------

  int nf_y = rssiToY(
    noise_floor,
    inner_y,
    inner_h
  );

  drawDashedLine(
    display,
    inner_x,
    nf_y,
    inner_w
  );

  // ----------------------------------------------
  // NOW aktuális érték
  // ----------------------------------------------

  int now_y = rssiToY(
    _current_rssi,
    inner_y,
    inner_h
  );

  drawDashedLine(
    display,
    inner_x,
    now_y,
    inner_w
  );

  // ----------------------------------------------
  // PK csúcsérték
  // ----------------------------------------------

  int pk_y = rssiToY(
    peak,
    inner_y,
    inner_h
  );

  drawDashedLine(
    display,
    inner_x,
    pk_y,
    inner_w
  );

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
