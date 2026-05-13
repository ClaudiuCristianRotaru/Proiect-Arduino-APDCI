#include "DHT.h"

const int moisturePin = A0; 
const int flamePin = 2;
const int pirPin = 3;
const int dhtPin = 7;     
const int dhtType = DHT11;

const int flameLedPin = 13;

DHT dht(dhtPin, dhtType);

unsigned long lastMoistureTime = 0;
unsigned long lastDhtTime = 0;
unsigned long lastFlameTime = 0;
volatile bool flameDetected = false;
int pirState = LOW;
bool fireAlarmActive = false;

void setup()
{
    Serial.begin(9600);
    Serial.println("Initializing agricultural monitoring...");

    Serial.print("Soil Moisture Sensor Starting...");
    Serial.println("Ready!");

    Serial.print("Flame Sensor Starting...");
    attachInterrupt(digitalPinToInterrupt(flamePin), fireISR, FALLING);
    Serial.println("Ready!");

    Serial.print("PIR Sensor Warming Up for 30s...");
    delay(30000); 
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
            //digitalWrite(ledPin, HIGH);
    
            if (pirState == LOW) {
                Serial.println("Motion detected!");
                pirState = HIGH;
            }
        } 
        else {
            //digitalWrite(ledPin, LOW);

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
    if (millis() - lastMoistureTime >= 1000) {
        int rawValue = analogRead(moisturePin);
        int moisturePercentage = map(rawValue, 1023, 300, 0, 100);
        moisturePercentage = constrain(moisturePercentage, 0, 100);

        Serial.print("Raw Soil Moisture Sensor Value: ");
        Serial.print(rawValue);
        Serial.print("  | Soil Moisture Level: ");
        Serial.print(moisturePercentage);
        Serial.println("%");

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