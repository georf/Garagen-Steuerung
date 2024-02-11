#include <Arduino.h>
#include <MCP3XXX.h>
#include <AdcSwitch.h>
#include <ShiftOutput.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <credentials.h>

#define RELAIS_1 14
#define RELAIS_2 13
#define RELAIS_3 12
#define RELAIS_4 11
#define RELAIS_5 10
#define RELAIS_6 9
#define RELAIS_7 8
#define RELAIS_8 15

#define SWITCH_5V_1 0
#define SWITCH_5V_GARAGE_GATE 1
#define SWITCH_5V_YARD_GATE 2
#define SWITCH_5V_TIMEOUT 3
#define SWITCH_MOTOR 4


#define LIGHTS_OFF 0
#define LIGHTS_MIDDLE 1
#define LIGHTS_MIDDLE_RIGHT 2
#define LIGHTS_MIDDLE_RIGHT_MOTOR 3

#define YARD_GATE_CLOSED 0
#define YARD_GATE_OPENED 1
#define YARD_GATE_RUNNING 2

#define GARAGE_GATE_CLOSED 0
#define GARAGE_GATE_OPENED 1
#define GARAGE_GATE_OPENING 2
#define GARAGE_GATE_CLOSING 3
#define GARAGE_GATE_UNKNOWN 4

#define GARAGE_PRESS_TIME 100

void switchLightsPushed();
void switchYardGatePushed();

void handleLights();
void doorStateChanged();
void gateStateChanged();
void pressSwitch();

#define DOOR_TIMEOUT 60 * 1 * 1000
#define GATE_TIMEOUT 60 * 5 * 1000

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

#define MQTT_STATUS_UPDATE_TIME 60 * 1000 // Every minute
#define MQTT_GATE_STATE "adebar/garage/gate/state"
#define MQTT_GATE_SWITCH "adebar/garage/gate/switch"
#define MQTT_GATE_OPEN "adebar/garage/gate/open"
#define MQTT_GATE_CLOSE "adebar/garage/gate/close"
#define MQTT_LIGHTS_SET "adebar/garage/lights/set"
#define MQTT_LIGHTS_STATE "adebar/garage/lights/state"
#define MQTT_DOOR0_STATE "adebar/garage/door0/state"
#define MQTT_DOOR1_STATE "adebar/garage/door1/state"
#define MQTT_SWITCH_LIGHTS_STATE "adebar/garage/switch_lights/state"
#define MQTT_SWITCH_YARD_GATE_STATE "adebar/garage/switch_yard_gate/state"

#define MQTT_HOFTOR_CHANNEL "adebar/smartgate/state"
#define MQTT_HOFTOR_SWITCH "adebar/smartgate/toggle"

void mqtt_reconnect();
void mqtt_callback(char *topic, byte *payload, unsigned int length);
void mqtt_send_status();