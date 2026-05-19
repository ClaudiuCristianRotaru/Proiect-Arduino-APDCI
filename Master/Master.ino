#include "DHT.h"
#include <avr/wdt.h>
#include <ArduinoJson.h>

volatile uint8_t mcusr_mirror __attribute__ ((section (".noinit")));
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
const unsigned long heartbeatInterval = 60000;
const unsigned long pirWarmupTimeout = 60000;

DHT dht(dhtPin, dhtType);

unsigned long lastHeartbeatTime = 0;
unsigned long lastMoistureTime = 0;
unsigned long lastClimateTime = 0;
unsigned long lastFlameTime = 0;

volatile bool flameDetected = false;
byte pirState = LOW;
bool fireAlarmActive = false;

void setup()
{
    unsigned long setupStartTime = millis();
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
    while(digitalRead(pirPin) == HIGH && (millis()-setupStartTime < pirWarmupTimeout)) {
        delay(100);
    }

    if (digitalRead(pirPin) == HIGH) {
        StaticJsonDocument<128> p;
        p["warmup_timeout"] = true;
        sendJson("alert", "pir", "error", p.as<JsonVariantConst>());
    }

    pinMode(soilPowerPin, OUTPUT);
    pinMode(pirLedPin, OUTPUT);
    Serial.println("Ready!");

    Serial.print("DHT11 Temperature Sensor Starting... ");
    dht.begin();
    lastClimateTime = millis();
    Serial.println("Ready!");

    StaticJsonDocument<128> p;
    p["reason"] = getResetReason();
    p["uptime"] = millis();
    p["online"] = true;
    p["free_ram"] = getFreeRam();
    sendJson("system_log", "boot_up", "ok", p.as<JsonVariantConst>());

    MCUSR = 0;
    wdt_enable(WDTO_8S);
    digitalWrite(LED_BUILTIN, HIGH);
}

void loop()
{
    wdt_reset();
    //HEARTBEAT
    sendHeartbeat();

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

void sendHeartbeat() {
    if (millis() - lastHeartbeatTime < heartbeatInterval)
        return;

    StaticJsonDocument<128> p;
    p["uptime"] = millis();
    p["free_ram"] = getFreeRam();

    sendJson("system_log", "heartbeat", "ok", p.as<JsonVariantConst>());

    lastHeartbeatTime = millis();
}

void checkPIR()
{
    byte motionValue = digitalRead(pirPin);
    if (motionValue == HIGH) {
        digitalWrite(pirLedPin, HIGH);

        if (pirState == LOW) {
            StaticJsonDocument<128> p;
            p["active"] = true;

            sendJson("alert", "pir", "ok", p.as<JsonVariantConst>());

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

        StaticJsonDocument<128> p;
        p["active"] = true;

        sendJson("alert", "flame", "ok", p.as<JsonVariantConst>());

        lastFlameTime = millis();
    }

    if (fireAlarmActive && (millis() - lastFlameTime >= fireAlarmDuration)) {
        if (digitalRead(flamePin) == LOW) {
            lastFlameTime = millis();
        }
        else {
            fireAlarmActive = false;
            flameDetected = false;
            digitalWrite(flameLedPin, LOW);
        }
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

    StaticJsonDocument<128> p;
    const char* status;

    if(failed)
    {
       status = "error";
        p["raw"] = nullptr;
        p["percentage"] = nullptr;
    }
    else 
    {
        status = "ok";
        p["raw"] = rawAverage;
        p["percentage"] = perAverage;
    }

    sendJson("telemetry", "soil_moisture", status, p.as<JsonVariantConst>());

    digitalWrite(soilPowerPin, LOW);
    lastMoistureTime = millis();
}

void checkClimate() {
    if (millis() - lastClimateTime < climateReadingInterval) 
        return;

    float humidity = dht.readHumidity();
    float tempC = dht.readTemperature();

    StaticJsonDocument<128> p;
    const char* status;

    if (isnan(humidity) || isnan(tempC)) {
        status = "error";
        p["temperatureC"] = nullptr;
        p["humidity"] = nullptr;
    }
    else 
    {
        status = "ok";
        p["temperatureC"] = tempC;
        p["humidity"] = humidity;
    }

    sendJson("telemetry", "climate", status, p.as<JsonVariantConst>());

    lastClimateTime = millis();
}

void sendJson(const char* type, const char* subject, const char* status, JsonVariantConst payload) {
    StaticJsonDocument<256> doc;
    doc["type"] = type;
    doc["node"] = nodeId;
    doc["subject"] = subject;
    doc["status"] = status;
    doc["payload"] = payload;

    serializeJson(doc, Serial);
    Serial.println();
}

const char* getResetReason() {
  if (mcusr_mirror == 0) return "cleared_by_bootloader";
  if (mcusr_mirror & (1 << WDRF))  return "watchdog"; //code issues
  if (mcusr_mirror & (1 << BORF))  return "brownout"; //voltage too low
  if (mcusr_mirror & (1 << EXTRF)) return "reset_button"; //physical reset button
  if (mcusr_mirror & (1 << PORF))  return "power_on";
  return "unknown";
}

int getFreeRam() {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}
