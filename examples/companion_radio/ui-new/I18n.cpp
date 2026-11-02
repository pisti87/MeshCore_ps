
#include "I18n.h"

#include <Arduino.h>
#include <InternalFileSystem.h>
#include <string.h>
#include <strings.h>

using namespace Adafruit_LittleFS_Namespace;

// Belső konfigurációs fájl az InternalFS tárhelyen.
#define LANG_CONFIG_FILE "/ui.conf"

static Lang g_lang = Lang::EN;

struct Row {
  const char* en;
  const char* hu;
  const char* de;
};

// Key = the English text exactly as written in TR("...").
// Order does not matter.
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
  { "Buzzer pins",             "Hangjelző lábai",                 "Summer-Pins" },
  { "Buzzer: OFF",             "Hangjelző: KI",                   "Summer: AUS" },
  { "Buzzer: ON",              "Hangjelző: BE",                   "Summer: EIN" },
  { "Brightness",              "Fényerő",                         "Helligkeit"},

  { "Calibrate",               "Kalibrálás",                      "Kalibrieren" },
  { "Can't access GPS",        "A GPS nem érhető el",             "Kein Zugriff auf GPS" },
  { "Clock",                   "Óra",                             "Uhr" },
  { "Companion Spectrum",      "Companion spektrum",              "Companion-Spektrum" },
  { "Compass",                 "Iránytű",                         "Kompass" },
  { "Connected",               "Csatlakozva",                     "Verbunden" },
  { "Current",                 "Áramerősség",                     "Stromstärke" },

  { "Default",                 "Alapértelmezett",                 "Standard" },
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

  {"I2C SDA",                  "I2C SDA",                         "I2C SDA"},
  {"I2C SCL",                  "I2C SCL",                         "I2C SCL"},
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
  { "MSG: %d",                 "ÜZENET: %d",                      "NACHR.: %d" },

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
  { "Time not set",            "Idő nincs beállítva",             "Zeit nicht gesetzt" },
  { "Time zone",               "Időzóna",                         "Zeitzone" },
  { "toggle: long press",      "váltás: hosszú nyomás",           "umschalten: lang drücken" },
  { "toggle: press Enter",     "váltás: Enter",                   "umschalten: Enter" },

  { "Unavailable pin",         "Nem használható láb",             "Nicht verfügbarer Pin" },
  { "Units",                   "Mértékegységek",                  "Einheiten" },
  { "Unknown",                 "Ismeretlen",                      "Unbekannt" },
  { "Unread Msg: %d",          "Olvasatlan üzenet: %d",           "Ungelesene Nachricht: %d" },
  { "Unread: %d",              "Olvasatlan: %d",                  "Ungelesen: %d" },

  { "Vibration",               "Rezgés",                          "Vibration" },
  { "Vibra",                   "Rezgés",                          "Vibration" },
  { "Vibra motor",             "Rezgőmotor",                      "Vibrationsmotor" },
  { "Voltage",                 "Feszültség",                      "Spannung" },

  { "Wake on message",         "Ébresztés üzenetre",              "Bei Nachricht aufwecken" }
};

static const char* const NAMES[(int)Lang::COUNT] = {
  "English",
  "Magyar",
  "Deutsch"
};

// Nyelvkód előállítása a konfigurációhoz.
static const char* languageCode(Lang lang) {
  switch (lang) {
    case Lang::HU: return "hu";
    case Lang::DE: return "de";
    case Lang::EN:
    default:       return "en";
  }
}

// A konfiguráció teljes tartalmának kiírása.
// A fájl formátuma:
// [ui]
// language=en
static bool writeConfig() {
  static const char* const HEADER = "[ui]\n";

  const char* code = languageCode(g_lang);

  InternalFS.remove(LANG_CONFIG_FILE);

  File f = InternalFS.open(LANG_CONFIG_FILE, FILE_O_WRITE);
  if (!f) return false;

  bool ok = true;

  if (f.write(
        reinterpret_cast<const uint8_t*>(HEADER),
        strlen(HEADER)) != strlen(HEADER)) {
    ok = false;
  }

  char setting[24];
  int len = snprintf(setting, sizeof(setting), "language=%s\n", code);

  if (len <= 0 || (size_t)len >= sizeof(setting)) {
    ok = false;
  } else if (f.write(
               reinterpret_cast<const uint8_t*>(setting),
               (size_t)len) != (size_t)len) {
    ok = false;
  }

  f.close();
  return ok;
}

// A konfiguráció sorainak feldolgozása.
static bool readLanguageConfig(File& f, Lang& result) {
  char line[64];
  bool found_language = false;

  while (f.available()) {
    size_t len = f.readBytesUntil('\n', line, sizeof(line) - 1);
    line[len] = '\0';

    // Sorvégi CR eltávolítása.
    while (len > 0 &&
           (line[len - 1] == '\r' || line[len - 1] == ' ' ||
            line[len - 1] == '\t')) {
      line[--len] = '\0';
    }

    // Sor eleji szóközök eltávolítása.
    char* p = line;
    while (*p == ' ' || *p == '\t') ++p;

    if (strncmp(p, "language=", 9) == 0) {
      Lang parsed;

      if (I18n::parse(p + 9, parsed)) {
        result = parsed;
        found_language = true;
      }
    }
  }

  return found_language;
}

// Advance *p to the next printf conversion and write its type signature
// (length modifiers plus conversion character) to out.
// "%%" is a literal percent.
static bool nextSpec(const char*& p, char out[8]) {
  for (;;) {
    while (*p && *p != '%') ++p;
    if (!*p) return false;

    ++p;

    if (*p == '%') {
      ++p;
      continue;
    }

    int n = 0;

    while (*p && !strchr("diouxXeEfFgGaAcspn", *p)) {
      if (strchr("hljztL*", *p) && n < 6) {
        out[n++] = *p;
      }
      ++p;
    }

    if (*p) {
      if (n < 7) out[n++] = *p;
      ++p;
    }

    out[n] = '\0';
    return true;
  }
}

static bool specsMatch(const char* a, const char* b) {
  const char* pa = a;
  const char* pb = b;

  for (;;) {
    char sa[8];
    char sb[8];

    bool ha = nextSpec(pa, sa);
    bool hb = nextSpec(pb, sb);

    if (ha != hb) return false;
    if (!ha) return true;
    if (strcmp(sa, sb) != 0) return false;
  }
}

// Fordítás keresése a beépített táblázatban.
const char* i18n::tr(const char* en) {
  if (!en) return "";

  if (g_lang == Lang::EN) return en;

  for (size_t i = 0; i < sizeof(ROWS) / sizeof(ROWS[0]); i++) {
    if (strcmp(en, ROWS[i].en) != 0) continue;

    const char* v =
      (g_lang == Lang::HU) ? ROWS[i].hu : ROWS[i].de;

    if (!v || !*v) return en;

    // A printf paramétereknek egyezniük kell.
    if (strchr(en, '%') && !specsMatch(en, v)) {
      return en;
    }

    return v;
  }

  // Hiányzó fordítás esetén az angol szöveg jelenik meg.
  return en;
}

void I18n::setLang(Lang l) {
  if ((uint8_t)l < (uint8_t)Lang::COUNT) {
    g_lang = l;
  }
}

Lang I18n::lang() {
  return g_lang;
}

const char* I18n::langName(Lang l) {
  return ((uint8_t)l < (uint8_t)Lang::COUNT)
    ? NAMES[(uint8_t)l]
    : "";
}

bool I18n::parse(const char* s, Lang& l) {
  if (!s) return false;

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

// Konfiguráció betöltése az InternalFS tárhelyről.
// Ha a fájl még nem létezik, első indításkor létrehozza
// az alapértelmezett angol beállítással.
void I18n::load() {
  // Alapértelmezett nyelv.
  g_lang = Lang::EN;

  // InternalFS inicializálása.
  if (!InternalFS.begin()) {
    return;
  }

  // Első indítás: még nincs konfigurációs fájl.
  if (!InternalFS.exists(LANG_CONFIG_FILE)) {
    writeConfig();
    return;
  }

  File f = InternalFS.open(LANG_CONFIG_FILE, FILE_O_READ);

  if (!f) {
    // Ha a fájl nem nyitható meg, megpróbáljuk létrehozni
    // az alapértelmezett konfigurációt.
    writeConfig();
    return;
  }

  Lang loaded = Lang::EN;
  bool valid = readLanguageConfig(f, loaded);

  f.close();

  if (valid) {
    g_lang = loaded;
  } else {
    // Hiányzó vagy hibás nyelvi beállítás esetén
    // visszaállunk angolra és javítjuk a konfigurációt.
    g_lang = Lang::EN;
    writeConfig();
  }
}

// Aktuális nyelv mentése az InternalFS konfigurációs fájljába.
void I18n::save() {
  if (!InternalFS.begin()) {
    return;
  }

  writeConfig();
}
