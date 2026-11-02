#include "ST7735Display.h"

//#include <Fonts/GFXFF/FreeSans9pt7b.h>

// Optimised ST7735 display driver, derived from Adafruit_ST7735 library.

#define ST_CMD_DELAY 0x80

#define ST77XX_NOP       0x00
#define ST77XX_SWRESET   0x01
#define ST77XX_RDDID     0x04
#define ST77XX_RDDST     0x09

#define ST77XX_SLPIN     0x10
#define ST77XX_SLPOUT    0x11
#define ST77XX_PTLON     0x12
#define ST77XX_NORON     0x13

#define ST77XX_INVOFF    0x20
#define ST77XX_INVON     0x21
#define ST77XX_DISPOFF   0x28
#define ST77XX_DISPON    0x29
#define ST77XX_CASET     0x2A
#define ST77XX_RASET     0x2B
#define ST77XX_RAMWR     0x2C
#define ST77XX_RAMRD     0x2E

#define ST77XX_PTLAR     0x30
#define ST77XX_TEOFF     0x34
#define ST77XX_TEON      0x35
#define ST77XX_MADCTL    0x36
#define ST77XX_COLMOD    0x3A

#define ST77XX_MADCTL_MY  0x80
#define ST77XX_MADCTL_MX  0x40
#define ST77XX_MADCTL_MV  0x20
#define ST77XX_MADCTL_ML  0x10
#define ST77XX_MADCTL_RGB 0x00

#define ST77XX_RDID1     0xDA
#define ST77XX_RDID2     0xDB
#define ST77XX_RDID3     0xDC
#define ST77XX_RDID4     0xDD

// Some ready-made 16-bit ('565') color settings:
#define ST77XX_BLACK   0x0000
#define ST77XX_WHITE   0xFFFF
#define ST77XX_RED     0xF800
#define ST77XX_GREEN   0x07E0
#define ST77XX_BLUE    0x001F
#define ST77XX_CYAN    0x07FF
#define ST77XX_MAGENTA 0xF81F
#define ST77XX_YELLOW  0xFFE0
#define ST77XX_ORANGE  0xFC00

// Some flags for initR():
#define INITR_GREENTAB          0x00
#define INITR_REDTAB            0x01
#define INITR_BLACKTAB          0x02
#define INITR_18GREENTAB        INITR_GREENTAB
#define INITR_18REDTAB          INITR_REDTAB
#define INITR_18BLACKTAB        INITR_BLACKTAB
#define INITR_144GREENTAB       0x01
#define INITR_MINI160x80        0x04
#define INITR_HALLOWING         0x05
#define INITR_MINI160x80_PLUGIN 0x06

// Some register settings
#define ST7735_MADCTL_BGR 0x08
#define ST7735_MADCTL_MH  0x04

#define ST7735_FRMCTR1 0xB1
#define ST7735_FRMCTR2 0xB2
#define ST7735_FRMCTR3 0xB3
#define ST7735_INVCTR  0xB4
#define ST7735_DISSET5 0xB6

#define ST7735_PWCTR1 0xC0
#define ST7735_PWCTR2 0xC1
#define ST7735_PWCTR3 0xC2
#define ST7735_PWCTR4 0xC3
#define ST7735_PWCTR5 0xC4
#define ST7735_VMCTR1 0xC5

#define ST7735_PWCTR6 0xFC

#define ST7735_GMCTRP1 0xE0
#define ST7735_GMCTRN1 0xE1

#define ST7735_BLACK   ST77XX_BLACK
#define ST7735_WHITE   ST77XX_WHITE
#define ST7735_RED     ST77XX_RED
#define ST7735_GREEN   ST77XX_GREEN
#define ST7735_BLUE    ST77XX_BLUE
#define ST7735_CYAN    ST77XX_CYAN
#define ST7735_MAGENTA ST77XX_MAGENTA
#define ST7735_YELLOW  ST77XX_YELLOW
#define ST7735_ORANGE  ST77XX_ORANGE

static TFT_eSPI lcd = TFT_eSPI(160, 80);

// -----------------------------------------------------------------------------
// Globális kijelzőváltozók
// Fontos: ezeknek az UTF-8 segédfüggvények előtt kell szerepelniük.
// -----------------------------------------------------------------------------

static uint32_t curr_color = ST77XX_WHITE;
static TFT_eSprite *sprite = NULL;

#define SCALE_X 1.25f
#define SCALE_Y 1.25f

// -----------------------------------------------------------------------------
// T096 UTF-8 és ékezetkezelés
// -----------------------------------------------------------------------------

#if defined(HELTEC_T096)

enum Utf8Accent : uint8_t {
    ACCENT_NONE,
    ACCENT_ACUTE,
    ACCENT_DIAERESIS,
    ACCENT_DOUBLE_ACUTE
};

static uint8_t _text_size = 1;

// UTF-8 karakter dekódolása.
static uint32_t decodeUtf8(const char*& p) {
    if (!p || !*p) {
        return 0;
    }

    const uint8_t c = static_cast<uint8_t>(*p++);

    if (c < 0x80) {
        return c;
    }

    uint32_t codepoint = 0;
    uint8_t remaining = 0;

    if ((c & 0xE0) == 0xC0) {
        codepoint = c & 0x1F;
        remaining = 1;
    } else if ((c & 0xF0) == 0xE0) {
        codepoint = c & 0x0F;
        remaining = 2;
    } else if ((c & 0xF8) == 0xF0) {
        codepoint = c & 0x07;
        remaining = 3;
    } else {
        return '?';
    }

    for (uint8_t i = 0; i < remaining; i++) {
        const uint8_t next = static_cast<uint8_t>(*p);

        if (next == 0 || (next & 0xC0) != 0x80) {
            return '?';
        }

        ++p;
        codepoint = (codepoint << 6) | (next & 0x3F);
    }

    // Érvénytelen Unicode tartományok ellenőrzése.
    if ((remaining == 1 && codepoint < 0x80) ||
        (remaining == 2 && codepoint < 0x800) ||
        (remaining == 3 && codepoint < 0x10000) ||
        codepoint > 0x10FFFF ||
        (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
        return '?';
    }

    return codepoint;
}

// Unicode karakter leképezése ASCII alapbetűre.
static char mapHungarianChar(uint32_t cp, Utf8Accent& accent) {
    accent = ACCENT_NONE;

    switch (cp) {
        // Á / á
        case 0x00C1: accent = ACCENT_ACUTE;        return 'A';
        case 0x00E1: accent = ACCENT_ACUTE;        return 'a';

        // É / é
        case 0x00C9: accent = ACCENT_ACUTE;        return 'E';
        case 0x00E9: accent = ACCENT_ACUTE;        return 'e';

        // Í / í
        case 0x00CD: accent = ACCENT_ACUTE;        return 'I';
        case 0x00ED: accent = ACCENT_ACUTE;        return 'i';

        // Ó / ó
        case 0x00D3: accent = ACCENT_ACUTE;        return 'O';
        case 0x00F3: accent = ACCENT_ACUTE;        return 'o';

        // Ö / ö
        case 0x00D6: accent = ACCENT_DIAERESIS;    return 'O';
        case 0x00F6: accent = ACCENT_DIAERESIS;    return 'o';

        // Ő / ő
        case 0x0150: accent = ACCENT_DOUBLE_ACUTE; return 'O';
        case 0x0151: accent = ACCENT_DOUBLE_ACUTE; return 'o';

        // Ú / ú
        case 0x00DA: accent = ACCENT_ACUTE;        return 'U';
        case 0x00FA: accent = ACCENT_ACUTE;        return 'u';

        // Ü / ü
        case 0x00DC: accent = ACCENT_DIAERESIS;    return 'U';
        case 0x00FC: accent = ACCENT_DIAERESIS;    return 'u';

        // Ű / ű
        case 0x0170: accent = ACCENT_DOUBLE_ACUTE; return 'U';
        case 0x0171: accent = ACCENT_DOUBLE_ACUTE; return 'u';

        // Német Ä / ä
        case 0x00C4: accent = ACCENT_DIAERESIS;    return 'A';
        case 0x00E4: accent = ACCENT_DIAERESIS;    return 'a';

        // Német ß: egy karakterhelyen s-ként jelenik meg.
        case 0x00DF: return 's';

        default:
            if (cp < 0x80) {
                return static_cast<char>(cp);
            }

            return '?';
    }
}

// Ékezet kirajzolása pixelekkel.
//
// A TFT_eSPI alap betűkészletének karaktercelláján belül rajzolunk.
// A karakter alapbetűje már ki van rajzolva, a kurzorpozíciót nem módosítjuk.
static void drawHungarianAccent(
    int16_t x,
    int16_t y,
    Utf8Accent accent
) {
    if (!sprite || accent == ACCENT_NONE) {
        return;
    }

    const int16_t s = (_text_size > 0) ? _text_size : 1;

    // Az ékezetet a karakter felső részéhez igazítjuk.
    // A kurzor Y pozíciója a karaktercella felső széle.
    auto pixel = [&](int16_t px, int16_t py) {
        const int16_t drawX = x + px * s;
        const int16_t drawY = y - s + py * s;

        if (drawY >= 0) {
            sprite->fillRect(
                drawX,
                drawY,
                s,
                s,
                curr_color
            );
        }
    };

    switch (accent) {
        case ACCENT_ACUTE:
            pixel(3, 0);
            pixel(2, 1);
            break;

        case ACCENT_DIAERESIS:
            pixel(1, 0);
            pixel(3, 0);
            break;

        case ACCENT_DOUBLE_ACUTE:
            pixel(1, 0);
            pixel(2, 1);
            pixel(3, 1);
            pixel(4, 0);
            break;

        default:
            break;
    }
}

#endif // HELTEC_T096

// -----------------------------------------------------------------------------
// SPI
// -----------------------------------------------------------------------------

#if defined(HELTEC_LORA_V3) || defined(HELTEC_TRACKER_V2)
static SPIClass tft_spi(HSPI);
#define _spi (&tft_spi)
#else
#define _spi (&SPI1)
#endif

SPISettings _spiSettings = SPISettings(40000000, MSBFIRST, SPI_MODE0);

// -----------------------------------------------------------------------------
// ST7735 inicializálási parancsok
// -----------------------------------------------------------------------------

// clang-format off

static const uint8_t PROGMEM
Bcmd[] = {
    18,

    ST77XX_SWRESET, ST_CMD_DELAY,
    50,

    ST77XX_SLPOUT, ST_CMD_DELAY,
    255,

    ST77XX_COLMOD, 1 + ST_CMD_DELAY,
    0x05,
    10,

    ST7735_FRMCTR1, 3 + ST_CMD_DELAY,
    0x00, 0x06, 0x03,
    10,

    ST77XX_MADCTL, 1,
    0x08,

    ST7735_DISSET5, 2,
    0x15, 0x02,

    ST7735_INVCTR, 1,
    0x00,

    ST7735_PWCTR1, 2 + ST_CMD_DELAY,
    0x02, 0x70,
    10,

    ST7735_PWCTR2, 1,
    0x05,

    ST7735_PWCTR3, 2,
    0x01, 0x02,

    ST7735_VMCTR1, 2 + ST_CMD_DELAY,
    0x3C, 0x38,
    10,

    ST7735_PWCTR6, 2,
    0x11, 0x15,

    ST7735_GMCTRP1, 16,
    0x09, 0x16, 0x09, 0x20,
    0x21, 0x1B, 0x13, 0x19,
    0x17, 0x15, 0x1E, 0x2B,
    0x04, 0x05, 0x02, 0x0E,

    ST7735_GMCTRN1, 16 + ST_CMD_DELAY,
    0x0B, 0x14, 0x08, 0x1E,
    0x22, 0x1D, 0x18, 0x1E,
    0x1B, 0x1A, 0x24, 0x2B,
    0x06, 0x06, 0x02, 0x0F,
    10,

    ST77XX_CASET, 4,
    0x00, 0x02,
    0x00, 0x81,

    ST77XX_RASET, 4,
    0x00, 0x02,
    0x00, 0x81,

    ST77XX_NORON, ST_CMD_DELAY,
    10,

    ST77XX_DISPON, ST_CMD_DELAY,
    255
},

Rcmd1[] = {
    14,

    ST77XX_SLPOUT, ST_CMD_DELAY,
    120,

    ST7735_FRMCTR1, 3,
    0x01, 0x2C, 0x2D,

    ST7735_FRMCTR2, 3,
    0x01, 0x2C, 0x2D,

    ST7735_FRMCTR3, 6,
    0x01, 0x2C, 0x2D,
    0x01, 0x2C, 0x2D,

    ST7735_INVCTR, 1,
    0x07,

    ST7735_PWCTR1, 3,
    0xA2, 0x02, 0x84,

    ST7735_PWCTR2, 1,
    0xC5,

    ST7735_PWCTR3, 2,
    0x0A, 0x00,

    ST7735_PWCTR4, 2,
    0x8A, 0x2A,

    ST7735_PWCTR5, 2,
    0x8A, 0xEE,

    ST7735_VMCTR1, 1,
    0x0E,

    ST77XX_INVOFF, 0,

    ST77XX_MADCTL, 1,
    0xC8,

    ST77XX_COLMOD, 1,
    0x05
},

Rcmd2green[] = {
    2,

    ST77XX_CASET, 4,
    0x00, 0x02,
    0x00, 0x7F + 0x02,

    ST77XX_RASET, 4,
    0x00, 0x01,
    0x00, 0x9F + 0x01
},

Rcmd2red[] = {
    2,

    ST77XX_CASET, 4,
    0x00, 0x00,
    0x00, 0x7F,

    ST77XX_RASET, 4,
    0x00, 0x00,
    0x00, 0x9F
},

Rcmd2green144[] = {
    2,

    ST77XX_CASET, 4,
    0x00, 0x00,
    0x00, 0x7F,

    ST77XX_RASET, 4,
    0x00, 0x00,
    0x00, 0x7F
},

Rcmd2green160x80[] = {
    2,

    ST77XX_CASET, 4,
    0x00, 0x00,
    0x00, 0x4F,

    ST77XX_RASET, 4,
    0x00, 0x00,
    0x00, 0x9F
},

Rcmd2green160x80plugin[] = {
    3,

    ST77XX_INVON, 0,

    ST77XX_CASET, 4,
    0x00, 0x00,
    0x00, 0x4F,

    ST77XX_RASET, 4,
    0x00, 0x00,
    0x00, 0x9F
},

Rcmd2invert[] = {
    1,
    ST77XX_INVON, 0
},

Rcmd3[] = {
    2,

    ST7735_GMCTRP1, 16,
    0x02, 0x1C, 0x07, 0x12,
    0x37, 0x32, 0x29, 0x2D,
    0x29, 0x25, 0x2B, 0x39,
    0x00, 0x01, 0x03, 0x10,

    ST7735_GMCTRN1, 16,
    0x03, 0x1D, 0x07, 0x06,
    0x2E, 0x2C, 0x29, 0x2D,
    0x2E, 0x2E, 0x37, 0x3F,
    0x00, 0x00, 0x02, 0x10
};

// clang-format on

// -----------------------------------------------------------------------------
// Kijelző állapotváltozók
// -----------------------------------------------------------------------------

static int16_t _xstart = 0;
static int16_t _ystart = 0;

static uint8_t _colstart = 0;
static uint8_t _rowstart = 0;

static uint8_t rotation = 0;

static int16_t _width = 0;
static int16_t _height = 0;

// -----------------------------------------------------------------------------
// Alacsony szintű kijelzőműveletek
// -----------------------------------------------------------------------------

static void set_CS(uint8_t level) {
    digitalWrite(PIN_TFT_CS, level);
}

static void sendCommand(uint8_t com) {
    set_CS(HIGH);
    digitalWrite(PIN_TFT_DC, LOW);
    set_CS(LOW);

    _spi->beginTransaction(_spiSettings);
    _spi->transfer(com);
    _spi->endTransaction();

    set_CS(HIGH);
    digitalWrite(PIN_TFT_DC, HIGH);
}

static void WriteData(uint8_t data) {
    digitalWrite(PIN_TFT_CS, LOW);

    _spi->beginTransaction(_spiSettings);
    _spi->transfer(data);
    _spi->endTransaction();

    digitalWrite(PIN_TFT_CS, HIGH);
}

static void SPI_WRITE32(uint32_t l) {
    _spi->transfer(l >> 24);
    _spi->transfer(l >> 16);
    _spi->transfer(l >> 8);
    _spi->transfer(l);
}

static void writeCommand(uint8_t cmd) {
    digitalWrite(PIN_TFT_DC, LOW);
    _spi->transfer(cmd);
    digitalWrite(PIN_TFT_DC, HIGH);
}

static void displayInit(const uint8_t *addr) {
    uint8_t numCommands;
    uint8_t cmd;
    uint8_t numArgs;
    uint16_t ms;

    numCommands = pgm_read_byte(addr++);

    while (numCommands--) {
        cmd = pgm_read_byte(addr++);
        numArgs = pgm_read_byte(addr++);

        ms = numArgs & ST_CMD_DELAY;
        numArgs &= ~ST_CMD_DELAY;

        sendCommand(cmd);

        for (int k = 0; k < numArgs; k++) {
            WriteData(addr[k]);
        }

        addr += numArgs;

        if (ms) {
            ms = pgm_read_byte(addr++);

            if (ms == 255) {
                ms = 500;
            }

            delay(ms);
        }
    }
}

static void setRotation(uint8_t m) {
    uint8_t madctl = 0;

    rotation = m & 3;

    switch (rotation) {
        case 0:
            madctl = ST77XX_MADCTL_MX |
                     ST77XX_MADCTL_MY |
                     ST7735_MADCTL_BGR;

            _height = 160;
            _width = 80;
            _xstart = _colstart;
            _ystart = _rowstart;
            break;

        case 1:
            madctl = ST77XX_MADCTL_MY |
                     ST77XX_MADCTL_MV |
                     ST7735_MADCTL_BGR;

            _width = 160;
            _height = 80;
            _ystart = _colstart;
            _xstart = _rowstart;
            break;

        case 2:
            madctl = ST7735_MADCTL_BGR;

            _height = 160;
            _width = 80;
            _xstart = _colstart;
            _ystart = _rowstart;
            break;

        case 3:
            madctl = ST77XX_MADCTL_MX |
                     ST77XX_MADCTL_MV |
                     ST7735_MADCTL_BGR;

            _width = 160;
            _height = 80;
            _ystart = _colstart;
            _xstart = _rowstart;
            break;
    }

    sendCommand(ST77XX_MADCTL);
    WriteData(madctl);
}

static void setAddrWindow(
    uint16_t x,
    uint16_t y,
    uint16_t w,
    uint16_t h
) {
    x += _xstart;
    y += _ystart;

    uint32_t xa = ((uint32_t)x << 16) | (x + w - 1);
    uint32_t ya = ((uint32_t)y << 16) | (y + h - 1);

    writeCommand(ST77XX_CASET);
    SPI_WRITE32(xa);

    writeCommand(ST77XX_RASET);
    SPI_WRITE32(ya);

    writeCommand(ST77XX_RAMWR);
}

// -----------------------------------------------------------------------------
// Display interface
// -----------------------------------------------------------------------------

bool ST7735Display::i2c_probe(TwoWire& wire, uint8_t addr) {
    (void)wire;
    (void)addr;

    return true;

    /*
    wire.beginTransmission(addr);
    uint8_t error = wire.endTransmission();
    return (error == 0);
    */
}

#ifndef PIN_TFT_LEDA_CTL_ACTIVE
#define PIN_TFT_LEDA_CTL_ACTIVE HIGH
#endif

// -----------------------------------------------------------------------------
// Dark theme
// -----------------------------------------------------------------------------

ColorVal UIColor::window_bkg    = 0x2104;
ColorVal UIColor::title_bkg     = 0x39C7;
ColorVal UIColor::title_txt     = 0xFFFF;
ColorVal UIColor::primary_txt   = 0xFFFF;
ColorVal UIColor::secondary_txt = 0xFFFF;
ColorVal UIColor::warning_txt   = 0xFFE0;
ColorVal UIColor::popup_bkg     = 0xF79E;
ColorVal UIColor::popup_txt     = 0x39C7;
ColorVal UIColor::corp_blue     = 0x03EF;
ColorVal UIColor::selected_bkg  = 0x03FB;
ColorVal UIColor::selected_txt  = 0xFFFF;

// -----------------------------------------------------------------------------
// Display begin
// -----------------------------------------------------------------------------

bool ST7735Display::begin() {
    if (!sprite) {
        sprite = new TFT_eSprite(&lcd);

        if (sprite) {
            if (sprite->createSprite(160, 80)) {
                sprite->fillScreen(ST77XX_BLACK);
                sprite->setTextColor(curr_color = ST77XX_WHITE);
            } else {
                Serial.printf(
                    "ST7735Display: failed to alloc canvas pixels"
                );
            }
        } else {
            Serial.printf(
                "ST7735Display: failed to alloc canvas"
            );
        }
    }

    if (!sprite) {
        return false;
    }

    if (!sprite->getPointer()) {
        return false;
    }

    if (!_isOn) {
        if (_peripher_power) {
            _peripher_power->claim();
        }

        delay(100);

        pinMode(PIN_TFT_RST, OUTPUT);
        pinMode(PIN_TFT_CS, OUTPUT);
        pinMode(PIN_TFT_DC, OUTPUT);
        pinMode(PIN_TFT_LEDA_CTL, OUTPUT);

#ifdef ESP_PLATFORM
        _spi->begin(
            PIN_TFT_SCL,
            -1,
            PIN_TFT_SDA,
            -1
        );
#else
        _spi->begin();
#endif

        _spi->setClockDivider(SPI_CLOCK_DIV2);

        _height = 80;
        _width = 160;

#if defined(HELTEC_LORA_V3)
        _colstart = 26;
        _rowstart = 1;
#else
        _colstart = 24;
        _rowstart = 0;
#endif

        _resetAndInit();

        sendCommand(ST77XX_DISPON);

        _isOn = true;
    }

    return true;
}

// -----------------------------------------------------------------------------
// Reset and initialization
// -----------------------------------------------------------------------------

void ST7735Display::_resetAndInit() {
    digitalWrite(PIN_TFT_RST, HIGH);
    delay(2);

    digitalWrite(PIN_TFT_RST, LOW);
    delay(10);

    digitalWrite(PIN_TFT_RST, HIGH);
    delay(2);

    displayInit(Rcmd1);

#if defined(HELTEC_TRACKER_V2) || defined(HELTEC_T096)

    displayInit(Rcmd2green160x80);

#elif defined(HELTEC_LORA_V3)

    displayInit(Rcmd2invert);

#endif

    displayInit(Rcmd3);

    setRotation(DISPLAY_ROTATION);

    sprite->fillScreen(ST77XX_BLACK);

    endFrame();

    digitalWrite(
        PIN_TFT_LEDA_CTL,
        PIN_TFT_LEDA_CTL_ACTIVE
    );
}

// -----------------------------------------------------------------------------
// Power management
// -----------------------------------------------------------------------------

void ST7735Display::turnOn() {
    if (!_isOn) {
        if (_peripher_power) {
            _peripher_power->claim();
        }

        _resetAndInit();

        sendCommand(ST77XX_DISPON);

        _isOn = true;
    }
}

void ST7735Display::turnOff() {
    if (_isOn) {
        sendCommand(ST77XX_DISPOFF);

        digitalWrite(
            PIN_TFT_LEDA_CTL,
            !PIN_TFT_LEDA_CTL_ACTIVE
        );

        _isOn = false;

        if (_peripher_power) {
            _peripher_power->release();
        }
    }
}

// -----------------------------------------------------------------------------
// Drawing
// -----------------------------------------------------------------------------

void ST7735Display::clear() {
    if (sprite) {
        sprite->fillScreen(ST77XX_BLACK);
    }
}

void ST7735Display::startFrame(ColorVal bkg) {
    if (!sprite) {
        return;
    }

    sprite->fillScreen(bkg);

    sprite->setTextColor(
        curr_color = UIColor::primary_txt
    );

    sprite->setFreeFont();

#if defined(HELTEC_T096)
    _text_size = 1;
#endif

    sprite->setTextSize(1);
}

void ST7735Display::setTextSize(int sz) {
    if (!sprite || sz <= 0) {
        return;
    }

#if defined(HELTEC_T096)
    _text_size = static_cast<uint8_t>(sz);
#endif

    sprite->setTextSize(sz);
}

void ST7735Display::setColor(ColorVal c) {
    if (!sprite) {
        return;
    }

    curr_color = c;
    sprite->setTextColor(curr_color);
}

void ST7735Display::setCursor(int x, int y) {
    if (!sprite) {
        return;
    }

    sprite->setCursor(
        static_cast<int16_t>(x * SCALE_X),
        static_cast<int16_t>(y * SCALE_Y)
    );
}

// -----------------------------------------------------------------------------
// UTF-8 print
// -----------------------------------------------------------------------------

void ST7735Display::print(const char* str) {
    if (!sprite || !str) {
        return;
    }

#if defined(HELTEC_T096)

    const char* p = str;

    while (*p) {
        const uint32_t cp = decodeUtf8(p);

        if (cp == '\n') {
            sprite->write('\n');
            continue;
        }

        if (cp == '\r') {
            continue;
        }

        Utf8Accent accent;
        const char base = mapHungarianChar(cp, accent);

        const int16_t x = sprite->getCursorX();
        const int16_t y = sprite->getCursorY();

        // ASCII alapbetű kirajzolása.
        sprite->write(static_cast<uint8_t>(base));

        // Ékezet kirajzolása.
        if (accent != ACCENT_NONE) {
            drawHungarianAccent(x, y, accent);
        }
    }

#else

    sprite->print(str);

#endif
}

// -----------------------------------------------------------------------------
// Geometriai rajzolás
// -----------------------------------------------------------------------------

void ST7735Display::fillRect(
    int x,
    int y,
    int w,
    int h
) {
    if (!sprite) {
        return;
    }

    sprite->fillRect(
        x * SCALE_X,
        y * SCALE_Y,
        w * SCALE_X,
        h * SCALE_Y,
        curr_color
    );
}

void ST7735Display::drawRect(
    int x,
    int y,
    int w,
    int h
) {
    if (!sprite) {
        return;
    }

    sprite->drawRect(
        x * SCALE_X,
        y * SCALE_Y,
        w * SCALE_X,
        h * SCALE_Y,
        curr_color
    );
}

void ST7735Display::drawXbm(
    int x,
    int y,
    const uint8_t* bits,
    int w,
    int h
) {
    if (!sprite || !bits) {
        return;
    }

    sprite->drawBitmap(
        x * SCALE_X,
        y * SCALE_Y,
        bits,
        w,
        h,
        curr_color
    );
}

// -----------------------------------------------------------------------------
// UTF-8 text width
// -----------------------------------------------------------------------------

uint16_t ST7735Display::getTextWidth(const char* str) {
    if (!sprite || !str) {
        return 0;
    }

#if defined(HELTEC_T096)

    const char* p = str;

    uint16_t width = 0;
    uint16_t line_width = 0;

    while (*p) {
        const uint32_t cp = decodeUtf8(p);

        if (cp == '\n') {
            if (line_width > width) {
                width = line_width;
            }

            line_width = 0;
            continue;
        }

        if (cp == '\r') {
            continue;
        }

        Utf8Accent accent;
        const char base = mapHungarianChar(cp, accent);

        char temp[2] = {
            base,
            '\0'
        };

        line_width += sprite->textWidth(temp);
    }

    if (line_width > width) {
        width = line_width;
    }

    return static_cast<uint16_t>(width / SCALE_X);

#else

    return sprite->textWidth(str) / SCALE_X;

#endif
}

// -----------------------------------------------------------------------------
// Framebuffer transfer
// -----------------------------------------------------------------------------

void ST7735Display::endFrame() {
    if (!sprite || !sprite->getPointer()) {
        return;
    }

    set_CS(LOW);

    _spi->beginTransaction(_spiSettings);

    uint16_t* pixels = static_cast<uint16_t*>(
        sprite->getPointer()
    );

    for (uint16_t y = 0; y < 80; y++, pixels += 160) {
        setAddrWindow(0, y, 160, 1);

#ifdef ESP_PLATFORM

        _spi->transferBytes(
            reinterpret_cast<uint8_t*>(pixels),
            NULL,
            2 * 160
        );

#else

        _spi->transfer(
            pixels,
            NULL,
            2 * 160
        );

#endif
    }

    _spi->endTransaction();

    set_CS(HIGH);
}
