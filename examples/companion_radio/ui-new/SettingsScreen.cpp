#include "SettingsScreen.h"

#include <stdio.h>
#include "I18n.h"
#include "MenuUi.h"

enum Kind : uint8_t { K_VALUE, K_ACTION, K_SUBMENU, K_CONFIRM };

struct ItemDef {
  const char* label;   // English key for TR()
  Kind kind;
};

// Same order as enum SettingsItem
static const ItemDef ITEMS[(int)SettingsItem::COUNT] = {
  { "Theme",          K_VALUE },
  { "Screen timeout", K_VALUE },
  { "Keep on USB",    K_VALUE },
  { "Msg wake",       K_VALUE },
  { "Bluetooth",      K_VALUE },
  { "GPS",            K_VALUE },
  { "Share position", K_VALUE },
  { "LNA",            K_VALUE },
  { "Boost RX",       K_VALUE },
  { "Flood advert",   K_ACTION },
  { "Units",          K_VALUE },
  { "Language",       K_VALUE },
  { "Sensors",        K_SUBMENU },
  { "Buttons",        K_SUBMENU },
  { "Buzzer",         K_SUBMENU },
  { "Reboot",         K_CONFIRM },
  { "Hibernate",      K_CONFIRM },
  { "Power off",      K_CONFIRM },
};

static const uint8_t COUNT = (uint8_t)SettingsItem::COUNT;
static const uint32_t CONFIRM_MS = 3000;

SettingsScreen::SettingsScreen(SettingsBackend* backend)
  : _backend(backend), _selected(0), _first_visible(0), _confirm(-1), _confirm_until(0) {}

void SettingsScreen::resetSelection() {
  _selected = 0;
  _first_visible = 0;
  _confirm = -1;
}

void SettingsScreen::move(int8_t dir) {
  _confirm = -1;
  _selected = (dir > 0) ? (_selected + 1) % COUNT : (_selected + COUNT - 1) % COUNT;
  if (_selected < _first_visible) _first_visible = _selected;
  if (_selected >= _first_visible + MenuUi::VISIBLE) _first_visible = _selected - MenuUi::VISIBLE + 1;
}

int SettingsScreen::render(DisplayDriver& display) {
  char pos[10];
  snprintf(pos, sizeof(pos), "%u/%u", (unsigned)(_selected + 1), (unsigned)COUNT);
  MenuUi::header(display, TR("Settings"), pos, false);

  for (uint8_t r = 0; r < MenuUi::VISIBLE; r++) {
    uint8_t idx = _first_visible + r;
    if (idx >= COUNT) break;
    const ItemDef& def = ITEMS[idx];
    int y = MenuUi::FIRST_Y + r * MenuUi::ROW_H;

    char value[20] = "";
    bool warn = false;
    if (def.kind == K_SUBMENU) {
      snprintf(value, sizeof(value), ">");
    } else if (def.kind == K_CONFIRM) {
      if (_confirm == (int8_t)idx) { snprintf(value, sizeof(value), "%s", TR("Confirm?")); warn = true; }
    } else if (_backend) {
      _backend->getValue((SettingsItem)idx, value, sizeof(value));
    }
    MenuUi::row(display, y, TR(def.label), value, idx == _selected, warn);
  }
  return (_confirm >= 0) ? 200 : 500;
}

bool SettingsScreen::handleInput(char c) {
  if (c == KEY_PREV) { move(-1); return true; }
  if (c == KEY_NEXT) { move(+1); return true; }

  if (c == KEY_LEFT || c == KEY_RIGHT) {
    if (_backend && ITEMS[_selected].kind == K_VALUE) {
      _backend->change((SettingsItem)_selected, c == KEY_RIGHT ? +1 : -1);
    }
    return true;
  }

  if (c == KEY_ENTER) {
    if (!_backend) return true;
    SettingsItem item = (SettingsItem)_selected;
    switch (ITEMS[_selected].kind) {
      case K_VALUE:
      case K_ACTION:
        _backend->change(item, 0);
        break;
      case K_SUBMENU:
        _backend->openPage(item);
        break;
      case K_CONFIRM:
        if (_confirm == (int8_t)_selected && millis() < _confirm_until) {
          _confirm = -1;
          _backend->change(item, 0);          // second OK: do it
        } else {
          _confirm = (int8_t)_selected;       // first OK: ask
          _confirm_until = millis() + CONFIRM_MS;
        }
        break;
    }
    return true;
  }

  if (c == KEY_CANCEL) {
    _confirm = -1;
    if (_backend) _backend->back();
    return true;
  }
  return false;
}

void SettingsScreen::poll() {
  if (_confirm >= 0 && millis() >= _confirm_until) _confirm = -1;
}
