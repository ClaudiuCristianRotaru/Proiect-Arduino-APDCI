#include "DHT.h"
#include <ArduinoJson.h>

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

    Serial.print("Soil Moisture Sensor Starting...");
    Serial.println("Ready!");

    Serial.print("Flame Sensor Starting...");
    attachInterrupt(digitalPinToInterrupt(flamePin), fireISR, FALLING);
    pinMode(flameLedPin, OUTPUT);
    Serial.println("Ready!");

    Serial.print("PIR Sensor Warming Up for 30s...");
    delay(30000); 
    pinMode(soilPowerPin, OUTPUT);
    pinMode(pirLedPin, OUTPUT);
    Serial.println("Ready!");

    Serial.print("DHT11 Temperature Sensor Starting... ");
    dht.begin();
    Serial.println("Ready!");
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
            doc["sensor"] = "pir";
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
        doc["sensor"] = "flame";
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
    doc["sensor"] = "soil_moisture";

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
    doc["sensor"] = "climate";

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

