#include "DHT.h"

const int moisturePin = A0; 
const int flamePin = 2;
const int pirPin = 3;
const int dhtPin = 7;     
const int dhtType = DHT11;
const int soilPowerPin = 5;
const unsigned long soilReadingInterval = 5000;

const int flameLedPin = 9;
const int pirLedPin = 10;

DHT dht(dhtPin, dhtType);

unsigned long lastMoistureTime = 0;
unsigned long lastDhtTime = 0;
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
    {
        int motionValue = digitalRead(pirPin);
        if (motionValue == HIGH) {
            digitalWrite(pirLedPin, HIGH);
    
            if (pirState == LOW) {
                Serial.println("Motion detected!");
                pirState = HIGH;
            }
        } 
        else {
            digitalWrite(pirLedPin, LOW);

            if (pirState == HIGH) {
                Serial.println("Motion ended.");
                pirState = LOW;
            }
        }
    }

    //FIRE
    if (flameDetected && !fireAlarmActive) {
        Serial.println("!!! FIRE DETECTED !!!");
        fireAlarmActive = true;
        flameDetected = false;
        digitalWrite(flameLedPin, HIGH);
        lastFlameTime = millis();
    }

    if (fireAlarmActive && (millis() - lastFlameTime >= 5000)) {
        Serial.println("----------Stopping fire alarm-----------");
        fireAlarmActive = false;
        flameDetected = false;
        digitalWrite(flameLedPin, LOW);
    }
    //SOIL MOISTURE
    if (millis() - lastMoistureTime >= soilReadingInterval) {
        digitalWrite(soilPowerPin, HIGH);
        delay(20);
        Serial.print("{\"soil_moisture\":{\"raw\": [");
        int rawSamples[10];

        for(int i = 0; i < 10; i++) {
            rawSamples[i] = analogRead(moisturePin);
            Serial.print(rawSamples[i]);
            if (i < 9) Serial.print(",");
            delay(10);
        }

        Serial.print("], \"percentage\": [");

        for(int i = 0; i < 10; i++) {
            int percent = map(rawSamples[i], 1023, 300, 0, 100);
            percent = constrain(percent, 0, 100);
            Serial.print(percent);
            if (i < 9) Serial.print(",");
        }
        Serial.println("]}}");
        digitalWrite(soilPowerPin, LOW);
        lastMoistureTime = millis();
    }

    //TEMP + HUMIDITY
    if (millis() - lastDhtTime >= 2000) {
        float humidity = dht.readHumidity();
        float tempC = dht.readTemperature();

        if (isnan(humidity) || isnan(tempC)) {
            Serial.println("Failed to read from DHT sensor! Check wiring.");
            return;
        }

        Serial.print("Humidity: ");
        Serial.print(humidity);
        Serial.print("%  |  ");
        
        Serial.print("Temperature: ");
        Serial.print(tempC);
        Serial.println("°C");

        lastDhtTime = millis();
    }
}

void fireISR() {
  flameDetected = true;
}