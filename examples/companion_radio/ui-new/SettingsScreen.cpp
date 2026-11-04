#include "SettingsScreen.h"
#include "I18n.h"
#include <stdio.h>
#include <string.h>

static const int ROW_H    = 10;
static const int LIST_TOP = 14;

static const char* mainLabel(SettingsItem i) {
  switch (i) {
    #if defined(HELTEC_T096)
    case SettingsItem::TX_POWER:
      return TR("TX power");
#endif
    case SettingsItem::LANGUAGE: return TR("Language");
    case SettingsItem::SCREEN_TIMEOUT: return TR("Screen timeout");
    case SettingsItem::BRIGHTNESS:     return TR("Brightness");
    case SettingsItem::KEEP_USB:       return TR("Keep on USB");
    case SettingsItem::MSG_WAKE:       return TR("Wake on message");
    case SettingsItem::BLUETOOTH:      return TR("Bluetooth");
    case SettingsItem::GPS:            return TR("GPS");
    case SettingsItem::BUZZER:         return TR("Buzzer");
    case SettingsItem::VIBRA:          return TR("Vibration");
    case SettingsItem::FLOOD_ADVERT:   return TR("Send advert");
    case SettingsItem::SENSORS:        return TR("Sensors");
    case SettingsItem::PINS:           return TR("Pins");
    case SettingsItem::REBOOT:         return TR("Reboot");
    case SettingsItem::POWER_OFF:      return TR("Power off");
    default:                           return "";
  }
}

static const char* sensorLabel(HwSensor s) {
  switch (s) {
    case HwSensor::Buzzer: return TR("Buzzer");
    case HwSensor::Vibra:  return TR("Vibra motor");
    case HwSensor::Gps:    return TR("GPS");
    case HwSensor::Env:    return TR("I2C sensor");
    default:               return "";
  }
}

static const char* pinLabel(HwPin p) {
  switch (p) {
    case HwPin::Buzzer: return TR("Buzzer");
    case HwPin::Vibra:  return TR("Vibra");
    case HwPin::I2cSda: return TR("I2C SDA");
    case HwPin::I2cScl: return TR("I2C SCL");
    default:            return "";
  }
}

// --------------------------------------------------

SettingsScreen::SettingsScreen(SettingsBackend* backend)
  : _backend(backend), _page(PAGE_MAIN), _selected(1), _first_visible(0),
    _confirm(-1), _confirm_until(0), _vis_count(0) {
}

void SettingsScreen::resetSelection() {
  _page = PAGE_MAIN;
  _selected = 1;
  _first_visible = 0;
  _confirm = -1;
}

void SettingsScreen::rebuildPins() {
  _vis_count = 0;

  for (uint8_t i = 0; i < (uint8_t)HwPin::COUNT; i++) {
    HwPin p = (HwPin)i;

    if (hw_config.sensorEnabled(HwConfig::sensorOfPin(p))) {
      _vis_pins[_vis_count++] = p;
    }
  }
}

int SettingsScreen::rowCount() const {
  switch (_page) {
    case PAGE_SENSORS: return 1 + (int)HwSensor::COUNT;
    case PAGE_PINS:    return 1 + _vis_count;
    default:           return 1 + (int)SettingsItem::COUNT;
  }
}

void SettingsScreen::getRow(int row, char* label, size_t ln, char* value, size_t vn) {
  label[0] = 0;
  value[0] = 0;

  if (row == 0) {
    snprintf(label, ln, "%s", TR("< Back"));
    return;
  }
  int idx = row - 1;

  if (_page == PAGE_MAIN) {
    SettingsItem item = (SettingsItem)idx;
    snprintf(label, ln, "%s", mainLabel(item));

    if (item == SettingsItem::SENSORS || item == SettingsItem::PINS) {
      snprintf(value, vn, ">");
    } else if (item == SettingsItem::REBOOT || item == SettingsItem::POWER_OFF) {
      if (_confirm == (int8_t)item && (int32_t)(millis() - _confirm_until) < 0) {
        snprintf(value, vn, "OK?");
      }
    } else {
      _backend->getValue(item, value, vn);
    }
  } else if (_page == PAGE_SENSORS) {
    HwSensor s = (HwSensor)idx;
    snprintf(label, ln, "%s", sensorLabel(s));
    snprintf(value, vn, "%s", hw_config.sensorEnabled(s) ? TR("On") : TR("Off"));
  } else {
    if (idx < _vis_count) {
      HwPin p = _vis_pins[idx];
      snprintf(label, ln, "%s", pinLabel(p));
      HwConfig::pinName(hw_config.pin(p), value, vn);
    }
  }
}

// --------------------------------------------------

int SettingsScreen::render(DisplayDriver& display) {
  const int w = display.width();
  const int h = display.height();

  // fejléc
  display.setColor(UIColor::title_bkg);
  display.fillRect(0, 0, w, 12);
  display.setTextSize(1);
  display.setColor(UIColor::title_txt);
  display.setCursor(0, 2);
  display.print(_page == PAGE_SENSORS ? TR("Sensors")
              : _page == PAGE_PINS    ? TR("Pins")
                                      : TR("Settings"));

  int visible = (h - LIST_TOP) / ROW_H;
  if (visible < 1) visible = 1;

  int rows = rowCount();
  if (_selected >= rows) _selected = rows - 1;
  if (_selected < _first_visible) _first_visible = _selected;
  if (_selected >= _first_visible + visible) _first_visible = _selected - visible + 1;

  const bool scroll = rows > visible;
  const int right = w - (scroll ? 4 : 1);

  char label[32];
  char value[24];

  for (int r = 0; r < visible && (_first_visible + r) < rows; r++) {
    int row = _first_visible + r;
    int y = LIST_TOP + r * ROW_H;

    getRow(row, label, sizeof(label), value, sizeof(value));

//    if (row == _selected) {
//      display.setColor(UIColor::primary_txt);
//      display.fillRect(0, y - 1, right + 1, ROW_H);
//      display.setColor(UIColor::window_bkg);
//    } else {
//      display.setColor(UIColor::primary_txt);
//    }
if (row == _selected) {
  display.setColor(UIColor::selected_bkg);
  display.fillRect(0, y - 1, right + 1, ROW_H);
  display.setColor(UIColor::selected_txt);
} else {
  display.setColor(UIColor::primary_txt);
}
    
    int vw = value[0] ? (int)display.getTextWidth(value) : 0;
    display.drawTextEllipsized(1, y, right - vw - 4, label);
    if (vw) {
      display.setCursor(right - vw, y);
      display.print(value);
    }
  }

  // üres Pinek oldal: magyarázat
  if (_page == PAGE_PINS && _vis_count == 0) {
    display.setColor(UIColor::secondary_txt);
    display.drawTextCentered(w / 2, LIST_TOP + 2 * ROW_H, TR("No sensor selected"));
  }

  // görgetősáv
  if (scroll) {
    int track_h = visible * ROW_H;
    int thumb_h = (track_h * visible) / rows;
    if (thumb_h < 3) thumb_h = 3;
    int thumb_y = LIST_TOP + ((track_h - thumb_h) * _first_visible) / (rows - visible);

    display.setColor(UIColor::secondary_txt);
    display.fillRect(w - 1, LIST_TOP, 1, track_h);
    display.setColor(UIColor::primary_txt);
    display.fillRect(w - 3, thumb_y, 3, thumb_h);
  }

  return _confirm >= 0 ? 500 : 5000;
}

// --------------------------------------------------

void SettingsScreen::poll() {
  if (_confirm >= 0 && (int32_t)(millis() - _confirm_until) > 0) {
    _confirm = -1;
  }
}

void SettingsScreen::move(int8_t dir) {
  int rows = rowCount();
  _selected = (_selected + rows + dir) % rows;
  _confirm = -1;
}

bool SettingsScreen::handleInput(char c) {
  if (c == KEY_PREV)   { move(-1); return true; }
  if (c == KEY_NEXT)   { move(1);  return true; }
  if (c == KEY_LEFT)   { act(-1);  return true; }
  if (c == KEY_RIGHT)  { act(1);   return true; }
  if (c == KEY_ENTER)  { act(0);   return true; }
  if (c == KEY_CANCEL) { goBack(); return true; }
  return false;
}

void SettingsScreen::act(int8_t dir) {
  int row = _selected;

  if (row == 0) {
    if (dir == 0) goBack();
    return;
  }
  int idx = row - 1;

  if (_page == PAGE_MAIN) {
    if (idx < (int)SettingsItem::COUNT) actMain((SettingsItem)idx, dir);
  } else if (_page == PAGE_SENSORS) {
    if (idx < (int)HwSensor::COUNT) actSensor((HwSensor)idx);
  } else {
    if (idx < _vis_count) actPin(_vis_pins[idx], dir);
  }
}

void SettingsScreen::actMain(SettingsItem item, int8_t dir) {
  if (item == SettingsItem::SENSORS) {
    if (dir == 0) {
      _page = PAGE_SENSORS;
      _selected = 1;
      _first_visible = 0;
    }
    return;
  }

  if (item == SettingsItem::PINS) {
    if (dir == 0) {
      rebuildPins();
      _page = PAGE_PINS;
      _selected = _vis_count ? 1 : 0;
      _first_visible = 0;
    }
    return;
  }

  if (item == SettingsItem::REBOOT || item == SettingsItem::POWER_OFF) {
    if (dir != 0) return;
    if (_confirm == (int8_t)item && (int32_t)(millis() - _confirm_until) < 0) {
      _confirm = -1;
      _backend->change(item, 0);
    } else {
      _confirm = (int8_t)item;
      _confirm_until = millis() + 3000;
    }
    return;
  }

  _backend->change(item, dir);
}

void SettingsScreen::actSensor(HwSensor s) {
  bool now_on = !hw_config.sensorEnabled(s);
  hw_config.setSensor(s, now_on);

  if (now_on) {
    if (s == HwSensor::Buzzer) hw_config.testBuzzer();
    if (s == HwSensor::Vibra)  hw_config.testVibra();
  }
}

void SettingsScreen::actPin(HwPin p, int8_t dir) {
  int8_t next = hw_config.nextPin(p, dir < 0 ? -1 : 1);
  hw_config.setPin(p, next);

  // próbajelzés, hogy rögtön hallod / érzed, jó-e a pin
  if (p == HwPin::Buzzer) hw_config.testBuzzer();
  if (p == HwPin::Vibra)  hw_config.testVibra();
}

void SettingsScreen::goBack() {
  _confirm = -1;
  if (_page == PAGE_SENSORS) {
    _page = PAGE_MAIN;
    _selected = (uint8_t)SettingsItem::SENSORS + 1;
  } else if (_page == PAGE_PINS) {
    _page = PAGE_MAIN;
    _selected = (uint8_t)SettingsItem::PINS + 1;
  } else {
    _backend->back();
  }
}
