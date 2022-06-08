#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <elapsedMillis.h>
#include <math.h>

#include "sensor.h"

struct Calculations {
  float atan2;
};

void printHeader() {
  Serial.println("ATan2,A,B,Jitter,A Read Time,B Read Time,Total Read Time");
}

void printLine(Calculations c, sensor::Reading r) {
  Serial.print(c.atan2); Serial.print(", ");

  Serial.print(r.a); Serial.print(", ");
  Serial.print(r.b); Serial.print(", ");
  Serial.print(r.jitter); Serial.print(", ");
  Serial.print(r.aReadTime); Serial.print(", ");
  Serial.print(r.bReadTime); Serial.print(", ");
  Serial.println(r.totalReadTime);
  Serial.flush();
}

void setup() {}

void loop() {
  while (!Serial.dtr()) {
    sensor::next();
  }

  printHeader();
  while (Serial.dtr()) {
    sensor::Reading reading = sensor::next();
    Calculations c;
    c.atan2 = atan2(reading.b - 300, reading.a - 300);
    printLine(c, reading);
  }
}

void loop1() {
  delay(100);
  sensor::runReadLoop();
}
