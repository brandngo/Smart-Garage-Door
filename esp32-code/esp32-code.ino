#include "../.env"
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

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
const unsigned long commandCooldown = 15000;   // 15 seconds
const unsigned long sensorIntervalSeconds = 3; // default 3 seconds

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

BLYNK_CONNECTED() {}

void myTimerEvent()
{
  // During cooldown show transitional state
  if (millis() - lastCommandTime < commandCooldown)
  {
    return;
  }

  // Otherwise show actual sensor state
  if (isDoorOpen())
  {
    writeDoorState(OPEN);
  }
  else
  {
    writeDoorState(CLOSED);
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

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  timer.setInterval(sensorIntervalSeconds * 1000UL, myTimerEvent);
}

void loop()
{
  Blynk.run();
  timer.run();
}