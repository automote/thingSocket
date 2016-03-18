const int buttonPin = 2;     // the number of the pushbutton pin
const int ledPin =  13;      // the number of the LED pin

// variables will change:

volatile int num = 0;

void setup() {
  Serial.begin(9600);
  // initialize the LED pin as an output:
  pinMode(ledPin, OUTPUT);
  // initialize the pushbutton pin as an input:
  pinMode(buttonPin, INPUT_PULLUP);
  // Attach an interrupt to the ISR vector
  attachInterrupt(digitalPinToInterrupt(buttonPin), pin_ISR, CHANGE);
}

void loop() {
  // Nothing here!
}

void pin_ISR() {
  noInterrupts();
  String s;
  s += "ISR Called ";
  s += String(num);
  Serial.println(s);
  num++;
  myDelay(250);
  volatile int buttonState = digitalRead(buttonPin);
  digitalWrite(ledPin, buttonState);
  //myDelay(1000);
  interrupts();
}

void myDelay(int x)   {
  for(int i=0; i<=x; i++)   
  {
    delayMicroseconds(1000);
  }
}
