#include "I18n.h"
#include "UIStorage.h"

#include <Arduino.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

// ==================================================
// HellCore – többnyelvű UI
//
// Nyelvek:
//   EN – English
//   HU – Magyar
//   DE – Deutsch
//
// Konfigurációs fájl:
//   /ui.conf
//
// Formátum:
//   [ui]
//   language=en
//
// A fájlkezelést a közös UIStorage végzi.
// ==================================================

#define LANG_CONFIG_FILE "/ui.conf"

static Lang g_lang = Lang::EN;


// ==================================================
// FORDÍTÁSI TÁBLÁZAT
// ==================================================

struct Row {
    const char* en;
    const char* hu;
    const char* de;
};

static const Row ROWS[] = {

    { "< Back",                  "< Vissza",                        "< Zurück" },
    { "< Connected >",           "< Csatlakozva >",                 "< Verbunden >" },

    { "Advert failed..",         "Hirdetés sikertelen..",           "Anzeige fehlgeschlagen.." },
    { "Advert sent!",            "Hirdetés elküldve!",              "Anzeige gesendet!" },
    { "advert: long press",      "hirdetés: hosszú nyomás",         "Anzeige: lang drücken" },
    { "advert: press Enter",     "hirdetés: Enter",                 "Anzeige: Enter" },
    { "Altitude",                "Magasság",                        "Höhe" },
    { "Automatic",               "Automatikus",                     "Automatisch" },
    { "Available pins",          "Elérhető lábak",                  "Verfügbare Pins" },

    { "Back",                    "Vissza",                          "Zurück" },
    { "Bluetooth",               "Bluetooth",                       "Bluetooth" },
    { "Boost RX",                "Vételi erősítés",                 "Empfangsverstärkung" },
    { "Buzzer",                  "Hangjelző",                       "Summer" },
    { "Buzzer pins",             "Hangjelző lábai",                  "Summer-Pins" },
    { "Buzzer: OFF",             "Hangjelző: KI",                   "Summer: AUS" },
    { "Buzzer: ON",              "Hangjelző: BE",                   "Summer: EIN" },
    { "Brightness",              "Fényerő",                         "Helligkeit" },

    { "Calibrate",               "Kalibrálás",                      "Kalibrieren" },
    { "Can't access GPS",        "A GPS nem érhető el",             "Kein Zugriff auf GPS" },
    { "Clock",                   "Óra",                             "Uhr" },
    { "Companion Spectrum",      "Companion spektrum",              "Companion-Spektrum" },
    { "Compass",                 "Iránytű",                         "Kompass" },
    { "Connected",               "Csatlakozva",                     "Verbunden" },
    { "Current",                 "Áramerősség",                     "Stromstärke" },

    { "Default",                 "Alapértelmezett",                 "Standard" },
    { "DHT11",                   "DHT11",                           "DHT11" },
    { "DHT11 pin",               "DHT11 lába",                      "DHT11-Pin" },
    { "Disabled",                "Letiltva",                        "Deaktiviert" },
    { "Disabled",                "Letiltva",                        "Deaktiviert" },
    { "Disconnected",            "Nincs kapcsolat",                 "Getrennt" },
    { "Discover",                "Felderítés",                      "Erkennung" },
    { "discover: long press",    "felderítés: hosszú nyomás",       "Erkennung: lang drücken" },
    { "discover: press Enter",   "felderítés: Enter",               "Erkennung: Enter" },

    { "Enabled",                 "Engedélyezve",                    "Aktiviert" },

    { "Flood advert",            "Flood hirdetés",                  "Flood-Ankündigung" },

    { "GPS",                     "GPS",                             "GPS" },
    { "GPS fix",                 "GPS fix",                         "GPS-Fix" },
    { "GPS off",                 "GPS ki",                          "GPS ausgeschaltet" },
    { "GPS off (hardware)",      "GPS ki (hardver)",                "GPS aus (Hardware)" },
    { "GPS off (software)",      "GPS ki (szoftver)",               "GPS aus (Software)" },
    { "GPS on",                  "GPS be",                          "GPS eingeschaltet" },
    { "GPS: Disabled",           "GPS: Letiltva",                   "GPS: Deaktiviert" },
    { "GPS: Enabled",            "GPS: Engedélyezve",               "GPS: Aktiviert" },

    { "Hardware",                "Hardver",                         "Hardware" },
    { "Hibernate",               "Hibernálás",                      "Ruhezustand" },
    { "hibernate: long press",   "hibernálás: hosszú nyomás",       "Ruhezustand: lang drücken" },
    { "hibernate: press Enter",  "hibernálás: Enter",               "Ruhezustand: Enter" },
    { "Hibernating...",          "Hibernálás...",                   "Ruhezustand..." },
    { "Home",                    "Főoldal",                         "Start" },
    { "Humidity",                "Páratartalom",                    "Luftfeuchte" },

    { "I2C SDA",                 "I2C SDA",                         "I2C SDA" },
    { "I2C SCL",                 "I2C SCL",                         "I2C SCL" },
    { "I2C sensor",              "I2C érzékelő",                    "I2C-Sensor" },
    { "Input pin",               "Bemeneti láb",                    "Eingangspin" },
    { "Input pins",              "Bemeneti lábak",                  "Eingangspins" },

    { "Keep on USB",             "Bekapcsolva USB-n",               "Bei USB eingeschaltet" },

    { "Language",                "Nyelv",                           "Sprache" },
    { "LNA",                     "LNA",                             "LNA" },
    { "Long press: Toggle",      "Hosszú nyomás: váltás",           "Lang drücken: Umschalten" },
    { "Low Battery.",            "Alacsony akkumulátortöltöttség.", "Niedriger Akkustand." },
    { "Low memory",              "Kevés memória",                   "Wenig Speicher" },

    { "Memory OK",               "Memória rendben",                 "Speicher OK" },
    { "Modules",                 "Modulok",                         "Module" },
    { "Msg wake",                "Üzenetre ébresztés",              "Bei Nachricht aufwecken" },
    { "MSG: %d",                 "Üzenetek: %d",                    "NACHR.: %d" },

    { "No GPS fix",              "Nincs GPS-helyzet",               "Kein GPS-Fix" },
    { "No module",               "Nincs modul",                     "Kein Modul" },
    { "No options",              "Nincs választható elem",          "Keine Optionen" },
    { "No pin",                  "Nincs láb",                       "Kein Pin" },
    { "No sensor",               "Nincs érzékelő",                  "Kein Sensor" },
    { "No sensor selected",      "Nincs érzékelő kiválasztva",      "Kein Sensor ausgewählt" },
    { "Noise floor: %d",         "Zajszint: %d",                    "Rauschpegel: %d" },

    { "Off",                     "Ki",                              "Aus" },
    { "OK",                      "OK",                              "OK" },
    { "On",                      "Be",                              "Ein" },
    { "open: long press",        "megnyitás: hosszú nyomás",        "öffnen: lang drücken" },
    { "open: press Enter",       "megnyitás: Enter",                "öffnen: Enter" },
    { "Output pin",              "Kimeneti láb",                    "Ausgangspin" },

    { "Pin assignment",          "Lábkiosztás",                     "Pinbelegung" },
    { "Pin in use",              "Foglalt láb",                     "Pin belegt" },
    { "Pins",                    "Lábkiosztás",                     "Pinbelegung" },
    { "Position",                "Pozíció",                         "Position" },
    { "Power",                   "Teljesítmény",                    "Leistung" },
    { "Power off",               "Kikapcsolás",                     "Ausschalten" },
    { "Pressure",                "Légnyomás",                       "Luftdruck" },

    { "Recent adverts",          "Legutóbbi hirdetések",            "Letzte Anzeigen" },
    { "Reboot",                  "Újraindítás",                     "Neustart" },
    { "Repeater discovery",      "Repeater-felderítés",             "Repeater-Erkennung" },
    { "RSSI: %.1f dBm",          "RSSI: %.1f dBm",                  "RSSI: %.1f dBm" },
    { "RSSI: waiting...",        "RSSI: várakozás...",              "RSSI: warte..." },

    { "Satellites",              "Műholdak",                        "Satelliten" },
    { "Screen timeout",          "Képernyő időkorlát",              "Bildschirm-Timeout" },
    { "Select",                  "Kiválasztás",                     "Auswählen" },
    { "Select sensor",           "Érzékelő kiválasztása",            "Sensor auswählen" },
    { "Send advert",             "Hirdetés küldése",                "Anzeige senden" },
    { "Sensor type",             "Érzékelő típusa",                 "Sensortyp" },
    { "Sensors",                 "Szenzorok",                       "Sensoren" },
    { "Settings",                "Beállítások",                     "Einstellungen" },
    { "Share position",          "Pozíció megosztása",              "Position teilen" },
    { "Short press: Next",       "Rövid nyomás: tovább",            "Kurz drücken: Weiter" },
    { "Shutting Down!",          "Leállítás!",                      "Wird heruntergefahren!" },
    { "Signal",                  "Jel",                             "Signal" },
    { "Sound",                   "Hang",                            "Ton" },
    { "Spectrum",                "Spektrum",                        "Spektrum" },

    { "Temperature",             "Hőmérséklet",                     "Temperatur" },
    { "Terminal",                "Terminál",                        "Terminal" },
    { "Theme",                   "Téma",                            "Design" },
    { "Time not set",             "Idő nincs beállítva",             "Zeit nicht gesetzt" },
    { "Time zone",               "Időzóna",                         "Zeitzone" },
    { "toggle: long press",      "váltás: hosszú nyomás",           "umschalten: lang drücken" },
    { "toggle: press Enter",     "váltás: Enter",                   "umschalten: Enter" },
    { "TX power",                "TX teljesítmény",                 "TX-Leistung" },

    { "Unavailable pin",         "Nem használható láb",             "Nicht verfügbarer Pin" },
    { "Units",                   "Mértékegységek",                  "Einheiten" },
    { "Unknown",                 "Ismeretlen",                      "Unbekannt" },
    { "Unread Msg: %d",          "Olvasatlan üzenet: %d",           "Ungelesene Nachricht: %d" },
    { "Unread: %d",              "Olvasatlan: %d",                  "Ungelesen: %d" },

    { "Vibration",               "Rezgés",                          "Vibration" },
    { "Vibra",                   "Rezgés",                          "Vibration" },
    { "Vibra motor",             "Rezgőmotor",                      "Vibrationsmotor" },
    { "Voltage",                 "Feszültség",                      "Spannung" },

    { "Wake on message",         "Ébresztés üzenetre",              "Bei Nachricht aufwecken" },

    { "Apr",                     "ápr",                             "Apr" },
    { "Aug",                     "aug",                             "Aug" },
    { "Dec",                     "dec",                             "Dez" },
    { "Feb",                     "feb",                             "Feb" },
    { "Jan",                     "jan",                             "Jan" },
    { "Jul",                     "júl",                             "Jul" },
    { "Jun",                     "jún",                             "Jun" },
    { "Mar",                     "márc",                            "Mär" },
    { "May",                     "máj",                             "Mai" },
    { "Nov",                     "nov",                             "Nov" },
    { "Oct",                     "okt",                             "Okt" },
    { "Sep",                     "szept",                           "Sep" }
};


// ==================================================
// NYELVNEVEK
// ==================================================

static const char* const NAMES[(int)Lang::COUNT] = {
    "English",
    "Magyar",
    "Deutsch"
};


// ==================================================
// NYELVKÓD
// ==================================================

static const char* languageCode(Lang lang) {

    switch (lang) {

        case Lang::HU:
            return "hu";

        case Lang::DE:
            return "de";

        case Lang::EN:
        default:
            return "en";
    }

}


// ==================================================
// KONFIGURÁCIÓ ELŐÁLLÍTÁSA
// ==================================================

static bool writeConfig() {

    char config[64];

    int length = snprintf(
        config,
        sizeof(config),
        "[ui]\nlanguage=%s\n",
        languageCode(g_lang)
    );

    if (
        length <= 0 ||
        (size_t)length >= sizeof(config)
    ) {
        return false;
    }

    return UIStorage::write(
        LANG_CONFIG_FILE,
        reinterpret_cast<const uint8_t*>(config),
        (size_t)length
    );

}


// ==================================================
// KONFIGURÁCIÓ FELDOLGOZÁSA
// ==================================================

static bool readLanguageConfig(
    const uint8_t* data,
    size_t length,
    Lang& result
) {

    if (!data || length == 0) {
        return false;
    }

    char line[64];

    size_t offset = 0;

    bool foundLanguage = false;

    while (offset < length) {

        size_t lineLength = 0;

        while (
            offset < length &&
            data[offset] != '\n' &&
            lineLength < sizeof(line) - 1
        ) {
            line[lineLength++] = (char)data[offset++];
        }

        if (offset < length && data[offset] != '\n') {
            return false;
        }

        if (offset < length && data[offset] == '\n') {
            ++offset;
        }

        line[lineLength] = '\0';

        // CR eltávolítása.

        while (
            lineLength > 0 &&
            (
                line[lineLength - 1] == '\r' ||
                line[lineLength - 1] == ' ' ||
                line[lineLength - 1] == '\t'
            )
        ) {
            line[--lineLength] = '\0';
        }

        // Sor eleji szóközök eltávolítása.

        char* p = line;

        while (*p == ' ' || *p == '\t') {
            ++p;
        }

        // Üres sor és szekció fejléc kihagyása.

        if (
            *p == '\0' ||
            *p == '#' ||
            *p == ';' ||
            *p == '['
        ) {
            continue;
        }

        if (strncmp(p, "language=", 9) == 0) {

            Lang parsed;

            if (!I18n::parse(p + 9, parsed)) {
                return false;
            }

            result = parsed;
            foundLanguage = true;
        }

    }

    return foundLanguage;

}


// ==================================================
// PRINTF FORMÁTUM ELLENŐRZÉS
// ==================================================

static bool nextSpec(
    const char*& p,
    char out[8]
) {

    for (;;) {

        while (*p && *p != '%') {
            ++p;
        }

        if (!*p) {
            return false;
        }

        ++p;

        if (*p == '%') {
            ++p;
            continue;
        }

        int n = 0;

        while (
            *p &&
            !strchr("diouxXeEfFgGaAcspn", *p)
        ) {

            if (
                strchr("hljztL*", *p) &&
                n < 6
            ) {
                out[n++] = *p;
            }

            ++p;
        }

        if (*p) {

            if (n < 7) {
                out[n++] = *p;
            }

            ++p;
        }

        out[n] = '\0';

        return true;

    }

}


static bool specsMatch(
    const char* a,
    const char* b
) {

    const char* pa = a;
    const char* pb = b;

    for (;;) {

        char sa[8];
        char sb[8];

        bool ha = nextSpec(pa, sa);
        bool hb = nextSpec(pb, sb);

        if (ha != hb) {
            return false;
        }

        if (!ha) {
            return true;
        }

        if (strcmp(sa, sb) != 0) {
            return false;
        }

    }

}


// ==================================================
// ÉKEZETEK ASCII-RA EGYSZERŰSÍTÉSE
//
// A Heltec T096 (ST7735) és a Heltec V3 (SSD1306) driverében van magyar ékezetrajzolás.
// Az egyszínű kijelzők (pl. Heltec T114) betűkészlete
// nem tartalmazza az ő/ű és sok más ékezetes karaktert, ezért ott
// az UTF-8 szöveg "kérdőjeles / szemetes" lenne. Ezeken az eszközökön
// a fordítás ékezet nélküli (ASCII) alakban jelenik meg.
//
// Kikapcsolható: -D I18N_KEEP_ACCENTS
// ==================================================

// A Heltec V3 (SSD1306) driverében is van magyar ékezetrajzolás, ezért ott sem egyszerűsítünk.
#if !defined(HELTEC_T096) && !defined(HELTEC_LORA_V3) && !defined(I18N_KEEP_ACCENTS)
  #define I18N_ASCII_FOLD 1
#endif

#ifdef I18N_ASCII_FOLD

static const int FOLD_BUFFERS = 6;
static const int FOLD_BUF_SIZE = 96;

static bool hasNonAscii(const char* s) {
    for (; *s; ++s) {
        if ((unsigned char)*s >= 0x80) {
            return true;
        }
    }
    return false;
}

// Egy kódpont ASCII megfelelője; visszaadja a kiírt karakterek számát.
static int foldCodepoint(uint32_t cp, char out[3]) {
    const bool de = (g_lang == Lang::DE);

    switch (cp) {
        case 0xE1: case 0xE0: case 0xE2: out[0] = 'a'; return 1;
        case 0xC1: case 0xC0: case 0xC2: out[0] = 'A'; return 1;
        case 0xE9: case 0xE8: case 0xEA: out[0] = 'e'; return 1;
        case 0xC9: case 0xC8: case 0xCA: out[0] = 'E'; return 1;
        case 0xED: case 0xEC: case 0xEE: out[0] = 'i'; return 1;
        case 0xCD: case 0xCC: case 0xCE: out[0] = 'I'; return 1;
        case 0xF3: case 0xF2: case 0xF4: case 0x151:
            out[0] = 'o'; return 1;
        case 0xD3: case 0xD2: case 0xD4: case 0x150:
            out[0] = 'O'; return 1;
        case 0xFA: case 0xF9: case 0xFB: case 0x171:
            out[0] = 'u'; return 1;
        case 0xDA: case 0xD9: case 0xDB: case 0x170:
            out[0] = 'U'; return 1;
        case 0xE4: out[0] = 'a'; if (de) { out[1] = 'e'; return 2; } return 1;
        case 0xC4: out[0] = 'A'; if (de) { out[1] = 'e'; return 2; } return 1;
        case 0xF6: out[0] = 'o'; if (de) { out[1] = 'e'; return 2; } return 1;
        case 0xD6: out[0] = 'O'; if (de) { out[1] = 'e'; return 2; } return 1;
        case 0xFC: out[0] = 'u'; if (de) { out[1] = 'e'; return 2; } return 1;
        case 0xDC: out[0] = 'U'; if (de) { out[1] = 'e'; return 2; } return 1;
        case 0xDF: out[0] = 's'; out[1] = 's'; return 2;
        default:   out[0] = '?'; return 1;
    }
}

static const char* foldAscii(const char* text) {

    if (!text || !hasNonAscii(text)) {
        return text;
    }

    static char buffers[FOLD_BUFFERS][FOLD_BUF_SIZE];
    static int next = 0;

    char* out = buffers[next];
    next = (next + 1) % FOLD_BUFFERS;

    size_t o = 0;

    for (size_t i = 0; text[i] && o < FOLD_BUF_SIZE - 3; ) {

        unsigned char c = (unsigned char)text[i];

        if (c < 0x80) {
            out[o++] = (char)c;
            ++i;
            continue;
        }

        uint32_t cp = 0;
        size_t len = 1;

        if ((c & 0xE0) == 0xC0) {
            len = 2;
            cp = c & 0x1F;
        } else if ((c & 0xF0) == 0xE0) {
            len = 3;
            cp = c & 0x0F;
        } else if ((c & 0xF8) == 0xF0) {
            len = 4;
            cp = c & 0x07;
        }

        bool valid = true;

        for (size_t k = 1; k < len; ++k) {
            unsigned char cc = (unsigned char)text[i + k];

            if ((cc & 0xC0) != 0x80) {
                valid = false;
                len = k;
                break;
            }

            cp = (cp << 6) | (cc & 0x3F);
        }

        char tmp[3] = {0, 0, 0};
        int n = valid ? foldCodepoint(cp, tmp) : 1;

        if (!valid) {
            tmp[0] = '?';
        }

        for (int k = 0; k < n; ++k) {
            out[o++] = tmp[k];
        }

        i += len;
    }

    out[o] = '\0';

    return out;
}

#else

static const char* foldAscii(const char* text) {
    return text;
}

#endif

// ==================================================
// FORDÍTÁS KERESÉSE
// ==================================================

const char* i18n::tr(const char* en) {

    if (!en) {
        return "";
    }

    if (g_lang == Lang::EN) {
        return en;
    }

    for (
        size_t i = 0;
        i < sizeof(ROWS) / sizeof(ROWS[0]);
        ++i
    ) {

        if (strcmp(en, ROWS[i].en) != 0) {
            continue;
        }

        const char* translated =
            (g_lang == Lang::HU)
                ? ROWS[i].hu
                : ROWS[i].de;

        if (!translated || !*translated) {
            return en;
        }

        // A printf formátumparamétereknek egyezniük kell.

        if (
            strchr(en, '%') &&
            !specsMatch(en, translated)
        ) {
            return en;
        }

        return translated;

    }

    // Hiányzó fordítás esetén az angol szöveg marad.

    return en;

}


// ==================================================
// NYELV BEÁLLÍTÁSA
// ==================================================

void I18n::setLang(Lang l) {

    if ((uint8_t)l < (uint8_t)Lang::COUNT) {
        g_lang = l;
    }

}


// ==================================================
// AKTUÁLIS NYELV
// ==================================================

Lang I18n::lang() {

    return g_lang;

}


// ==================================================
// NYELV MEGNEVEZÉSE
// ==================================================

const char* I18n::langName(Lang l) {

    return (
        (uint8_t)l < (uint8_t)Lang::COUNT
    )
        ? NAMES[(uint8_t)l]
        : "";

}


// ==================================================
// NYELVKÓD FELDOLGOZÁSA
// ==================================================

bool I18n::parse(
    const char* s,
    Lang& l
) {

    if (!s) {
        return false;
    }

    if (!strcasecmp(s, "en")) {
        l = Lang::EN;
        return true;
    }

    if (!strcasecmp(s, "hu")) {
        l = Lang::HU;
        return true;
    }

    if (!strcasecmp(s, "de")) {
        l = Lang::DE;
        return true;
    }

    return false;

}


// ==================================================
// NYELV BETÖLTÉSE
// ==================================================

void I18n::load() {

    // Biztonságos alapértelmezett nyelv.

    g_lang = Lang::EN;

    // A UIStorage végzi a megfelelő platform
    // fájlrendszerének inicializálását.

    if (!UIStorage::begin()) {
        return;
    }

    uint8_t buffer[64];

    size_t bytesRead = 0;

    // Konfiguráció beolvasása.

    if (
        !UIStorage::read(
            LANG_CONFIG_FILE,
            buffer,
            sizeof(buffer),
            bytesRead
        )
    ) {

        // Első indítás vagy hiányzó fájl:
        // létrehozzuk az alapértelmezett angol beállítást.

        g_lang = Lang::EN;

        writeConfig();

        return;
    }

    Lang loaded = Lang::EN;

    if (
        readLanguageConfig(
            buffer,
            bytesRead,
            loaded
        )
    ) {

        g_lang = loaded;

    } else {

        // Hibás konfiguráció esetén angol alapértelmezés.

        g_lang = Lang::EN;

        writeConfig();

    }

}


// ==================================================
// NYELV MENTÉSE
// ==================================================

void I18n::save() {

    // A kiválasztott nyelv mentése a közös
    // UIStorage rétegen keresztül.

    if (!UIStorage::begin()) {
        return;
    }

    writeConfig();

}
