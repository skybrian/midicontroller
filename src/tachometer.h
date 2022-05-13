#ifndef TACHOMETER_H
#define TACHOMETER_H

#include <Arduino.h>
#include <elapsedMillis.h>
#include "filters.h"

// Converts a noisy pulse wave signal to a frequency in hertz.
class Tachometer {

  // === Parameters.

  // The the time between voltage reads in microseconds.
  static constexpr int deltaT = 1000;

  static constexpr float minHertz = 0.2;

  // Amount of smoothing for the low-pass filter, removing high-frequency noise.
  static constexpr float maxHertz = 200;

  // Amount of smoothing for ambient lighting.
  static constexpr float maxAmbientHertz = 200;

  // Expected amount of voltage change above ambient lighting.
  static constexpr float pulseHeight = 1000.0;

  // When counting cycles to compute the frequency, how much time to average over.
  // In microseconds.
  static constexpr int cycleAveragePeriod = 200000;

  static constexpr int cyclePasses = 3;

  // == Low pass filters to smooth the ambient and lit voltages.
  LowPassFilter smoothAmbient1 = LowPassFilter(maxAmbientHertz, deltaT);
  LowPassFilter smoothAmbient2 = LowPassFilter(maxAmbientHertz, deltaT);
  LowPassFilter smoothV1 = LowPassFilter(maxHertz, deltaT);
  LowPassFilter smoothV2 = LowPassFilter(maxHertz, deltaT);

  HighPassFilter slopeV = HighPassFilter(maxHertz, deltaT);
  ZeroLevelTracker zeroV = ZeroLevelTracker(25.0, minHertz, deltaT, pulseHeight / 2);

  // An estimate of cycles seen within the previous cycleAveragePeriod.
  MultipassFilter<cycleAveragePeriod, deltaT> cycles;

  // === Finds when the pulse changed between low and high.

  bool pulseHigh;

  // Takes a voltage (v), centered on the midpoint of the signal.
  // Returns true if the voltage has changed between the high and low regions of the pulse.
  bool crossed(int v) {
    bool crossed = (pulseHigh && v < -0.1 * pulseHeight) || (!pulseHigh && v > 0.1 * pulseHeight);
    if (crossed) pulseHigh = v > 0;
    return crossed;
  }

  // The pin to turn the LED on.
  const int lightPin;

  // The analog pins to read the voltage from the phototransistors.
  const int readPin1;
  const int readPin2;

public:

  Tachometer(const int ledPin, const int sensorPin1, const int sensorPin2)
    : lightPin(ledPin), readPin1(sensorPin1), readPin2(sensorPin2) {}

  struct Reading {
    // The voltaage witht the LED turned off (smoothed).
    int ambientV1;
    int ambientV2;
    // The raw voltage signals, as returned by analogRead().
    // They should be 90 degrees out of phase.
    int rawV1;
    int rawV2;
    // The voltage with high frequencies removed and adjusted for ambient lighting.
    float smoothV1;
    float smoothV2;
    // The slope of smoothV.
    float slopeV;
    // Estimated midpoint of the pulses.
    float zeroV;
    // The voltage after converting to a binary signal.
    bool pulseHigh;
    // The estimated frequency.
    float frequency;
  };

private:

  elapsedMicros sincePoll;
  bool gotAmbient = false;
  int rawAmbientV1, rawAmbientV2;
  Reading lastRead;

public:

  // Reads the next value if enough time has elapsed. Returns true if a new reading is available.
  bool poll() {
    if (sincePoll < deltaT - 500) {
      return false;
    }

    if (!gotAmbient) {
      rawAmbientV1 = analogRead(readPin1);
      rawAmbientV2 = analogRead(readPin2);
      digitalWrite(lightPin, HIGH);
      gotAmbient = true;
      return false;
    }

    if (sincePoll < deltaT) {
      return false;
    }
    sincePoll = 0;

    int rawV1 = 0;
    int rawV2 = 0;
    for (int i = 0; i < 5; i++) {
      rawV1 += analogRead(readPin1);
      rawV2 += analogRead(readPin2);
      delayMicroseconds(20);
    }
    rawV1 = rawV1 / 5;
    rawV2 = rawV2 / 5;

    Reading result;
    result.ambientV1 = smoothAmbient1.update(rawAmbientV1);
    result.ambientV2 = smoothAmbient1.update(rawAmbientV2);
    result.rawV1 = rawV1;
    result.rawV2 = rawV2;
    digitalWrite(lightPin, LOW);

    result.smoothV1 = smoothV1.update(result.rawV1 - result.ambientV1);
    result.smoothV2 = smoothV2.update(result.rawV2 - result.ambientV2);
    result.slopeV = slopeV.update(result.smoothV1);
    result.zeroV = zeroV.update(result.smoothV1, result.slopeV);

    float progress = crossed(result.smoothV1 - result.zeroV) ? 0.5 : 0;
    result.frequency = cycles.update(progress) * 1000000 / deltaT;
    result.pulseHigh = pulseHigh;

    lastRead = result;
    gotAmbient = false;
    return true;
  }

  // Returns the most recently read value.
  Reading read() {
    return lastRead;
  }

  // Clears the filters and waits until we get a pulse.
  void begin() {
    // pinMode(readPin1,INPUT);
    // pinMode(readPin2,INPUT);
    pinMode(lightPin, OUTPUT);
    digitalWrite(lightPin, LOW);
    delay(1);
    float v1 = analogRead(readPin1);
    float v2 = analogRead(readPin2);
    smoothAmbient1.reset(v1);
    smoothAmbient2.reset(v2);
    smoothV1.reset(v1);
    smoothV2.reset(v2);
    zeroV.reset();
    cycles.reset();
    pulseHigh = false;

    // Do a few reads to get started.
    for (int i = 0; i < 10; i++) {
      while (!poll()) {
        // busy wait. It doesn't look like the Pico has sleep.
      }
    }
  }
}; // end tachometer

#endif // TACHOMETER_H
