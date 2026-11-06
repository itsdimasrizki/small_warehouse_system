#pragma once
#include <iostream>
#include <string>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>

typedef uint8_t byte;

inline unsigned long mock_millis = 0;
inline unsigned long millis() {
    return mock_millis;
}

inline void delay(unsigned long ms) {
    mock_millis += ms;
}

class String : public std::string {
public:
    String() : std::string() {}
    String(const char* s) : std::string(s ? s : "") {}
    String(const std::string& s) : std::string(s) {}

    void trim() {
        size_t first = find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            clear();
            return;
        }
        size_t last = find_last_not_of(" \t\r\n");
        *this = substr(first, (last - first + 1));
    }

    void toUpperCase() {
        for (auto &c : *this) {
            c = ::toupper(c);
        }
    }
};

inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t, uint8_t) {}
inline void analogWrite(uint8_t, int) {}
inline int analogRead(uint8_t) { return 0; }
inline void analogReadResolution(uint8_t) {}

#define HIGH 1
#define LOW  0
#define INPUT 0
#define OUTPUT 1
