#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>

#include <elapsedMillis.h>
#include <math.h>

#include "sensor.h"
#include "calibration.h"
#include "filters.h"

Adafruit_USBD_MIDI midiDev;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, midiDev, MID);

midi::DataByte prevControlValue = -1;

void __not_in_flash_func(sendControlChange)(int value) {
  if (value < 0) value = 0;
  if (value > 127) value = 127;
  midi::DataByte val = value;
  if (val == prevControlValue) {
    return;
  }
  //Serial.println(val);
  MID.sendControlChange(1, val, 1);
  prevControlValue = val;
}

struct LapMetrics {
  float laps;
  float adjustedLaps;
  float adjustedDelta;
  float smoothDelta;
  int midiVelocity;
};

float prevAdjustedLaps = nanf("");
MovingAverageFilter<9> smoothDelta;

LapMetrics __not_in_flash_func(calculateLaps)(sensor::Reading reading) {
    LapMetrics lm;
    lm.laps = reading.laps + reading.theta / ((float)sensor::ticksPerTurn);

    lm.adjustedLaps = calibration::adjustLaps(lm.laps);
    lm.adjustedDelta = (lm.adjustedLaps - prevAdjustedLaps) * 360.0;
    prevAdjustedLaps = lm.adjustedLaps;

    lm.smoothDelta = smoothDelta.update(lm.adjustedDelta);
    lm.midiVelocity = floor(abs(lm.smoothDelta) * 0.5);
    return lm;
}

void printHeader() {
  Serial.println("\nMIDIVelocity,SmoothDelta,AdjustedDelta,AdjustedLaps,Laps,WeightUpdates,Bin,binWeight,binAdjustment,a,b,theta,thetaChange,idle,jitter,aReadTime,bReadTime,totalReadTime");
}

void __not_in_flash_func(printLine)(LapMetrics lm, calibration::WeightMetrics wm, sensor::Reading r) {
  Serial.print(lm.midiVelocity); Serial.print(", ");
  Serial.print(lm.smoothDelta); Serial.print(", ");
  Serial.print(lm.adjustedDelta); Serial.print(", ");
  Serial.print(lm.adjustedLaps, 4); Serial.print(", ");
  Serial.print(lm.laps, 4); Serial.print(", ");

  Serial.print(wm.updateCount); Serial.print(", ");
  Serial.print(wm.bin); Serial.print(", ");
  Serial.print(wm.binWeight, 4); Serial.print(", ");
  Serial.print(wm.binAdjustment, 4); Serial.print(", ");

  Serial.print(r.a); Serial.print(", ");
  Serial.print(r.b); Serial.print(", ");
  Serial.print(r.theta * 360.0 / sensor::ticksPerTurn); Serial.print(", ");
  Serial.print(r.thetaChange * 360.0 / sensor::ticksPerTurn); Serial.print(", ");
  Serial.print(r.idle); Serial.print(", ");
  Serial.print(r.jitter); Serial.print(", ");
  Serial.print(r.aReadTime); Serial.print(", ");
  Serial.print(r.bReadTime); Serial.print(", ");
  Serial.println(r.totalReadTime);
  Serial.flush();
}

void setup() {
  MID.begin();
  sensor::begin();
}

void loop() {
  while (!Serial.dtr()) {
    sensor::Reading reading = sensor::next();
    LapMetrics lm = calculateLaps(reading);
    sendControlChange(lm.midiVelocity);
    calibration::adjustWeights(lm.laps);
  }

  printHeader();
  while (Serial.dtr()) {
    sensor::Reading reading = sensor::next();
    LapMetrics lm = calculateLaps(reading);
    sendControlChange(lm.midiVelocity);
    calibration::WeightMetrics wm = calibration::adjustWeights(lm.laps);
    printLine(lm, wm, reading);
  }
}

void loop1() {
  delay(100);
  sensor::runReadLoop();
}
