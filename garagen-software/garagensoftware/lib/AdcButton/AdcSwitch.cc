#include "Arduino.h"
#include "AdcSwitch.h"

AdcSwitch::AdcSwitch(MCP3008 *adc, const uint8_t channel, const uint32_t threshold, const bool normal)
{
    _adc = adc;
    _channel = channel;
    _threshold = threshold;
    _state = normal;
    _lastValue = normal;
    _lastDebounceTime = 0;
    _firstCallback = false;
}

void AdcSwitch::read()
{
    bool value = _adc->analogRead(_channel) > _threshold;
    if (value != _lastValue)
        _lastDebounceTime = millis();

    if (((millis() - _lastDebounceTime) > 10 && value != _state) || !_firstCallback)
    {
        _firstCallback = true;
        _state = value;
        if (value && onHighCallback)
            onHighCallback();
        else if (!value && onLowCallback)
            onLowCallback();
    }
    _lastValue = value;
}

bool AdcSwitch::high()
{
    return _state;
}

void AdcSwitch::debug()
{
    Serial.print("adc switch ");
    Serial.print(_channel);
    Serial.print(": ");
    Serial.print(_adc->analogRead(_channel));
    Serial.print(" (");
    Serial.print(_threshold);
    Serial.println(")");
}