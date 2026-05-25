#include "../.env"
#define BLYNK_PRINT Serial
#define BLYNK_FIRMWARE_VERSION "0.1.0"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32_SSL.h>

const int sensor = 13;     // GPIO Pin number on which door sensor is connected
const int door_relay = 18; // GPIO pin number for relay

enum DoorState
{
  OPEN,
  OPENING,
  CLOSING,
  CLOSED
};

BlynkTimer timer;
unsigned long bootTime;
unsigned long lastCommandTime = 0;
unsigned long lastBlynkConnectAttempt = 0;
unsigned long firstBlynkConnectAttempt = 0;
const unsigned long commandCooldown = 15000;    // 15 seconds
const unsigned long sensorIntervalSeconds = 10; // default 45 seconds
const unsigned long blynkReconnectInterval = 5000;
const unsigned long blynkReconnectTimeout = 5UL * 60UL * 1000UL;

// Returns true when the sensor reports the door is open.
// Hardware mapping: sensor == 1 -> OPEN, sensor == 0 -> CLOSED
bool isDoorOpen()
{
  return digitalRead(sensor) == 0;
}

// Write door state enum to V3 virtual pin
void writeDoorState(DoorState state)
{
  switch (state)
  {
  case OPEN:
    Blynk.logEvent("door_open");
    Blynk.virtualWrite(V3, "OPEN");
    break;
  case OPENING:
    Blynk.logEvent("door_opening");
    Blynk.virtualWrite(V3, "OPENING");
    break;
  case CLOSING:
    Blynk.virtualWrite(V3, "CLOSING");
    break;
  case CLOSED:
    Blynk.virtualWrite(V3, "CLOSED");
    break;
  }
}

// Trigger the door relay with a pulse
void triggerRelay()
{
  digitalWrite(door_relay, LOW); // ON
  delay(250);
  digitalWrite(door_relay, HIGH); // OFF
  Serial.println("Relay pulse complete");
}

// This function is called every time the app writes to pin V1 (OPEN/CLOSE commands)
BLYNK_WRITE(V1)
{
  if (millis() - bootTime < 10000)
  {
    Serial.println("Ignoring startup command");
    return;
  }
  // Ignore commands during cooldown (during door transitions)
  if (millis() - lastCommandTime < commandCooldown)
  {
    Serial.println("Command ignored - cooldown active");
    return;
  }
  int incOpenClose = param.asInt();

  Serial.print("Received V1 value: ");
  Serial.println(incOpenClose);

  Serial.print("Sensor state: ");
  Serial.println(digitalRead(sensor));

  if (incOpenClose != 0 && incOpenClose != 1)
  {
    return;
  }

  // OPEN COMMAND
  if (incOpenClose == 0)
  {

    Serial.println("OPEN command received");

    if (!isDoorOpen())
    {

      Serial.println("Door currently closed -> triggering relay");
      writeDoorState(OPENING);
      triggerRelay();
      lastCommandTime = millis();
    }
    else
    {
      Serial.println("Door already open");
    }
  }

  // CLOSE COMMAND
  if (incOpenClose == 1)
  {

    Serial.println("CLOSE command received");

    if (isDoorOpen())
    {

      Serial.println("Door currently open -> triggering relay");
      writeDoorState(CLOSING);
      triggerRelay();
      lastCommandTime = millis();
    }
    else
    {
      Serial.println("Door already closed");
    }
  }
}

BLYNK_CONNECTED()
{
  Serial.println("Blynk connected - sending current sensor state");
  writeDoorState(isDoorOpen() ? OPEN : CLOSED);
}

void myTimerEvent()
{
  static bool hasLastReportedDoorOpen = false;
  static bool lastReportedDoorOpen = false;

  // During cooldown show transitional state
  if (millis() - lastCommandTime < commandCooldown)
  {
    return;
  }

  // Otherwise show actual sensor state
  bool doorOpen = isDoorOpen();

  if (!hasLastReportedDoorOpen || doorOpen != lastReportedDoorOpen)
  {
    writeDoorState(doorOpen ? OPEN : CLOSED);
    lastReportedDoorOpen = doorOpen;
    hasLastReportedDoorOpen = true;
  }
}

void ensureBlynkConnection()
{
  unsigned long now = millis();

  if (firstBlynkConnectAttempt == 0)
  {
    firstBlynkConnectAttempt = now;
    lastBlynkConnectAttempt = 0;
  }

  if (Blynk.connected())
  {
    firstBlynkConnectAttempt = 0;
    lastBlynkConnectAttempt = 0;
    return;
  }

  if (now - firstBlynkConnectAttempt >= blynkReconnectTimeout)
  {
    Serial.println("Blynk connection timed out - restarting ESP");
    delay(100);
    ESP.restart();
  }

  if (lastBlynkConnectAttempt != 0 && now - lastBlynkConnectAttempt < blynkReconnectInterval)
  {
    return;
  }

  lastBlynkConnectAttempt = now;

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi not connected - retrying WiFi");
    WiFi.begin(ssid, pass);
    return;
  }

  Serial.println("Attempting to connect to Blynk...");

  if (Blynk.connect(1000))
  {
    Serial.println("Blynk connection established");
    firstBlynkConnectAttempt = 0;
    lastBlynkConnectAttempt = 0;
  }
}

void setup()
{
  Serial.begin(115200);

  // Active LOW relay -> HIGH means OFF
  pinMode(door_relay, OUTPUT);
  digitalWrite(door_relay, HIGH);
  bootTime = millis();

  pinMode(sensor, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  Blynk.config(BLYNK_AUTH_TOKEN);

  timer.setInterval(sensorIntervalSeconds * 1000UL, myTimerEvent);
}

void loop()
{
  ensureBlynkConnection();
  Blynk.run();
  timer.run();
}