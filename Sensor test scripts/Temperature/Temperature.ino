#include "DHT.h"

#define DHTPIN 7    
#define DHTTYPE DHT11   

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(9600);
  Serial.println("DHT11 Sensor Test!");

  dht.begin();
}

void loop() {
  delay(2000);

  float humidity = dht.readHumidity();
  float tempC = dht.readTemperature();
  float tempF = dht.readTemperature(true);

  if (isnan(humidity) || isnan(tempC) || isnan(tempF)) {
    Serial.println("Failed to read from DHT sensor! Check wiring.");
    return;
  }

  float heatIndexF = dht.computeHeatIndex(tempF, humidity);
  float heatIndexC = dht.computeHeatIndex(tempC, humidity, false);

  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.print("%  |  ");
  
  Serial.print("Temperature: ");
  Serial.print(tempC);
  Serial.print("°C / ");
  Serial.print(tempF);
  Serial.print("°F  |  ");
  
  Serial.print("Heat index: ");
  Serial.print(heatIndexC);
  Serial.print("°C / ");
  Serial.print(heatIndexF);
  Serial.println("°F");
}