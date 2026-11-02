#pragma once
// Settings menu (reached from the last carousel page with OK, BACK returns).
//
// Keys: UP/DOWN (KEY_PREV/KEY_NEXT) move, LEFT/RIGHT change a value, OK toggles / runs,
// BACK (KEY_CANCEL) leaves. Reboot / Hibernate / Power off need OK twice within 3 s.
// The real work is done by a SettingsBackend that the UITask patch implements.

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>
#include <helpers/ui/UIScreen.h>
#include <helpers/ui/DisplayDriver.h>

enum class SettingsItem : uint8_t {
  THEME,            // Light / Dark
  SCREEN_TIMEOUT,   // 5s 10s 15s 30s 1min 2min 5min
  KEEP_USB,         // keep the screen on while on USB power
  MSG_WAKE,         // wake the screen on a new message
  BLUETOOTH,
  GPS,
  SHARE_POSITION,
  LNA,
  RX_BOOST,
  FLOOD_ADVERT,     // action: send a flood advert now
  UNITS,            // Metric / Imperial
  LANGUAGE,         // English / Magyar / Deutsch
  SENSORS,          // submenu
  BUTTONS,          // submenu
  BUZZER,           // submenu
  REBOOT,
  HIBERNATE,
  POWER_OFF,
  COUNT
};

class SettingsBackend {
public:
  virtual ~SettingsBackend() {}
  // Text shown on the right ("On", "Dark", "30s" ...). Empty = nothing.
  virtual void getValue(SettingsItem item, char* out, size_t n) = 0;
  // dir +1 / -1 = LEFT/RIGHT, 0 = OK pressed (toggle, cycle, or run the action).
  virtual void change(SettingsItem item, int8_t dir) = 0;
  // SENSORS / BUTTONS / BUZZER: show the hardware screen section.
  virtual void openPage(SettingsItem item) = 0;
  // BACK pressed: return to the carousel.
  virtual void back() = 0;
};

class SettingsScreen : public UIScreen {
public:
  explicit SettingsScreen(SettingsBackend* backend);

  int render(DisplayDriver& display) override;
  bool handleInput(char c) override;
  void poll() override;

  void resetSelection();

private:
  SettingsBackend* _backend;
  uint8_t _selected;
  uint8_t _first_visible;
  int8_t _confirm;              // item waiting for a second OK, -1 = none
  uint32_t _confirm_until;

  void move(int8_t dir);
};
