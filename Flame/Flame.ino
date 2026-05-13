const int flamePin = 2;
const int ledPin = 13;

volatile bool flameDetected = false;

void setup() {
  pinMode(flamePin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  
  Serial.begin(9600);

  attachInterrupt(digitalPinToInterrupt(flamePin), fireISR, FALLING);
  
  Serial.println("Monitoring for flames");
}

void loop() {
  if (flameDetected) {
    Serial.println("!!! FIRE DETECTED !!!");
    digitalWrite(ledPin, HIGH);
    
    delay(1000); 
    flameDetected = false;
    digitalWrite(ledPin, LOW);
  }
}

void fireISR() {
  flameDetected = true;
}