#include "SSD1306Display.h"

bool SSD1306Display::i2c_probe(TwoWire& wire, uint8_t addr) {
  wire.beginTransmission(addr);
  uint8_t error = wire.endTransmission();
  return (error == 0);
}

// Color scheme
ColorVal UIColor::window_bkg = SSD1306_BLACK;
ColorVal UIColor::title_bkg = SSD1306_BLACK;
ColorVal UIColor::title_txt = SSD1306_WHITE;
ColorVal UIColor::primary_txt = SSD1306_WHITE;
ColorVal UIColor::secondary_txt = SSD1306_WHITE;
ColorVal UIColor::warning_txt = SSD1306_WHITE;
ColorVal UIColor::popup_bkg = SSD1306_BLACK;
ColorVal UIColor::popup_txt = SSD1306_WHITE;
ColorVal UIColor::corp_blue = SSD1306_WHITE;


// -----------------------------------------------------------------------------
// UTF-8 és magyar ékezetek (a T096 / ST7735 megoldás mintájára)
//
// Az Adafruit 5x7-es fontjában nincsenek ékezetes betűk. Ezért a UTF-8
// szöveget kódpontokra bontjuk, az ékezetes betűt az ékezet nélküli alapbetűvel
// írjuk ki, majd az ékezetet pixelről pixelre rárajzoljuk.
//   kisbetű: az ékezet a 0-1. sorban (a kisbetű x-magassága a 2. sortól indul)
//   nagybetű: az ékezet a betű FÖLÉ kerül (-2, -1. sor)
// -----------------------------------------------------------------------------

enum : uint8_t { ACC_NONE = 0, ACC_ACUTE, ACC_DIAERESIS, ACC_DOUBLE_ACUTE };

#define BLOCK_CHAR ((char)0xDB)   // CP437 teli blokk, a nem támogatott karakterek jele

// Kódpont -> alapbetű + ékezet. Ismeretlen (>=0x80) kódpont: '?'.
static char mapHungarianChar(uint32_t cp, uint8_t& accent) {
  accent = ACC_NONE;
  switch (cp) {
    case 0x00C1: accent = ACC_ACUTE;        return 'A';   // Á
    case 0x00E1: accent = ACC_ACUTE;        return 'a';   // á
    case 0x00C9: accent = ACC_ACUTE;        return 'E';   // É
    case 0x00E9: accent = ACC_ACUTE;        return 'e';   // é
    case 0x00CD: accent = ACC_ACUTE;        return 'I';   // Í
    case 0x00ED: accent = ACC_ACUTE;        return 'i';   // í
    case 0x00D3: accent = ACC_ACUTE;        return 'O';   // Ó
    case 0x00F3: accent = ACC_ACUTE;        return 'o';   // ó
    case 0x00D6: accent = ACC_DIAERESIS;    return 'O';   // Ö
    case 0x00F6: accent = ACC_DIAERESIS;    return 'o';   // ö
    case 0x0150: accent = ACC_DOUBLE_ACUTE; return 'O';   // Ő
    case 0x0151: accent = ACC_DOUBLE_ACUTE; return 'o';   // ő
    case 0x00DA: accent = ACC_ACUTE;        return 'U';   // Ú
    case 0x00FA: accent = ACC_ACUTE;        return 'u';   // ú
    case 0x00DC: accent = ACC_DIAERESIS;    return 'U';   // Ü
    case 0x00FC: accent = ACC_DIAERESIS;    return 'u';   // ü
    case 0x0170: accent = ACC_DOUBLE_ACUTE; return 'U';   // Ű
    case 0x0171: accent = ACC_DOUBLE_ACUTE; return 'u';   // ű
    case 0x00C4: accent = ACC_DIAERESIS;    return 'A';   // Ä (német)
    case 0x00E4: accent = ACC_DIAERESIS;    return 'a';   // ä (német)
    case 0x00DF: return 's';                              // ß (német)
    default:
      return (cp < 0x80) ? (char)cp : '?';
  }
}

static bool isSupportedAccented(uint32_t cp) {
  uint8_t acc;
  char base = mapHungarianChar(cp, acc);
  return acc != ACC_NONE || cp == 0x00DF || (base != '?' && cp >= 0x80);
}

// Egy karakter kiolvasása. p a következő karakterre lép.
// Visszatér: false = nincs kirajzolandó karakter (csonka / érvénytelen bájt).
// cp: kódpont, start: a karakter első bájtja (a translate-hez).
static bool nextCodepoint(const char*& p, uint32_t& cp, bool& raw_block) {
  raw_block = false;
  const uint8_t c = (uint8_t)*p++;
  if (c < 0x80) { cp = c; return true; }

  uint8_t remaining = 0;
  uint32_t v = 0;
  if      ((c & 0xE0) == 0xC0) { v = c & 0x1F; remaining = 1; }
  else if ((c & 0xF0) == 0xE0) { v = c & 0x0F; remaining = 2; }
  else if ((c & 0xF8) == 0xF0) { v = c & 0x07; remaining = 3; }

  if (remaining) {
    const char* q = p;
    bool ok = true;
    for (uint8_t i = 0; i < remaining; i++) {
      const uint8_t b = (uint8_t)*q;
      if ((b & 0xC0) != 0x80) { ok = false; break; }
      v = (v << 6) | (b & 0x3F);
      q++;
    }
    if (ok && !((remaining == 1 && v < 0x80) ||
                (remaining == 2 && v < 0x800) ||
                (remaining == 3 && v < 0x10000))) {
      p = q;
      cp = v;
      return true;
    }
  }

  // Érvénytelen UTF-8: a translateUTF8ToBlocks() által beírt 0xDB blokk
  // jelet kirajzoljuk, minden más magányos bájtot eldobunk.
  if (c == (uint8_t)BLOCK_CHAR) { raw_block = true; cp = c; return true; }
  return false;
}

void SSD1306Display::drawAccent(int16_t x, int16_t y, uint8_t accent, bool upper) {
  const int16_t s = _text_size > 0 ? _text_size : 1;
  const int16_t r0 = upper ? -2 : 0;   // az ékezet felső sora (betűsor-egységben)
  const int16_t r1 = r0 + 1;

  auto px = [&](int16_t col, int16_t row) {
    const int16_t dy = y + row * s;
    if (dy < 0) return;                 // a kijelző tetején kilógó rész kimarad
    display.fillRect(x + col * s, dy, s, s, _color);
  };

  switch (accent) {
    case ACC_ACUTE:         // ´
      px(3, r0); px(2, r1);
      break;
    case ACC_DIAERESIS:     // ¨
      px(1, r0); px(3, r0);
      break;
    case ACC_DOUBLE_ACUTE:  // ˝  (két egymás melletti hosszúmetszőjel)
      px(2, r0); px(1, r1);
      px(4, r0); px(3, r1);
      break;
    default:
      break;
  }
}

void SSD1306Display::translateUTF8ToBlocks(char* dest, const char* src, size_t dest_size) {
  if (dest_size == 0) return;
  size_t j = 0;
  const char* p = src;
  while (*p && j < dest_size - 1) {
    const char* start = p;
    uint32_t cp;
    bool raw;
    if (!nextCodepoint(p, cp, raw)) continue;   // csonka bájt: eldobjuk
    if (raw) {
      dest[j++] = BLOCK_CHAR;
    } else if (cp >= 32 && cp <= 126) {
      dest[j++] = (char)cp;                     // nyomtatható ASCII
    } else if (cp >= 0x80) {
      const size_t len = (size_t)(p - start);
      if (isSupportedAccented(cp) && j + len < dest_size) {
        for (size_t k = 0; k < len; k++) dest[j++] = start[k];   // ékezetes betű marad
      } else {
        dest[j++] = BLOCK_CHAR;                 // nem támogatott: blokk
      }
    }
    // < 32 vezérlőkarakter: eldobjuk (mint az alapimplementációban)
  }
  dest[j] = 0;
}

bool SSD1306Display::begin() {
  if (!_isOn) {
    if (_peripher_power) _peripher_power->claim();
    _isOn = true;
  }
  #ifdef DISPLAY_ROTATION
  display.setRotation(DISPLAY_ROTATION);
  #endif
  bool ok = display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDRESS, true, false) && i2c_probe(Wire, DISPLAY_ADDRESS);
  if (ok) applyContrast();   // a begin() visszaállítja az alapértelmezett kontrasztot
  return ok;
}

// Fényerő = az OLED kontrasztja (0x81 parancs). 100% = az Adafruit alapérték (0xCF).
void SSD1306Display::applyContrast() {
  static const uint8_t CONTRAST[4] = { 0x30, 0x70, 0xA0, 0xCF };   // 25 / 50 / 75 / 100 %
  uint8_t idx = _brightness <= 25 ? 0 : _brightness <= 50 ? 1 : _brightness <= 75 ? 2 : 3;
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(CONTRAST[idx]);
}

void SSD1306Display::setBrightness(uint8_t percent) {
  // támogatott szintek: 25, 50, 75, 100 %
  if (percent < 38)      _brightness = 25;
  else if (percent < 63) _brightness = 50;
  else if (percent < 88) _brightness = 75;
  else                   _brightness = 100;

  if (_isOn) applyContrast();
}

void SSD1306Display::turnOn() {
  if (!_isOn) {
    if (_peripher_power) _peripher_power->claim();
    _isOn = true;  // set before begin() to prevent double claim
    if (_peripher_power) begin();  // re-init display after power was cut
  }
  display.ssd1306_command(SSD1306_DISPLAYON);
  applyContrast();
}

void SSD1306Display::turnOff() {
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  if (_isOn) {
    if (_peripher_power) {
#if PIN_OLED_RESET >= 0
      digitalWrite(PIN_OLED_RESET, LOW);
#endif
      _peripher_power->release();
    }
    _isOn = false;
  }
}

void SSD1306Display::clear() {
  display.clearDisplay();
  display.display();
}

void SSD1306Display::startFrame(ColorVal bkg) {
  display.clearDisplay();  // TODO: apply 'bkg'
  _color = SSD1306_WHITE;
  _text_size = 1;
  display.setTextColor(_color);
  display.setTextSize(1);
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
}

void SSD1306Display::setTextSize(int sz) {
  if (sz < 1) sz = 1;
  _text_size = (uint8_t)sz;
  display.setTextSize(sz);
}

void SSD1306Display::setColor(ColorVal c) {
  _color = c;
  display.setTextColor(_color);
}

void SSD1306Display::setCursor(int x, int y) {
  display.setCursor(x, y);
}

void SSD1306Display::print(const char* str) {
  if (!str) return;

  const char* p = str;
  while (*p) {
    uint32_t cp;
    bool raw;
    if (!nextCodepoint(p, cp, raw)) continue;

    if (cp == '\n' || cp == '\r') {
      display.write((uint8_t)cp);
      continue;
    }

    if (raw) {                       // 0xDB blokk jel
      display.write((uint8_t)BLOCK_CHAR);
      continue;
    }

    uint8_t accent;
    const char base = mapHungarianChar(cp, accent);

    if (accent == ACC_NONE) {
      display.write((uint8_t)base);
      continue;
    }

    // Az Adafruit_GFX a sor végén tördel: az ékezetet a tördelés UTÁNI helyre rajzoljuk.
    const int16_t s = _text_size > 0 ? _text_size : 1;
    int16_t x = display.getCursorX();
    int16_t y = display.getCursorY();
    if (x + 6 * s > display.width()) {
      x = 0;
      y += 8 * s;
    }

    display.write((uint8_t)base);

    const bool upper = (base >= 'A' && base <= 'Z');
    drawAccent(x, y, accent, upper);
  }
}

void SSD1306Display::fillRect(int x, int y, int w, int h) {
  display.fillRect(x, y, w, h, _color);
}

void SSD1306Display::drawRect(int x, int y, int w, int h) {
  display.drawRect(x, y, w, h, _color);
}

void SSD1306Display::drawXbm(int x, int y, const uint8_t* bits, int w, int h) {
  display.drawBitmap(x, y, bits, w, h, _color);
}

uint16_t SSD1306Display::getTextWidth(const char* str) {
  if (!str) return 0;

  // A UTF-8 szöveget alapbetűkre képezzük le, így az ékezetes betű is 1 karakter széles.
  char mapped[256];
  size_t n = 0;
  const char* p = str;
  while (*p && n < sizeof(mapped) - 1) {
    uint32_t cp;
    bool raw;
    if (!nextCodepoint(p, cp, raw)) continue;
    if (raw) { mapped[n++] = BLOCK_CHAR; continue; }
    uint8_t accent;
    mapped[n++] = mapHungarianChar(cp, accent);
  }
  mapped[n] = 0;

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(mapped, 0, 0, &x1, &y1, &w, &h);
  return w;
}

void SSD1306Display::endFrame() {
  display.display();
}
