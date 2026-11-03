#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

class UIStorage {
public:
    static bool begin();
    static bool ready();

    static bool exists(const char* path);
    static bool read(
        const char* path,
        uint8_t* buffer,
        size_t capacity,
        size_t& bytesRead
    );

    static bool write(
        const char* path,
        const uint8_t* data,
        size_t length
    );

    static bool remove(const char* path);
};
