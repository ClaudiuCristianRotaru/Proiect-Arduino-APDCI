const int moisturePin = A0; 

int rawValue = 0;
int moisturePercentage = 0;

void setup() {
  Serial.begin(9600);
  Serial.println("Soil Moisture Sensor Starting...");
}

void loop() {
  rawValue = analogRead(moisturePin);

  moisturePercentage = map(rawValue, 1023, 300, 0, 100);
  
  moisturePercentage = constrain(moisturePercentage, 0, 100);

  Serial.print("Raw Sensor Value: ");
  Serial.print(rawValue);
  Serial.print("  |  Moisture Level: ");
  Serial.print(moisturePercentage);
  Serial.println("%");

  delay(1000); 
}