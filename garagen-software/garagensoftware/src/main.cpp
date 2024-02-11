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

uint8_t lightModus = LIGHTS_OFF;
uint8_t yardGateModus = YARD_GATE_CLOSED;
uint8_t garageGateModus = GARAGE_GATE_CLOSED;

uint32_t lightTimeout = 0;
bool lightTimeoutSet = false;
uint32_t gateSwitchPressedTime = 0;
bool gateSwitchPressed = false;
uint8_t desiredState = GARAGE_GATE_CLOSED;

WiFiClient espClient;
PubSubClient client(espClient);
uint32_t lastMqttStatusUpdate = 0;

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

  motor0.onHighCallback = &gateStateChanged;
  motor0.onLowCallback = &gateStateChanged;
  motor1.onHighCallback = &gateStateChanged;
  motor1.onLowCallback = &gateStateChanged;
  finalPosition0.onLowCallback = &gateStateChanged;
  finalPosition1.onLowCallback = &gateStateChanged;

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

  shiftOutput.digitalWrite(SWITCH_MOTOR, LOW);
}

void loop()
{
  if (!client.connected())
    mqtt_reconnect();
  client.loop();

  door0.read();
  door1.read();
  switchLights.read();
  switchYardGate.read();
  motor0.read();
  motor1.read();
  finalPosition0.read();
  finalPosition1.read();

  handleLights();

  if (desiredState != GARAGE_GATE_UNKNOWN)
  {
    if (desiredState == garageGateModus)
      desiredState = GARAGE_GATE_UNKNOWN;
    else if (garageGateModus == GARAGE_GATE_OPENED || garageGateModus == GARAGE_GATE_CLOSED || garageGateModus == GARAGE_GATE_UNKNOWN)
      pressSwitch();
  }

  if (gateSwitchPressed && (gateSwitchPressedTime + GARAGE_PRESS_TIME) < millis())
  {
    shiftOutput.digitalWrite(SWITCH_MOTOR, LOW);
    gateSwitchPressed = false;
    Serial.println("Switch off");
  }

  if ((lastMqttStatusUpdate + MQTT_STATUS_UPDATE_TIME) < millis())
    mqtt_send_status();
}

void pressSwitch()
{
  if (gateSwitchPressed)
    return;
  Serial.println("Switch on");
  shiftOutput.digitalWrite(SWITCH_MOTOR, HIGH);
  gateSwitchPressed = true;
  gateSwitchPressedTime = millis();
}

void switchLightsPushed()
{
  lightModus++;
  if (lightModus > LIGHTS_MIDDLE_RIGHT_MOTOR)
    lightModus = LIGHTS_OFF;
  handleLights();
  mqtt_send_status();
}

void switchYardGatePushed()
{
  client.publish(MQTT_HOFTOR_SWITCH, "1");
  mqtt_send_status();
  yardGateModus = YARD_GATE_RUNNING;
  handleLights();
}

void doorStateChanged()
{
  if ((DOOR0_OPEN || DOOR1_OPEN) && lightModus == LIGHTS_OFF)
    lightModus = LIGHTS_MIDDLE;

  else if (DOOR0_CLOSED && DOOR1_CLOSED && lightModus == LIGHTS_MIDDLE)
    lightModus = LIGHTS_OFF;

  lightTimeout = millis() + DOOR_TIMEOUT;
  lightTimeoutSet = true;

  handleLights();
  mqtt_send_status();
}

void gateStateChanged()
{
  uint8 oldModus = garageGateModus;
  if (GATE_FINAL_OPENED && !GATE_MOTOR_RUNNING)
    garageGateModus = GARAGE_GATE_OPENED;
  else if (GATE_FINAL_CLOSED && !GATE_MOTOR_RUNNING)
    garageGateModus = GARAGE_GATE_CLOSED;
  else if (GATE_MOTOR_OPENING_RUNNING)
    garageGateModus = GARAGE_GATE_OPENING;
  else if (GATE_MOTOR_CLOSING_RUNNING)
    garageGateModus = GARAGE_GATE_CLOSING;
  else
    garageGateModus = GARAGE_GATE_UNKNOWN;

  if (oldModus != garageGateModus)
  {
    lightTimeout = millis() + GATE_TIMEOUT;
    lightTimeoutSet = true;
    handleLights();
    mqtt_send_status();
  }
}

void handleLights()
{

  if ((DOOR0_OPEN || DOOR1_OPEN))
  {
    lightTimeout = millis() + DOOR_TIMEOUT;
    lightTimeoutSet = true;
  }

  if (lightModus == LIGHTS_OFF)
  {
    shiftOutput.digitalWrite(RELAIS_1, HIGH);
    shiftOutput.digitalWrite(RELAIS_2, HIGH);
    shiftOutput.digitalWrite(RELAIS_3, HIGH);
  }
  else if (lightModus == LIGHTS_MIDDLE)
  {
    shiftOutput.digitalWrite(RELAIS_1, LOW);
    shiftOutput.digitalWrite(RELAIS_2, HIGH);
    shiftOutput.digitalWrite(RELAIS_3, HIGH);
  }
  else if (lightModus == LIGHTS_MIDDLE_RIGHT)
  {

    shiftOutput.digitalWrite(RELAIS_1, LOW);
    shiftOutput.digitalWrite(RELAIS_2, LOW);
    shiftOutput.digitalWrite(RELAIS_3, HIGH);
    if (lightTimeoutSet)
      lightTimeoutSet = false;
  }
  else if (lightModus == LIGHTS_MIDDLE_RIGHT_MOTOR)
  {
    shiftOutput.digitalWrite(RELAIS_1, LOW);
    shiftOutput.digitalWrite(RELAIS_2, LOW);
    shiftOutput.digitalWrite(RELAIS_3, LOW);
    if (lightTimeoutSet)
      lightTimeoutSet = false;
  }

  if (lightTimeoutSet && lightTimeout > millis())
  {
    lightModus = LIGHTS_MIDDLE;
    if (lightTimeout > millis() + 45 * 1000)
      shiftOutput.digitalWrite(SWITCH_5V_TIMEOUT, HIGH);
    else if (lightTimeout > millis() + 30 * 1000)
      shiftOutput.digitalWrite(SWITCH_5V_TIMEOUT, millis() / 300 % 2 == 0);
    else if (lightTimeout > millis() + 20 * 1000)
      shiftOutput.digitalWrite(SWITCH_5V_TIMEOUT, millis() / 200 % 2 == 0);
    else if (lightTimeout > millis() + 10 * 1000)
      shiftOutput.digitalWrite(SWITCH_5V_TIMEOUT, millis() / 100 % 2 == 0);
    else
      shiftOutput.digitalWrite(SWITCH_5V_TIMEOUT, millis() / 50 % 2 == 0);
  }
  else if (lightTimeoutSet && lightTimeout < millis())
  {
    lightModus = LIGHTS_OFF;
    lightTimeoutSet = false;
    mqtt_send_status();
    shiftOutput.digitalWrite(SWITCH_5V_TIMEOUT, LOW);
  }

  if (yardGateModus == YARD_GATE_CLOSED)
    shiftOutput.digitalWrite(SWITCH_5V_YARD_GATE, LOW);
  else if (yardGateModus == YARD_GATE_OPENED)
    shiftOutput.digitalWrite(SWITCH_5V_YARD_GATE, HIGH);
  else if (yardGateModus == YARD_GATE_RUNNING)
    shiftOutput.digitalWrite(SWITCH_5V_YARD_GATE, millis() / 100 % 2 == 0);

  if (garageGateModus == GARAGE_GATE_CLOSED || garageGateModus == GARAGE_GATE_OPENED || garageGateModus == GARAGE_GATE_UNKNOWN)
    shiftOutput.digitalWrite(SWITCH_5V_GARAGE_GATE, LOW);
  else if (garageGateModus == GARAGE_GATE_CLOSING || garageGateModus == GARAGE_GATE_OPENING)
    shiftOutput.digitalWrite(SWITCH_5V_GARAGE_GATE, millis() / 100 % 2 == 0);

  if (garageGateModus == GARAGE_GATE_CLOSED || garageGateModus == GARAGE_GATE_OPENED || garageGateModus == GARAGE_GATE_UNKNOWN)
    shiftOutput.digitalWrite(SWITCH_5V_GARAGE_GATE, LOW);
  else if (garageGateModus == GARAGE_GATE_CLOSING || garageGateModus == GARAGE_GATE_OPENING)
    shiftOutput.digitalWrite(SWITCH_5V_GARAGE_GATE, millis() / 100 % 2 == 0);
}

void mqtt_reconnect()
{
  if (!client.connected())
  {
    Serial.println("Reconnecting MQTT...");

    if (!client.connect("garage", mqttUser, mqttPassword))
    {
      Serial.print("failed, rc=");
      Serial.println(client.state());
    }
    else
    {
      client.subscribe(MQTT_GATE_SWITCH);
      client.subscribe(MQTT_GATE_OPEN);
      client.subscribe(MQTT_GATE_CLOSE);
      client.subscribe(MQTT_LIGHTS_SET);
      client.subscribe(MQTT_HOFTOR_CHANNEL);
      Serial.println("MQTT Connected...");
    }
  }
}

void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("MQTT: receive ");
  Serial.println(topic);
  if (strcmp(topic, MQTT_GATE_SWITCH) == 0)
    pressSwitch();
  else if (strcmp(topic, MQTT_GATE_OPEN) == 0)
    desiredState = GARAGE_GATE_OPENED;
  else if (strcmp(topic, MQTT_GATE_CLOSE) == 0)
    desiredState = GARAGE_GATE_CLOSED;
  else if (strcmp(topic, MQTT_LIGHTS_SET) == 0)
  {
    if (!strncmp((char *)payload, "0", length))
      lightModus = LIGHTS_OFF;
    else if (!strncmp((char *)payload, "1", length))
      lightModus = LIGHTS_MIDDLE;
    else if (!strncmp((char *)payload, "2", length))
      lightModus = LIGHTS_MIDDLE_RIGHT;
    else if (!strncmp((char *)payload, "3", length))
      lightModus = LIGHTS_MIDDLE_RIGHT_MOTOR;
    handleLights();
  }
  else if (strcmp(topic, MQTT_HOFTOR_CHANNEL) == 0)
  {
    if (!strncmp((char *)payload, "closed", length) || !strncmp((char *)payload, "unknown", length))
      yardGateModus = YARD_GATE_CLOSED;
    else if (!strncmp((char *)payload, "opened", length))
      yardGateModus = YARD_GATE_OPENED;
    else if (!strncmp((char *)payload, "opening", length) || !strncmp((char *)payload, "closing", length) || !strncmp((char *)payload, "running", length))
      yardGateModus = YARD_GATE_RUNNING;
  }
}

void mqtt_send_status()
{
  char buffer[2];

  sprintf(buffer, "%d", lightModus);
  client.publish(MQTT_LIGHTS_STATE, buffer);
  client.publish(MQTT_DOOR0_STATE, DOOR0_OPEN ? "1" : "0");
  client.publish(MQTT_DOOR1_STATE, DOOR1_OPEN ? "1" : "0");
  client.publish(MQTT_SWITCH_LIGHTS_STATE, SWITCH_LIGHTS_PRESSED ? "1" : "0");
  client.publish(MQTT_SWITCH_YARD_GATE_STATE, SWITCH_YARD_GATE_PRESSED ? "1" : "0");

  if (garageGateModus == GARAGE_GATE_CLOSED)
    client.publish(MQTT_GATE_STATE, "closed");
  else if (garageGateModus == GARAGE_GATE_OPENED)
    client.publish(MQTT_GATE_STATE, "opened");
  else if (garageGateModus == GARAGE_GATE_OPENING)
    client.publish(MQTT_GATE_STATE, "opening");
  else if (garageGateModus == GARAGE_GATE_CLOSING)
    client.publish(MQTT_GATE_STATE, "closing");
  else if (garageGateModus == GARAGE_GATE_UNKNOWN)
    client.publish(MQTT_GATE_STATE, "unknown");

  motor0.debug();
  motor1.debug();
  finalPosition0.debug();
  finalPosition1.debug();

  Serial.println(lightTimeoutSet);
  Serial.println(lightTimeout);
  Serial.println(millis());

  lastMqttStatusUpdate = millis();
}