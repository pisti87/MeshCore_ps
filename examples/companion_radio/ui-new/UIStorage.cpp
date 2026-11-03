#include "UIStorage.h"

#include <string.h>
#include <stdio.h>

// ==================================================
// PLATFORMFÜGGŐ TÁRHELY KIVÁLASZTÁSA
// ==================================================

#if defined(NRF52_PLATFORM)

    // Heltec T096
    // Heltec T114
    #include <InternalFileSystem.h>


    using namespace Adafruit_LittleFS_Namespace;
    
#elif defined(ESP32)

    // Heltec V3
    #include <SPIFFS.h>

#else

    #error "UIStorage: Nem támogatott platform!"

#endif


// ==================================================
// BELSŐ ÁLLAPOT
// ==================================================

namespace {

    bool storage_ready = false;

    // Ideiglenes fájl maximális elérésiút-hossza.
    constexpr size_t PATH_BUFFER_SIZE = 128;


    // --------------------------------------------------
    // Fájlrendszer fájllétezés ellenőrzése
    // --------------------------------------------------

    bool fileExists(const char* path) {

#if defined(NRF52_PLATFORM)

        return InternalFS.exists(path);

#elif defined(ESP32)

        return SPIFFS.exists(path);

#endif

    }


    // --------------------------------------------------
    // Fájl megnyitása olvasásra
    // --------------------------------------------------

    File openRead(const char* path) {

#if defined(NRF52_PLATFORM)

        return InternalFS.open(
            path,
            FILE_O_READ
        );

#elif defined(ESP32)

        return SPIFFS.open(
            path,
            "r"
        );

#endif

    }


    // --------------------------------------------------
    // Fájl megnyitása írásra
    // --------------------------------------------------

    File openWrite(const char* path) {

#if defined(NRF52_PLATFORM)

        // Az InternalFS esetében a meglévő fájlt
        // előbb törölni kell, hogy ne maradjon
        // benne korábbi tartalom.

        InternalFS.remove(path);

        return InternalFS.open(
            path,
            FILE_O_WRITE
        );

#elif defined(ESP32)

        return SPIFFS.open(
            path,
            "w"
        );

#endif

    }


    // --------------------------------------------------
    // Fájl törlése
    // --------------------------------------------------

    bool removeFile(const char* path) {

        if (!fileExists(path)) {
            return true;
        }

#if defined(NRF52_PLATFORM)

        return InternalFS.remove(path);

#elif defined(ESP32)

        return SPIFFS.remove(path);

#endif

    }


    // --------------------------------------------------
    // Kiegészítő fájlnév előállítása
    // --------------------------------------------------

    bool makeTempPath(
        const char* path,
        char* output,
        size_t outputSize
    ) {

        if (
            path == nullptr ||
            output == nullptr ||
            outputSize == 0
        ) {
            return false;
        }

        int length = snprintf(
            output,
            outputSize,
            "%s.tmp",
            path
        );

        return length > 0 &&
               static_cast<size_t>(length) < outputSize;

    }


    // --------------------------------------------------
    // Fájl tartalmának ellenőrzése
    // --------------------------------------------------

    bool verifyFile(
        const char* path,
        const uint8_t* expected,
        size_t expectedLength
    ) {

        File file = openRead(path);

        if (!file) {
            return false;
        }

        if (file.size() != expectedLength) {
            file.close();
            return false;
        }

        uint8_t buffer[64];

        size_t offset = 0;

        bool valid = true;

        while (offset < expectedLength) {

            size_t remaining =
                expectedLength - offset;

            size_t amount =
                remaining < sizeof(buffer)
                    ? remaining
                    : sizeof(buffer);

            size_t received = file.read(
                buffer,
                amount
            );

            if (received != amount) {
                valid = false;
                break;
            }

            if (
                memcmp(
                    buffer,
                    expected + offset,
                    amount
                ) != 0
            ) {
                valid = false;
                break;
            }

            offset += amount;

        }

        file.close();

        return valid;

    }


    // --------------------------------------------------
    // Belső fájlbeolvasás
    // --------------------------------------------------

    bool readFile(
        const char* path,
        uint8_t* buffer,
        size_t capacity,
        size_t& bytesRead
    ) {

        bytesRead = 0;

        File file = openRead(path);

        if (!file) {
            return false;
        }

        size_t fileSize = file.size();

        if (fileSize > capacity) {
            file.close();
            return false;
        }

        if (fileSize == 0) {
            file.close();
            return true;
        }

        size_t received = file.read(
            buffer,
            fileSize
        );

        file.close();

        if (received != fileSize) {
            return false;
        }

        bytesRead = received;

        return true;

    }

}


// ==================================================
// TÁRHELY INICIALIZÁLÁSA
// ==================================================

bool UIStorage::begin() {

    if (storage_ready) {
        return true;
    }

#if defined(NRF52_PLATFORM)

    // T096 / T114
    //
    // A belső flash fájlrendszer használata.
    // Nem formázunk automatikusan.

    storage_ready = InternalFS.begin();

#elif defined(ESP32)

    // Heltec V3
    //
    // SPIFFS fájlrendszer.
    // Hiba esetén automatikus formázás TILOS.

    storage_ready = SPIFFS.begin(false);

#endif

    return storage_ready;

}


// ==================================================
// TÁRHELY ÁLLAPOTA
// ==================================================

bool UIStorage::ready() {

    return storage_ready;

}


// ==================================================
// TÁRHELY TÍPUSÁNAK LEKÉRDEZÉSE
// ==================================================

const char* UIStorage::backendName() {

#if defined(NRF52_PLATFORM)

    return "InternalFS";

#elif defined(ESP32)

    return "SPIFFS";

#else

    return "Unknown";

#endif

}


// ==================================================
// FÁJL LÉTEZÉSÉNEK ELLENŐRZÉSE
// ==================================================

bool UIStorage::exists(
    const char* path
) {

    if (
        path == nullptr ||
        !begin()
    ) {
        return false;
    }

    return fileExists(path);

}


// ==================================================
// FÁJL BEOLVASÁSA
// ==================================================

bool UIStorage::read(
    const char* path,
    uint8_t* buffer,
    size_t capacity,
    size_t& bytesRead
) {

    bytesRead = 0;

    if (
        path == nullptr ||
        buffer == nullptr ||
        !begin()
    ) {
        return false;
    }

    // Elsődleges fájl beolvasása.

    if (
        readFile(
            path,
            buffer,
            capacity,
            bytesRead
        )
    ) {
        return true;
    }

    // Ha a fő fájl nem olvasható,
    // megpróbáljuk az ideiglenes fájlt.

    char tempPath[PATH_BUFFER_SIZE];

    if (
        !makeTempPath(
            path,
            tempPath,
            sizeof(tempPath)
        )
    ) {
        return false;
    }

    if (
        readFile(
            tempPath,
            buffer,
            capacity,
            bytesRead
        )
    ) {
        return true;
    }

    return false;

}


// ==================================================
// FÁJL MENTÉSE
// ==================================================

bool UIStorage::write(
    const char* path,
    const uint8_t* data,
    size_t length
) {

    if (
        path == nullptr ||
        (data == nullptr && length > 0) ||
        !begin()
    ) {
        return false;
    }

    char tempPath[PATH_BUFFER_SIZE];

    if (
        !makeTempPath(
            path,
            tempPath,
            sizeof(tempPath)
        )
    ) {
        return false;
    }

    // Korábbi ideiglenes fájl eltávolítása.

    removeFile(tempPath);

    // Új ideiglenes fájl létrehozása.

    File file = openWrite(tempPath);

    if (!file) {
        return false;
    }

    size_t written = 0;

    if (length > 0) {

        written = file.write(
            data,
            length
        );

    }

    file.close();

    if (written != length) {
        removeFile(tempPath);
        return false;
    }

    // Az ideiglenes fájl tartalmának ellenőrzése.

    if (
        !verifyFile(
            tempPath,
            data,
            length
        )
    ) {
        removeFile(tempPath);
        return false;
    }

    // A régi végleges fájl eltávolítása.

    if (!removeFile(path)) {
        return false;
    }

    // Az ellenőrzött ideiglenes fájl tartalmának
    // átmásolása a végleges fájlba.

    File source = openRead(tempPath);

    if (!source) {
        return false;
    }

    File destination = openWrite(path);

    if (!destination) {
        source.close();
        return false;
    }

    uint8_t buffer[64];

    bool success = true;

    size_t copied = 0;

    while (copied < length) {

        size_t remaining = length - copied;

        size_t amount =
            remaining < sizeof(buffer)
                ? remaining
                : sizeof(buffer);

        size_t received = source.read(
            buffer,
            amount
        );

        if (received != amount) {
            success = false;
            break;
        }

        size_t output = destination.write(
            buffer,
            received
        );

        if (output != received) {
            success = false;
            break;
        }

        copied += received;

    }

    source.close();
    destination.close();

    if (!success || copied != length) {
        removeFile(path);
        return false;
    }

    // A végleges fájl visszaellenőrzése.

    if (
        !verifyFile(
            path,
            data,
            length
        )
    ) {
        removeFile(path);
        return false;
    }

    // Sikeres mentés után az ideiglenes fájl törlése.

    removeFile(tempPath);

    return true;

}


// ==================================================
// FÁJL TÖRLÉSE
// ==================================================

bool UIStorage::remove(
    const char* path
) {

    if (
        path == nullptr ||
        !begin()
    ) {
        return false;
    }

    char tempPath[PATH_BUFFER_SIZE];

    if (
        !makeTempPath(
            path,
            tempPath,
            sizeof(tempPath)
        )
    ) {
        return false;
    }

    bool mainOk = removeFile(path);

    bool tempOk = removeFile(tempPath);

    return mainOk && tempOk;

}
