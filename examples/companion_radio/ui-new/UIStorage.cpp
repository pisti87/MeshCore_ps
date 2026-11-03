
#include "UIStorage.h"

#include <string.h>
#include <stdio.h>

// --------------------------------------------------
// Platformfüggő fájlrendszerek
// --------------------------------------------------

#if defined(NRF52_PLATFORM)

    #include <InternalFileSystem.h>

#elif defined(ESP32)

    #include <SPIFFS.h>

#else

    #error "UIStorage: Nem támogatott platform!"

#endif

// --------------------------------------------------
// Belső állapot
// --------------------------------------------------

static bool storage_ready = false;

// --------------------------------------------------
// Fájlrendszer inicializálása
// --------------------------------------------------

bool UIStorage::begin() {

    if (storage_ready) {
        return true;
    }

#if defined(NRF52_PLATFORM)

    // Heltec T096 / T114
    storage_ready = InternalFS.begin();

#elif defined(ESP32)

    // Heltec V3
    // Automatikus formázás TILOS!
    storage_ready = SPIFFS.begin(false);

#endif

    return storage_ready;
}

// --------------------------------------------------
// Állapot
// --------------------------------------------------

bool UIStorage::ready() {

    return storage_ready;
}

// --------------------------------------------------
// Belső segédfüggvények
// --------------------------------------------------

namespace {

    // Fájllétezés.
    bool fileExists(const char* path) {

#if defined(NRF52_PLATFORM)

        return InternalFS.exists(path);

#elif defined(ESP32)

        return SPIFFS.exists(path);

#endif
    }

    // Fájl megnyitása olvasásra.
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

    // Fájl megnyitása írásra.
    File openWrite(const char* path) {

#if defined(NRF52_PLATFORM)

        // A régi fájlt előbb töröljük,
        // mert az InternalFS írási módja
        // nem minden esetben truncálja.
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

    // Fájl törlése.
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

    // Fájl átnevezése.
    bool renameFile(
        const char* from,
        const char* to
    ) {

#if defined(NRF52_PLATFORM)

        return InternalFS.rename(
            from,
            to
        );

#elif defined(ESP32)

        return SPIFFS.rename(
            from,
            to
        );

#endif
    }

    // Kiegészítő fájlnév létrehozása.
    bool makeSidePath(
        const char* path,
        const char* suffix,
        char* output,
        size_t output_size
    ) {

        if (
            path == NULL ||
            suffix == NULL ||
            output == NULL ||
            output_size == 0
        ) {
            return false;
        }

        int n = snprintf(
            output,
            output_size,
            "%s%s",
            path,
            suffix
        );

        return n > 0 &&
               (size_t)n < output_size;
    }

    // Fájl méretének és tartalmának ellenőrzése.
    bool verifyFile(
        const char* path,
        const uint8_t* expected,
        size_t expected_length
    ) {

        File f = openRead(path);

        if (!f) {
            return false;
        }

        if (f.size() != expected_length) {
            f.close();
            return false;
        }

        uint8_t buffer[64];

        size_t offset = 0;

        bool valid = true;

        while (offset < expected_length) {

            size_t remaining =
                expected_length - offset;

            size_t amount =
                remaining < sizeof(buffer)
                    ? remaining
                    : sizeof(buffer);

            size_t received =
                f.read(buffer, amount);

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

        f.close();

        return valid;
    }

    // Fájl beolvasása.
    bool readFile(
        const char* path,
        uint8_t* buffer,
        size_t capacity,
        size_t& bytesRead
    ) {

        bytesRead = 0;

        File f = openRead(path);

        if (!f) {
            return false;
        }

        size_t file_size = f.size();

        if (file_size > capacity) {
            f.close();
            return false;
        }

        if (file_size == 0) {
            f.close();
            return true;
        }

        size_t received = f.read(
            buffer,
            file_size
        );

        f.close();

        if (received != file_size) {
            return false;
        }

        bytesRead = received;

        return true;
    }

} // namespace

// --------------------------------------------------
// Fájl létezésének ellenőrzése
// --------------------------------------------------

bool UIStorage::exists(
    const char* path
) {

    if (
        path == NULL ||
        !begin()
    ) {
        return false;
    }

    return fileExists(path);
}

// --------------------------------------------------
// Fájl beolvasása
// --------------------------------------------------

bool UIStorage::read(
    const char* path,
    uint8_t* buffer,
    size_t capacity,
    size_t& bytesRead
) {

    bytesRead = 0;

    if (
        path == NULL ||
        buffer == NULL ||
        !begin()
    ) {
        return false;
    }

    // Elsődleges fájl.
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
    // megpróbáljuk a félbemaradt mentés
    // ideiglenes fájlját.

    char temp_path[96];

    if (
        !makeSidePath(
            path,
            ".tmp",
            temp_path,
            sizeof(temp_path)
        )
    ) {
        return false;
    }

    if (
        readFile(
            temp_path,
            buffer,
            capacity,
            bytesRead
        )
    ) {
        return true;
    }

    // Utolsó lehetőség:
    // korábbi biztonsági másolat.

    char backup_path[96];

    if (
        !makeSidePath(
            path,
            ".bak",
            backup_path,
            sizeof(backup_path)
        )
    ) {
        return false;
    }

    return readFile(
        backup_path,
        buffer,
        capacity,
        bytesRead
    );
}

// --------------------------------------------------
// Fájl biztonságos mentése
// --------------------------------------------------

bool UIStorage::write(
    const char* path,
    const uint8_t* data,
    size_t length
) {

    if (
        path == NULL ||
        (data == NULL && length > 0) ||
        !begin()
    ) {
        return false;
    }

    char temp_path[96];
    char backup_path[96];

    if (
        !makeSidePath(
            path,
            ".tmp",
            temp_path,
            sizeof(temp_path)
        ) ||
        !makeSidePath(
            path,
            ".bak",
            backup_path,
            sizeof(backup_path)
        )
    ) {
        return false;
    }

    // Korábbi félbemaradt ideiglenes
    // fájl eltávolítása.

    removeFile(temp_path);

    // Új ideiglenes fájl létrehozása.

    File f = openWrite(temp_path);

    if (!f) {
        return false;
    }

    size_t written = 0;

    if (length > 0) {

        written = f.write(
            data,
            length
        );
    }

    f.close();

    if (written != length) {
        removeFile(temp_path);
        return false;
    }

    // A tényleges tartalom ellenőrzése.

    if (
        !verifyFile(
            temp_path,
            data,
            length
        )
    ) {
        removeFile(temp_path);
        return false;
    }

    // Korábbi biztonsági másolat törlése.

    removeFile(backup_path);

    bool had_original = fileExists(path);

    // A jelenlegi fájlt biztonsági
    // másolatként megtartjuk.

    if (had_original) {

        if (
            !renameFile(
                path,
                backup_path
            )
        ) {
            removeFile(temp_path);
            return false;
        }
    }

    // Az új fájl átnevezése.

    if (
        !renameFile(
            temp_path,
            path
        )
    ) {

        // Sikertelen csere esetén
        // megpróbáljuk visszaállítani
        // a korábbi konfigurációt.

        if (had_original) {

            renameFile(
                backup_path,
                path
            );
        }

        return false;
    }

    // Sikeres csere után a biztonsági
    // másolat már nem szükséges.

    if (had_original) {
        removeFile(backup_path);
    }

    return true;
}

// --------------------------------------------------
// Fájl törlése
// --------------------------------------------------

bool UIStorage::remove(
    const char* path
) {

    if (
        path == NULL ||
        !begin()
    ) {
        return false;
    }

    char temp_path[96];
    char backup_path[96];

    if (
        !makeSidePath(
            path,
            ".tmp",
            temp_path,
            sizeof(temp_path)
        ) ||
        !makeSidePath(
            path,
            ".bak",
            backup_path,
            sizeof(backup_path)
        )
    ) {
        return false;
    }

    bool main_ok = removeFile(path);
    bool temp_ok = removeFile(temp_path);
    bool backup_ok = removeFile(backup_path);

    return main_ok &&
           temp_ok &&
           backup_ok;
}
