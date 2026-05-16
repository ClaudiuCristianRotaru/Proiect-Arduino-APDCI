const int pirPin = 3;
const int ledPin = 13;

int pirState = LOW;
int motionValue = 0;

void setup() {
  pinMode(pirPin, INPUT);
  pinMode(ledPin, OUTPUT);
  
  Serial.begin(9600);
  Serial.println("PIR Sensor Warming Up...");

  delay(30000); 
  Serial.println("Sensor Ready!");
}

void loop() {
  motionValue = digitalRead(pirPin);
  
  if (motionValue == HIGH) {
    digitalWrite(ledPin, HIGH);
    
    if (pirState == LOW) {
      Serial.println("Motion detected!");
      pirState = HIGH;
    }
  } 
  else {
    digitalWrite(ledPin, LOW);
    
    if (pirState == HIGH) {
      Serial.println("Motion ended.");
      pirState = LOW;
    }
  }
}