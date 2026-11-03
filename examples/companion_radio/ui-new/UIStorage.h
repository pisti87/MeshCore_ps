
#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

// --------------------------------------------------
// Közös UI tárhelykezelő
//
// Támogatott platformok:
//
// Heltec T096  -> nRF52 InternalFileSystem
// Heltec T114  -> nRF52 InternalFileSystem
// Heltec V3    -> ESP32 SPIFFS
//
// A konfigurációs fájlok kezelését egységesíti.
// --------------------------------------------------

class UIStorage {
public:

    // Fájlrendszer inicializálása.
    // Többször is biztonságosan meghívható.
    static bool begin();

    // A fájlrendszer elérhetősége.
    static bool ready();

    // Fájl létezésének ellenőrzése.
    static bool exists(
        const char* path
    );

    // Fájl beolvasása.
    //
    // buffer:
    //   célmemória
    //
    // capacity:
    //   a célmemória mérete
    //
    // bytesRead:
    //   ténylegesen beolvasott bájtok száma
    //
    // A függvény nem olvas túl a megadott memórián.
    static bool read(
        const char* path,
        uint8_t* buffer,
        size_t capacity,
        size_t& bytesRead
    );

    // Fájl biztonságos mentése.
    //
    // Ideiglenes fájlt használ:
    //   .tmp
    //
    // Ellenőrzi a kiírt adatokat.
    // A korábbi fájlt csak sikeres írás után cseréli le.
    static bool write(
        const char* path,
        const uint8_t* data,
        size_t length
    );

    // Fájl törlése.
    // A hozzá tartozó ideiglenes és biztonsági
    // fájlokat is eltávolítja.
    static bool remove(
        const char* path
    );
};
