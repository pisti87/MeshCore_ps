#pragma once
// UI languages: English, Hungarian, German. Own implementation of the idea used by wadamesh:
// wrap the ENGLISH text in TR("...") and get the active language's text back. A missing
// translation returns the English text itself, so the UI is never blank.
//
// No PSRAM on the nRF52840, so the table is a small const array in flash (linear search).
// APPEND new languages before COUNT only: the index is saved to flash.
//
// IMPORTANT: if the result is used as a printf FORMAT (snprintf(buf, n, TR("Temp %d"), v)), a
// translation must contain the same conversions as the English key. TR() checks this and falls
// back to English when they differ (wadamesh hit a reboot bug from a reordered %s/%lu).
//
// NOTE: Hungarian ő/ű and German ä/ö/ü/ß need glyphs in the display font; see README.

#include <stdint.h>

enum class Lang : uint8_t { EN = 0, HU = 1, DE = 2, COUNT };

namespace i18n {
  const char* tr(const char* en);
}

#ifndef TR
#define TR(s) i18n::tr(s)
#endif

class I18n {
public:
  static void setLang(Lang l);
  static Lang lang();
  static const char* langName(Lang l);       // "English", "Magyar", "Deutsch"
  static bool parse(const char* s, Lang& l); // accepts en/hu/de
  static void load();                        // read saved language from flash
  static void save();                        // persist current language
};
