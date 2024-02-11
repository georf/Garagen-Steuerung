#include "ShiftOutput.h"

void ShiftOutput::begin(uint8_t data_pin, uint8_t load_pin, uint8_t clock_pin)
{
    dataState[0] = 0;
    dataState[1] = 0b11111111;

    _data_pin = data_pin;
    _load_pin = load_pin;
    _clock_pin = clock_pin;

    pinMode(_data_pin, OUTPUT);
    pinMode(_load_pin, OUTPUT);
    pinMode(_clock_pin, OUTPUT);

    // write all states to 0
    write();
}

void ShiftOutput::write()
{
    ::digitalWrite(_load_pin, LOW);
    shiftOut(_data_pin, _clock_pin, MSBFIRST, dataState[0]);
    shiftOut(_data_pin, _clock_pin, MSBFIRST, dataState[1]);
    ::digitalWrite(_load_pin, HIGH);
}

void ShiftOutput::digitalWrite(uint8_t pin, uint8_t newValue)
{
    uint8_t row = pin / 8;
    pin = pin % 8 + 1;
    uint8_t oldValue = bitRead(dataState[row], pin);
    if (oldValue != newValue)
    {
        Serial.println(row);
        Serial.println(pin);
        Serial.println(newValue);
        bitWrite(dataState[row], pin, newValue);
        write();
    }
}

uint8_t ShiftOutput::digitalRead(uint8_t pin)
{
    uint8_t row = pin / 8;
    pin = pin % 8 + 1;
    return bitRead(dataState[row], pin);
}
