#include <main.h>

// define pin connections
#define CLOCK_PIN D2
#define MOSI_PIN D5
#define MISO_PIN D1
#define CS0_PIN D4
#define CS1_PIN D3

#define SR_DATA_PIN D8  // Outputs the byte to transfer
#define SR_LOAD_PIN D7  // Controls the internal transference of data in SN74HC595 internal registers
#define SR_CLOCK_PIN D6 // Generates the clock signal to control the transference of data

MCP3008 adc0;
MCP3008 adc1;
AdcSwitch door0(&adc1, 0, 600, LOW);
AdcSwitch door1(&adc1, 1, 600, LOW);
AdcSwitch switchLights(&adc1, 2, 600, HIGH);
AdcSwitch switchYardGate(&adc1, 3, 600, HIGH);
AdcSwitch motor0(&adc1, 4, 100, HIGH);
AdcSwitch motor1(&adc1, 5, 100, HIGH);
AdcSwitch finalPosition0(&adc1, 6, 300, HIGH);
AdcSwitch finalPosition1(&adc1, 7, 300, HIGH);
ShiftOutput shiftOutput;

uint8_t yardGateModus = YARD_GATE_CLOSED;
uint8_t coverModus = COVER_STATE_CLOSED;

uint32_t lightTimeout = 0;
bool lightTimeoutSet = false;
uint32_t coverSwitchPressedTime = 0;
bool coverSwitchPressed = false;
uint8_t coverDesiredState = COVER_STATE_CLOSED;

WiFiClient espClient;
PubSubClient client(espClient);
uint32_t lastMqttStatusUpdate = 0;
uint8_t loopIndex = 0;

void setup()
{
  // init adcs
  adc0.begin(CS0_PIN, MOSI_PIN, MISO_PIN, CLOCK_PIN);
  adc1.begin(CS1_PIN, MOSI_PIN, MISO_PIN, CLOCK_PIN);

  Serial.begin(115200);

  door0.onHighCallback = &doorStateChanged;
  door0.onLowCallback = &doorStateChanged;
  door1.onHighCallback = &doorStateChanged;
  door1.onLowCallback = &doorStateChanged;

  motor0.onHighCallback = &coverStateChanged;
  motor0.onLowCallback = &coverStateChanged;
  motor1.onHighCallback = &coverStateChanged;
  motor1.onLowCallback = &coverStateChanged;
  finalPosition0.onLowCallback = &coverStateChanged;
  finalPosition1.onLowCallback = &coverStateChanged;

  switchLights.onLowCallback = &switchLightsPushed;
  switchYardGate.onLowCallback = &switchYardGatePushed;

  shiftOutput.begin(SR_DATA_PIN, SR_LOAD_PIN, SR_CLOCK_PIN);

  // wifi startup
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print('.');
    delay(100);
  }
  Serial.println("Connected to Wi-Fi sucessfully.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  client.setServer(mqttServer, mqttPort);
  client.setCallback(mqtt_callback);

  shiftOutput.digitalWrite(OUTPUT_COVER_SWITCH, LOW);

  ArduinoOTA.begin();
}

void loop()
{
  loopIndex++;
  if (loopIndex > 11)
  {
    loopIndex = 0;
  }

  ArduinoOTA.handle();

  if (!client.connected())
    mqtt_reconnect();
  client.loop();

  if (loopIndex == 0)
    door0.read();

  if (loopIndex == 1)
    door1.read();

  if (loopIndex == 2)
    switchLights.read();

  if (loopIndex == 3)
    switchYardGate.read();

  if (loopIndex == 4)
    motor0.read();

  if (loopIndex == 5)
    motor1.read();

  if (loopIndex == 6)
    finalPosition0.read();

  if (loopIndex == 7)
    finalPosition1.read();

  if (loopIndex == 8)
    handleLights();

  if (loopIndex == 9)
    handleCover();

  if (loopIndex == 10 && (lastMqttStatusUpdate + 10 * 60 * 1000) < millis())
    mqtt_send_status(true);
}

void handleCover()
{
  if (coverDesiredState != COVER_STATE_UNKNOWN)
  {
    if (coverDesiredState == coverModus)
      coverDesiredState = COVER_STATE_UNKNOWN;
    else if (coverModus == COVER_STATE_OPENED || coverModus == COVER_STATE_CLOSED || coverModus == COVER_STATE_UNKNOWN)
      pressCoverSwitch();
  }

  if (coverSwitchPressed && (coverSwitchPressedTime + COVER_SWITCH_PRESS_TIME) < millis())
  {
    shiftOutput.digitalWrite(OUTPUT_COVER_SWITCH, LOW);
    coverSwitchPressed = false;
  }
}

void pressCoverSwitch()
{
  if (coverSwitchPressed)
    return;
  shiftOutput.digitalWrite(OUTPUT_COVER_SWITCH, HIGH);
  coverSwitchPressed = true;
  coverSwitchPressedTime = millis();
}

void switchLightsPushed()
{
  boolean light0 = readRelay(0);
  boolean light1 = readRelay(1);
  boolean light2 = readRelay(2);

  // alle an => alle aus
  if (light0 && light1 && light2)
  {
    changeRelay(0, LOW);
    changeRelay(1, LOW);
    changeRelay(2, LOW);
    lightTimeoutSet = false;
    return;
  }

  // licht0 an, aber licht1 aus => licht1 auch an
  if (light0 && !light1)
  {
    changeRelay(1, HIGH);
    setLightTimeout(20 * 60); // 20 minutes
    return;
  }

  // licht1 an, aber licht2 aus => licht2 auch an
  if (light1 && !light2)
  {
    changeRelay(2, HIGH);
    setLightTimeout(20 * 60); // 20 minutes
    return;
  }

  // licht0 aus => licht0 auch an
  if (!light0)
  {
    changeRelay(0, HIGH);
    setLightTimeout(20 * 60); // 20 minutes
    return;
  }
}

void switchYardGatePushed()
{
  client.publish(MQTT_HOFTOR_SWITCH, "1");
  mqtt_send_status(true);
  yardGateModus = YARD_GATE_RUNNING;
  handleLights();
}

void setLightTimeout(uint16_t seconds)
{
  uint32_t current = millis() / 1000;
  if (!lightTimeoutSet || lightTimeout < current + seconds)
    lightTimeout = current + seconds;

  lightTimeoutSet = true;
}

void doorStateChanged()
{
  if (DOOR0_OPEN || DOOR1_OPEN)
    changeRelay(0, HIGH);
  else if (DOOR0_CLOSED && DOOR1_CLOSED)
    setLightTimeout(2 * 60);

  mqtt_send_adebar_garage_door(0, false);
  mqtt_send_adebar_garage_door(1, false);
}

void coverStateChanged()
{
  uint8 oldModus = coverModus;
  if (GATE_FINAL_OPENED && !GATE_MOTOR_RUNNING)
    coverModus = COVER_STATE_OPENED;
  else if (GATE_FINAL_CLOSED && !GATE_MOTOR_RUNNING)
    coverModus = COVER_STATE_CLOSED;
  else if (GATE_MOTOR_OPENING_RUNNING)
    coverModus = COVER_STATE_OPENING;
  else if (GATE_MOTOR_CLOSING_RUNNING)
    coverModus = COVER_STATE_CLOSING;
  else
    coverModus = COVER_STATE_UNKNOWN;

  if (oldModus != coverModus)
  {
    setLightTimeout(10 * 60); // 10 minutes
    mqtt_send_adebar_garage_cover(false);
  }
}

void handleLights()
{
  uint32_t currentMillis = millis();
  uint32_t current = currentMillis / 1000;
  boolean blink = currentMillis / 100 % 2 == 0;

  if ((DOOR0_OPEN || DOOR1_OPEN))
    setLightTimeout(2 * 60);

  if (lightTimeoutSet && lightTimeout > current)
  {
    if (!readRelay(0))
      changeRelay(0, HIGH);

    if (lightTimeout > current + 30)
      shiftOutput.digitalWrite(OUTPUT_5V_TIMEOUT_LED, currentMillis / 300 % 2 == 0);
    else if (lightTimeout > current + 20)
      shiftOutput.digitalWrite(OUTPUT_5V_TIMEOUT_LED, currentMillis / 200 % 2 == 0);
    else if (lightTimeout > current + 10)
      shiftOutput.digitalWrite(OUTPUT_5V_TIMEOUT_LED, blink);
    else
      shiftOutput.digitalWrite(OUTPUT_5V_TIMEOUT_LED, currentMillis / 50 % 2 == 0);
  }
  else if (lightTimeoutSet && lightTimeout < current)
  {
    changeRelay(0, LOW);
    changeRelay(1, LOW);
    changeRelay(2, LOW);
    lightTimeoutSet = false;
  } else {
    shiftOutput.digitalWrite(OUTPUT_5V_TIMEOUT_LED, LOW);
  }

  if (yardGateModus == YARD_GATE_CLOSED)
    shiftOutput.digitalWrite(OUTPUT_5V_YARD_GATE_BLINK_LED, LOW);
  else if (yardGateModus == YARD_GATE_OPENED)
    shiftOutput.digitalWrite(OUTPUT_5V_YARD_GATE_BLINK_LED, HIGH);
  else if (yardGateModus == YARD_GATE_RUNNING)
    shiftOutput.digitalWrite(OUTPUT_5V_YARD_GATE_BLINK_LED, blink);

  if (coverModus == COVER_STATE_CLOSED || coverModus == COVER_STATE_OPENED || coverModus == COVER_STATE_UNKNOWN)
    shiftOutput.digitalWrite(OUTPUT_5V_COVER_BLINK_LED, LOW);
  else if (coverModus == COVER_STATE_CLOSING || coverModus == COVER_STATE_OPENING)
    shiftOutput.digitalWrite(OUTPUT_5V_COVER_BLINK_LED, blink);

  if (coverModus == COVER_STATE_CLOSED || coverModus == COVER_STATE_OPENED || coverModus == COVER_STATE_UNKNOWN)
    shiftOutput.digitalWrite(OUTPUT_5V_COVER_BLINK_LED, LOW);
  else if (coverModus == COVER_STATE_CLOSING || coverModus == COVER_STATE_OPENING)
    shiftOutput.digitalWrite(OUTPUT_5V_COVER_BLINK_LED, blink);
}

void mqtt_reconnect()
{
  if (!client.connected())
  {
    Serial.println("Reconnecting MQTT...");

    if (!client.connect("adebar_garage", mqttUser, mqttPassword))
    {
      Serial.print("failed, rc=");
      Serial.println(client.state());
    }
    else
    {
      client.subscribe("adebar/garage/+/set");
      client.subscribe(MQTT_HOFTOR_CHANNEL);
      Serial.println("MQTT Connected...");

      mqtt_send_status(true);
    }
  }
}

void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
  char buffer[56];

  if (strcmp(topic, MQTT_HOFTOR_CHANNEL) == 0)
  {
    if (!strncmp((char *)payload, "closed", length) || !strncmp((char *)payload, "unknown", length))
      yardGateModus = YARD_GATE_CLOSED;
    else if (!strncmp((char *)payload, "opened", length))
      yardGateModus = YARD_GATE_OPENED;
    else if (!strncmp((char *)payload, "opening", length) || !strncmp((char *)payload, "closing", length) || !strncmp((char *)payload, "running", length))
      yardGateModus = YARD_GATE_RUNNING;
  }

  if (!strcmp(topic, "adebar/garage/cover/set"))
  {
    if (!strncmp((char *)payload, "STOP", length) && (coverModus == COVER_STATE_OPENING || coverModus == COVER_STATE_CLOSING))
      pressCoverSwitch();
    else if (!strncmp((char *)payload, "CLOSE", length))
      coverDesiredState = COVER_STATE_CLOSED;
    else if (!strncmp((char *)payload, "OPEN", length))
      coverDesiredState = COVER_STATE_OPENED;
    return;
  }

  for (uint8_t relay = 0; relay < 8; relay++)
  {
    sprintf(buffer, "adebar/garage/relay%d/set", relay);
    if (!strcmp(topic, buffer))
    {
      if (!strncmp((char *)payload, "ON", length))
      {
        if (relay < 3)              // lights
          setLightTimeout(20 * 60); // 20 minutes
        changeRelay(relay, HIGH);
      }
      else if (!strncmp((char *)payload, "OFF", length))
      {
        if (relay < 3) // lights
          lightTimeoutSet = false;
        changeRelay(relay, LOW);
      }
      return;
    }
  }
}

void changeRelay(uint8_t relay, boolean value)
{
  shiftOutput.digitalWrite(relayBits[relay], !value);
  mqtt_send_adebar_garage_relay(relay, false);
}

void mqtt_send_status(boolean full)
{
  mqtt_send_adebar_garage_cover(full);
  mqtt_send_adebar_garage_ip_address(full);

  mqtt_send_adebar_garage_door(0, full);
  mqtt_send_adebar_garage_door(1, full);

  mqtt_send_adebar_garage_relay(0, full);
  mqtt_send_adebar_garage_relay(1, full);
  mqtt_send_adebar_garage_relay(2, full);
  mqtt_send_adebar_garage_relay(3, full);
  mqtt_send_adebar_garage_relay(4, full);
  mqtt_send_adebar_garage_relay(5, full);
  mqtt_send_adebar_garage_relay(6, full);
  mqtt_send_adebar_garage_relay(7, full);

  lastMqttStatusUpdate = millis();
}

void mqtt_send_adebar_garage_cover(boolean full)
{
  if (full)
  {
    // see https://www.home-assistant.io/integrations/cover.mqtt/
    JsonDocument discoveryConfig;
    discoveryConfig["name"] = "Garagentor";
    discoveryConfig["dev_cla"] = "garage";
    discoveryConfig["cmd_t"] = "adebar/garage/cover/set";
    discoveryConfig["stat_t"] = "adebar/garage/cover/state";
    discoveryConfig["uniq_id"] = "adebar_garage_cover";

    JsonObject device = discoveryConfig["dev"].to<JsonObject>();
    device["identifiers"][0] = "adebar_garage";
    device["name"] = "Garage";

    char buffer[256];
    serializeJson(discoveryConfig, buffer);
    client.publish("homeassistant/cover/adebar_garage_cover/config", buffer);
  }

  if (coverModus == COVER_STATE_CLOSED)
    client.publish("adebar/garage/cover/state", "closed");
  else if (coverModus == COVER_STATE_OPENED)
    client.publish("adebar/garage/cover/state", "open");
  else if (coverModus == COVER_STATE_OPENING)
    client.publish("adebar/garage/cover/state", "opening");
  else if (coverModus == COVER_STATE_CLOSING)
    client.publish("adebar/garage/cover/state", "closing");
  else if (coverModus == COVER_STATE_UNKNOWN)
    client.publish("adebar/garage/cover/state", "unknown");
}

void mqtt_send_adebar_garage_ip_address(boolean full)
{
  if (full)
  {
    // see https://www.home-assistant.io/integrations/sensor.mqtt/
    JsonDocument discoveryConfig;
    discoveryConfig["name"] = "Garage IP-Adresse";
    discoveryConfig["stat_t"] = "adebar/garage/ip_address/state";
    discoveryConfig["uniq_id"] = "adebar_garage_ip_address";

    JsonObject device = discoveryConfig["dev"].to<JsonObject>();
    device["identifiers"][0] = "adebar_garage";
    device["name"] = "Garage";

    char buffer[256];
    serializeJson(discoveryConfig, buffer);
    client.publish("homeassistant/sensor/adebar_garage_ip_address/config", buffer);
  }

  String ip = WiFi.localIP().toString();
  char ip_char[ip.length() + 1];
  ip.toCharArray(ip_char, ip.length() + 1);

  client.publish("adebar/garage/ip_address/state", ip_char);
}

void mqtt_send_adebar_garage_door(uint8_t door, boolean full)
{
  if (full)
  {
    // see https://www.home-assistant.io/integrations/binary_sensor.mqtt/
    JsonDocument discoveryConfig;
    discoveryConfig["name"] = door == 0 ? "Garage Vordertuer" : "Garage Hintertuer";
    discoveryConfig["dev_cla"] = "door";
    discoveryConfig["stat_t"] = door == 0 ? "adebar/garage/door_front/state" : "adebar/garage/door_back/state";
    discoveryConfig["uniq_id"] = door == 0 ? "adebar_garage_door_front" : "adebar_garage_door_back";

    JsonObject device = discoveryConfig["dev"].to<JsonObject>();
    device["identifiers"][0] = "adebar_garage";
    device["name"] = "Garage";

    char buffer[256];
    serializeJson(discoveryConfig, buffer);

    client.publish(door == 0 ? "homeassistant/binary_sensor/adebar_garage_door_front/config" : "homeassistant/binary_sensor/adebar_garage_door_back/config", buffer);
  }

  if (door == 0)
    client.publish("adebar/garage/door_front/state", DOOR0_OPEN ? "ON" : "OFF");
  else
    client.publish("adebar/garage/door_back/state", DOOR1_OPEN ? "ON" : "OFF");
}

void mqtt_send_adebar_garage_relay(uint8_t relay, boolean full)
{
  char bufferSmall[56];
  char bufferBig[256];

  if (full)
  {
    // see https://www.home-assistant.io/integrations/switch.mqtt/
    JsonDocument discoveryConfig;

    sprintf(bufferSmall, "Garagenschalter %d", relay);
    discoveryConfig["name"] = bufferSmall;

    sprintf(bufferSmall, "adebar/garage/relay%d/state", relay);
    discoveryConfig["stat_t"] = bufferSmall;

    sprintf(bufferSmall, "adebar/garage/relay%d/set", relay);
    discoveryConfig["cmd_t"] = bufferSmall;

    sprintf(bufferSmall, "adebar_garage_relay%d", relay);
    discoveryConfig["uniq_id"] = bufferSmall;

    JsonObject device = discoveryConfig["dev"].to<JsonObject>();
    device["identifiers"][0] = "adebar_garage";
    device["name"] = "Garage";

    serializeJson(discoveryConfig, bufferBig);
    sprintf(bufferSmall, "homeassistant/switch/adebar_garage_relay%d/config", relay);
    client.publish(bufferSmall, bufferBig);
  }

  sprintf(bufferSmall, "adebar/garage/relay%d/state", relay);
  client.publish(bufferSmall, readRelay(relay) ? "ON" : "OFF");
}
