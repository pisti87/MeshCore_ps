#pragma once

// Page 2: signal history. Shows the radio's instantaneous RSSI over time (about 8 s of history,
// one sample every 250 ms) as bars. This is NOT a frequency sweep: the radio is not retuned and
// the mesh keeps running. (wadamesh's analyzer pauses the mesh and sweeps the band; that needs
// a "mesh paused" flag in the main loop and can be added later.)

#include <Arduino.h>
#include <stdint.h>
#include <helpers/ui/UIScreen.h>
#include <helpers/ui/DisplayDriver.h>

// Returns the current RSSI in dBm (UITask patch: radio_driver.getCurrentRSSI()).
typedef float (*SpectrumRSSIReader)(void* context);

class UITask;

class SpectrumScreen : public UIScreen {
public:
  static const uint8_t  SAMPLE_COUNT       = 32;
  static const uint16_t SAMPLE_INTERVAL_MS = 250;

  explicit SpectrumScreen(UITask* task,
                          SpectrumRSSIReader reader,
                          void* context = nullptr);

  int  render(DisplayDriver& display) override;
  bool handleInput(char c) override;
  void poll() override;

  void reset();

private:
  UITask*            _task;
  SpectrumRSSIReader _reader;
  void*              _context;

  float    _samples[SAMPLE_COUNT];
  float    _current_rssi;
  uint8_t  _write_pos;
  uint8_t  _sample_count;
  uint32_t _last_sample;

  // i = 0 a legrégebbi, i = _sample_count - 1 a legújabb minta.
  float sampleAt(uint8_t i) const;

  static int rssiToY(float rssi, int graph_y, int graph_h);
};
