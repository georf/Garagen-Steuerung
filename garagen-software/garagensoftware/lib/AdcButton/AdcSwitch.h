#include "Arduino.h"
#include <MCP3XXX.h>

class AdcSwitch
{
private:
    unsigned long _lastDebounceTime;
    void (*_pCallback)();
    bool _lastValue;
    uint8_t _channel;
    bool _state;
    uint32_t _threshold;
    bool _firstCallback;

public:
    MCP3008 *_adc;
    void (*onHighCallback)();
    void (*onLowCallback)();
    void read();
    bool high();
    void debug();
    AdcSwitch(MCP3008 *adc, const uint8_t channel, const uint32_t threshold, const bool normal);
};
