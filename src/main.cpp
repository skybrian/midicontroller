#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <elapsedMillis.h>
#include <math.h>

#include "sensor.h"
#include "calibration.h"

struct LapMetrics {
  double theta; // in degrees of phase; 360 degrees = 1 grating
  double delta; // in degrees
  double laps;
};

int laps = -1;
double prevTheta = nan("");

LapMetrics calculateLaps(sensor::Reading reading) {
    LapMetrics lm;
    lm.theta = atan2(reading.b - 300, reading.a - 300) * 360.0 / (2 * PI);
    double delta = lm.theta - prevTheta;
    prevTheta = lm.theta;
    if (delta < -180.0) {
      laps++;
      delta += 360.0;
    } else if (delta > 180.0) {
      laps--;
      delta -= 360.0;
    }
    lm.delta = delta;
    lm.laps = laps + lm.theta / 360.0;
    return lm;
}

void printHeader() {
  Serial.println("Laps,Delta,Theta,WeightUpdates,Bin,Weight,A,B,Jitter,A Read Time,B Read Time,Total Read Time");
}

void printLine(LapMetrics lm, calibration::WeightMetrics wm, sensor::Reading r) {
  Serial.print(lm.laps, 4); Serial.print(", ");
  Serial.print(lm.delta); Serial.print(", ");
  Serial.print(lm.theta); Serial.print(", ");

  Serial.print(wm.updateCount); Serial.print(", ");
  Serial.print(wm.bin); Serial.print(", ");
  Serial.print(wm.binValue, 4); Serial.print(", ");

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
    LapMetrics lm = calculateLaps(reading);
    calibration::WeightMetrics wm = calibration::calculateWeights(lm.laps);
    printLine(lm, wm, reading);
  }
}

void loop1() {
  delay(100);
  sensor::runReadLoop();
}