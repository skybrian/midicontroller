#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <elapsedMillis.h>
#include <math.h>

#include "sensor.h"

struct Calculations {
  double theta; // in degrees of phase; 360 degrees = 1 grating
  double delta; // in degrees
  double laps;
};

int laps = -1;
double prevTheta = nan("");

Calculations calculate(sensor::Reading reading) {
    Calculations c;
    c.theta = atan2(reading.b - 300, reading.a - 300) * 360.0 / (2 * PI);
    double delta = c.theta - prevTheta;
    prevTheta = c.theta;
    if (delta < -180.0) {
      laps++;
      delta += 360.0;
    } else if (delta > 180.0) {
      laps--;
      delta -= 360.0;
    }
    c.delta = delta;
    c.laps = ((double)laps) + c.theta / 360.0;
    return c;
}

void printHeader() {
  Serial.println("Laps,Delta,Theta,A,B,Jitter,A Read Time,B Read Time,Total Read Time");
}

void printLine(Calculations c, sensor::Reading r) {
  Serial.print(c.laps, 4); Serial.print(", ");
  Serial.print(c.delta); Serial.print(", ");
  Serial.print(c.theta); Serial.print(", ");

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
    printLine(calculate(reading), reading);
  }
}

void loop1() {
  delay(100);
  sensor::runReadLoop();
}
