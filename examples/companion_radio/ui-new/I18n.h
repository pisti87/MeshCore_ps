
#pragma once

// UI languages: English, Hungarian, German.
//
// Platform-independent language implementation.
// Wrap the ENGLISH text in TR("...") to get the active
// language's translation.
//
// A missing translation returns the English text itself,
// so the UI is never blank.
//
// The translation table is stored as const data in flash.
// No dynamic allocation is required.
//
// IMPORTANT:
// If the result is used as a printf FORMAT:
//
//   snprintf(buf, n, TR("Temp %d"), value);
//
// The translation must contain the same format conversions
// as the English key. TR() checks this and falls back to
// English when they differ.
//
// The selected language is stored in /ui.conf using a
// textual language code:
//
//   language=en
//   language=hu
//   language=de
//
// UIStorage handles all filesystem operations.
// I18n does not depend on the target hardware or filesystem.
//
// Hungarian ő/ű and German ä/ö/ü/ß need glyphs in the
// display font; see README.

#include <stdint.h>

// --------------------------------------------------
// Available languages
// --------------------------------------------------

enum class Lang : uint8_t {
    EN = 0,
    HU = 1,
    DE = 2,
    COUNT
};

// --------------------------------------------------
// Translation interface
// --------------------------------------------------

namespace i18n {
    const char* tr(const char* en);
}

#ifndef TR
#define TR(s) i18n::tr(s)
#endif

// --------------------------------------------------
// Language management
// --------------------------------------------------

class I18n {
public:

    // Set active language.
    static void setLang(Lang l);

    // Get active language.
    static Lang lang();

    // Get language display name:
    // "English", "Magyar", "Deutsch"
    static const char* langName(Lang l);

    // Parse language code:
    // accepts "en", "hu", "de".
    static bool parse(const char* s, Lang& l);

    // Load saved language through UIStorage.
    static void load();

    // Save current language through UIStorage.
    static void save();
};
