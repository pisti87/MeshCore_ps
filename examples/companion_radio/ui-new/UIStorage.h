#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

// ==================================================
// HellCore – közös UI tárhelykezelő
//
// Támogatott eszközök:
//
// Heltec T096:
//   nRF52 InternalFileSystem
//
// Heltec T114:
//   nRF52 InternalFileSystem
//
// Heltec V3:
//   ESP32 SPIFFS
//
// A konfigurációs fájlok tárolása az adott eszköz
// belső flash memóriájának fájlrendszerében történik.
//
// A platformfüggő fájlrendszer kiválasztása a
// UIStorage.cpp fájlban történik.
// ==================================================

class UIStorage {
public:

    // Fájlrendszer inicializálása.
    // Többször is meghívható.
    static bool begin();

    // Ellenőrzi, hogy a tárhely elérhető-e.
    static bool ready();

    // Fájl létezésének ellenőrzése.
    static bool exists(const char* path);

    // Fájl beolvasása.
    //
    // path:
    //   fájl elérési útja
    //
    // buffer:
    //   célmemória
    //
    // capacity:
    //   célmemória mérete
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

    // Fájl mentése.
    //
    // A mentés először ideiglenes fájlba történik.
    // A kiírt adatot visszaolvassa és ellenőrzi.
    //
    // Csak ellenőrzött adat kerül a végleges fájlba.
    static bool write(
        const char* path,
        const uint8_t* data,
        size_t length
    );

    // Fájl és ideiglenes fájl törlése.
    static bool remove(const char* path);

    // A kiválasztott háttértár neve.
    static const char* backendName();

};
