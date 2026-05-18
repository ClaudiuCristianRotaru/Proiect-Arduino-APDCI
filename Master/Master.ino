#include "DHT.h"
#include <avr/wdt.h>
#include <ArduinoJson.h>

// 1. Create a global variable to "capture" the state
volatile uint8_t mcusr_mirror __attribute__ ((section (".noinit")));

// 2. This function runs BEFORE setup()
void get_mcusr(void) __attribute__((naked)) __attribute__((section(".init0")));
void get_mcusr(void) {
  mcusr_mirror = MCUSR;
  MCUSR = 0;
  wdt_disable();
}


const byte moisturePin = A0; 
const byte flamePin = 2;
const byte pirPin = 3;
const byte soilPowerPin = 5;
const byte dhtPin = 7;     
const byte flameLedPin = 9;
const byte pirLedPin = 10;
const byte dhtType = DHT11;

const char* nodeId = "CA_01";
const unsigned long soilReadingInterval = 10000;
const unsigned long fireAlarmDuration = 10000;
const unsigned long climateReadingInterval = 10000; //keep above 2000ms

DHT dht(dhtPin, dhtType);

unsigned long lastMoistureTime = 0;
unsigned long lastClimateTime = 0;
unsigned long lastFlameTime = 0;

volatile bool flameDetected = false;
byte pirState = LOW;
bool fireAlarmActive = false;

void setup()
{
    Serial.begin(115200);
    Serial.println("Initializing agricultural monitoring...");

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    Serial.print("Soil Moisture Sensor Starting...");
    Serial.println("Ready!");

    Serial.print("Flame Sensor Starting...");
    attachInterrupt(digitalPinToInterrupt(flamePin), fireISR, FALLING);
    pinMode(flameLedPin, OUTPUT);
    Serial.println("Ready!");

    Serial.print("Warming up PIR...");
    while(digitalRead(pirPin) == HIGH) {
        delay(100);
    }
    pinMode(soilPowerPin, OUTPUT);
    pinMode(pirLedPin, OUTPUT);
    Serial.println("Ready!");

    Serial.print("DHT11 Temperature Sensor Starting... ");
    dht.begin();
    Serial.println("Ready!");

    StaticJsonDocument<200> doc;
    doc["type"] = "system_log";
    doc["node"] = nodeId;
    doc["subject"] = "boot_up";
    doc["status"] = "ok";
    JsonObject payload = doc.createNestedObject("payload");
    payload["reason"] = getResetReason();
    payload["uptime"] = millis();

    serializeJson(doc, Serial);
    Serial.println();
    MCUSR = 0;

    digitalWrite(LED_BUILTIN, HIGH);

}

void loop()
{
    //PIR
    checkPIR();

    //FIRE
    checkFlame();

    //SOIL MOISTURE
    checkSoilMoisture();

    //TEMP + HUMIDITY
    checkClimate();
}

void fireISR() {
  flameDetected = true;
}

void checkPIR()
{
    byte motionValue = digitalRead(pirPin);
    if (motionValue == HIGH) {
        digitalWrite(pirLedPin, HIGH);

        if (pirState == LOW) {

            StaticJsonDocument<200> doc;
            doc["type"] = "alert";
            doc["node"] = nodeId;
            doc["subject"] = "pir";
            doc["status"] = "ok";
            JsonObject payload = doc.createNestedObject("payload");
            payload["active"] = true;

            serializeJson(doc, Serial);
            Serial.println();

            pirState = HIGH;
        }
    } 
    else {
        digitalWrite(pirLedPin, LOW);

        if (pirState == HIGH) {
            pirState = LOW;
        }
    }
}

void checkFlame() {
    if (flameDetected && !fireAlarmActive) {
        fireAlarmActive = true;
        flameDetected = false;
        digitalWrite(flameLedPin, HIGH);

        StaticJsonDocument<200> doc;
        doc["type"] = "alert";
        doc["node"] = nodeId;
        doc["subject"] = "flame";
        doc["status"] = "ok";
        JsonObject payload = doc.createNestedObject("payload");
        payload["active"] = true;

        serializeJson(doc, Serial);
        Serial.println();

        lastFlameTime = millis();
    }

    if (fireAlarmActive && (millis() - lastFlameTime >= fireAlarmDuration)) {
        fireAlarmActive = false;
        flameDetected = false;
        digitalWrite(flameLedPin, LOW);
    }
}

void checkSoilMoisture() {
    if (millis() - lastMoistureTime < soilReadingInterval) 
        return;

    digitalWrite(soilPowerPin, HIGH);
    delay(20);

    int sum = 0;
    bool failed = false;
    for(int i = 0; i < 10; i++) {
        int value = analogRead(moisturePin);
        if(value < 300 || value >= 1023)
            failed = true;
        sum += value;
        delay(10);
    }

    int rawAverage = sum / 10;
    int perAverage = map(rawAverage, 1023, 300, 0, 100);
    perAverage = constrain(perAverage, 0, 100);

    StaticJsonDocument<200> doc;
    doc["type"] = "telemetry";
    doc["node"] = nodeId;
    doc["subject"] = "soil_moisture";

    if(failed)
    {
        doc["status"] = "error";
    }
    else 
    {
        doc["status"] = "ok";
        JsonObject payload = doc.createNestedObject("payload");
        payload["raw"] = rawAverage;
        payload["percentage"] = perAverage;
    }
    serializeJson(doc, Serial);
    Serial.println();

    digitalWrite(soilPowerPin, LOW);
    lastMoistureTime = millis();
}

void checkClimate() {
    if (millis() - lastClimateTime < climateReadingInterval) 
        return;

    float humidity = dht.readHumidity();
    float tempC = dht.readTemperature();

    StaticJsonDocument<200> doc;
    doc["type"] = "telemetry";
    doc["node"] = nodeId;
    doc["subject"] = "climate";

    if (isnan(humidity) || isnan(tempC)) {
        doc["status"] = "error";
    }
    else 
    {
        doc["status"] = "ok";
        JsonObject payload = doc.createNestedObject("payload");
        payload["temperatureC"] = tempC;
        payload["humidity"] = humidity;
    }

    serializeJson(doc, Serial);
    Serial.println();

    lastClimateTime = millis();
}

String getResetReason() {
// If the mirror is 0, the bootloader already wiped the evidence
  if (mcusr_mirror == 0) return "cleared_by_bootloader";

  if (mcusr_mirror & (1 << WDRF))  return "watchdog"; //code issues
  if (mcusr_mirror & (1 << BORF))  return "brownout"; //voltage too low
  if (mcusr_mirror & (1 << EXTRF)) return "reset_button"; //physical reset button
  if (mcusr_mirror & (1 << PORF))  return "power_on";
  return "unknown";
}

