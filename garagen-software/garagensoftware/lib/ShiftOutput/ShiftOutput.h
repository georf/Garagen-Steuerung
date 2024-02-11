#include "Arduino.h"

class ShiftOutput
{
private:
    // Shift register pins
    uint8_t _data_pin;
    uint8_t _load_pin;
    uint8_t _clock_pin;


public:
    // hold current state
    byte dataState[2];

    void begin(uint8_t data_pin, uint8_t load_pin, uint8_t clock_pin);
    void write();
    void digitalWrite(uint8_t pin, uint8_t value);
    uint8_t digitalRead(uint8_t pin);
};
