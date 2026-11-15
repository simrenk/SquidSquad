#include <ESP32Servo.h>
#include <OSCMessage.h>

struct Goo {
  Servo servo;
  int pin;
  int stopMicroSeconds; //this is what the continuous servo registers as its "stopping point" and distance from this is what affects speed               
  int screwInMicroSeconds; //how fast it screws in
  int screwOutMicroSeconds; //how fast it screws out

  void begin(int pin, int stopMicroSeconds, int screwInMicroSeconds, int screwOutMicroSeconds) {
    this->pin = pin;
    this->stopMicroSeconds = stopMicroSeconds;
    this->screwInMicroSeconds = screwInMicroSeconds;
    this->screwOutMicroSeconds = screwOutMicroSeconds;
    servo.setPeriodHertz(50);
    servo.attach(this->pin, 1000, 2000);
    servo.writeMicroseconds(this->stopMicroSeconds);
  }
};

struct Tentacles {
  int in1;
  int in2;
  int ena;
  int slowedSpeed;

  void begin(int in1, int in2, int ena, int slowedSpeed) {
    this->in1 = in1;
    this->in2 = in2;
    this->ena = ena;
    this->slowedSpeed = slowedSpeed;

    pinMode(this->in1, OUTPUT);
    pinMode(this->in2, OUTPUT);
    pinMode(this->ena, OUTPUT);
  }
};

Goo goo;
Tentacles tentacles;

const int PORT = 12000;              // Wek output port
const unsigned long HELD_FOR = 300;  // Hand gesture must hold at least this long to register

int incomingHandMovement = 0;
unsigned long incomingSince = 0;
int activeHandMovement = 0;

void setup() {
  Serial.begin(115200);
  pinMode(2, OUTPUT);
  goo.begin(32, 1500, 2000, 1200); //these values might be wrong
  tentacles.begin(25, 26, 27, 150); //these too
  Serial.println("Setup complete");
}

void gooEject(Goo &goo, int delayTime) {
  goo.servo.attach(goo.pin, 1000, 2000);
  goo.servo.writeMicroseconds(goo.screwInMicroSeconds);
  delay(delayTime);
  goo.servo.writeMicroseconds(goo.stopMicroSeconds);
  goo.servo.detach();
}

void gooReset(Goo &goo, int delayTime) {
  goo.servo.attach(goo.pin, 1000, 2000);
  goo.servo.writeMicroseconds(goo.screwOutMicroSeconds);
  delay(delayTime);
  goo.servo.writeMicroseconds(goo.stopMicroSeconds);
  goo.servo.detach();
}

void tentaclesWrithing(Tentacles &tentacles, int delayTime) {
  digitalWrite(tentacles.in1, HIGH); //current differential causes it to spin
  digitalWrite(tentacles.in2, LOW);

  analogWrite(tentacles.ena, 200);   //analogWrite so we can modulate the voltage w/ PWM. start w a bit more power so it starts up
  delay(100);
  analogWrite(tentacles.ena, tentacles.slowedSpeed); //pop down to our actual desired speed

  delay(delayTime);
  analogWrite(tentacles.ena, 0);
  digitalWrite(tentacles.in1, LOW); //same voltage on both inputs so there's no current
  digitalWrite(tentacles.in2, LOW);
}

void setIncomingHandMovement(int handMovement) {
  if (handMovement != incomingHandMovement) {
    incomingHandMovement = handMovement;
    incomingSince = millis();
  }
}

void onWekinator(OSCMessage &msg) {
  int handMovement = (int)round(msg.getFloat(0));
  Serial.printf("  handMovement received: %d\n", handMovement);
  setIncomingHandMovement((int)round(msg.getFloat(0)));
}

void respondToHandMovement(int handMovement) {
    switch (handMovement) {
      case 1: //flick, twitch leg
        //activate & deactivate dc motor
        Serial.println("Case 1");
        tentaclesWrithing(tentacles, 3000);
        break;
      case 2: //boo, short goo, big twitch
        //activate & deactivate servo
        Serial.println("Case 2");
        //gooEject(goo, 1800);
        gooEject(goo, 1800);
        //activate & deactivate dcmotor
        delay(300);
        tentaclesWrithing(tentacles, 3000);
        break;
      case 3: //squeeze, big goo
      //activate & deactivate servo
        Serial.println("Case 3");
        gooEject(goo, 2500);
        break;
      case 5: //for testing
        gooReset(goo, 7000);
        break;
      default:
        break;
  };
}

void loop() {
  //Proof of life for board (rather than it being shorted)
  static unsigned long lastPOL = 0;
  static bool led = false;
  if (millis() - lastPOL > 1000) {
    lastPOL = millis();
    led = !led; //switching value to create LED pulse
    digitalWrite(2, led);
    Serial.println("Alive");
  }

  //Manual testing
  if (Serial.available()) {
    char c = Serial.read();
    if (c >= '1' && c <= '5') setIncomingHandMovement(c - '0');
  }

  //Wekinator testing
  if ((incomingHandMovement != activeHandMovement) && (millis() - incomingSince > HELD_FOR)) {
    activeHandMovement = incomingHandMovement;
    Serial.printf("Gesture %d\n", activeHandMovement);
    respondToHandMovement(activeHandMovement);
  }
}
