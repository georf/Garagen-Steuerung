#include <Arduino.h>
#include <MCP3XXX.h>
#include <AdcSwitch.h>
#include <ShiftOutput.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <credentials.h>
#include <ArduinoJson.h>

uint8_t const relayBits[] = {
    15, //  RELAY_1
    14, //  RELAY_2
    13, //  RELAY_3
    12, //  RELAY_4
    11, //  RELAY_5
    10, //  RELAY_6
    9,  //  RELAY_7
    8,  //  RELAY_8
};

#define OUTPUT_5V_1 1
#define OUTPUT_5V_COVER_BLINK_LED 2
#define OUTPUT_5V_YARD_GATE_BLINK_LED 3
#define OUTPUT_5V_TIMEOUT_LED 4
#define OUTPUT_COVER_SWITCH 5

#define YARD_GATE_CLOSED 0
#define YARD_GATE_OPENED 1
#define YARD_GATE_RUNNING 2
#define YARD_GATE_STOPPED 3

#define COVER_STATE_CLOSED 0
#define COVER_STATE_OPENED 1
#define COVER_STATE_OPENING 2
#define COVER_STATE_CLOSING 3
#define COVER_STATE_UNKNOWN 4

#define COVER_SWITCH_PRESS_TIME 100

void switchLightsPushed();
void switchYardGatePushed();

void handleLights();
void handleCover();
void setLightTimeout(uint16_t seconds);
void changeRelay(uint8_t relay, boolean value) ;
void doorStateChanged();
void coverStateChanged();
void pressCoverSwitch();

#define DOOR0_OPEN door0.high()
#define DOOR1_OPEN door1.high()
#define DOOR0_CLOSED !door0.high()
#define DOOR1_CLOSED !door1.high()

#define GATE_MOTOR_RUNNING (motor0.high() != motor1.high())
#define GATE_MOTOR_CLOSING_RUNNING !motor0.high()
#define GATE_MOTOR_OPENING_RUNNING !motor1.high()
#define GATE_FINAL_OPENED !finalPosition0.high()
#define GATE_FINAL_CLOSED !finalPosition1.high()

#define SWITCH_LIGHTS_PRESSED !switchLights.high()
#define SWITCH_YARD_GATE_PRESSED !switchYardGate.high()

#define readRelay(relay) !shiftOutput.digitalRead(relayBits[relay])

void mqtt_reconnect();
void mqtt_callback(char *topic, byte *payload, unsigned int length);

void mqtt_send_status(boolean full);
void mqtt_send_adebar_garage_cover(boolean full);
void mqtt_send_adebar_garage_ip_address(boolean full);
void mqtt_send_adebar_garage_door(uint8_t door, boolean full);
void mqtt_send_adebar_garage_relay(uint8_t relay, boolean full);
