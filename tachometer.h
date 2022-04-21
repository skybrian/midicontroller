#ifndef TACHOMETER_H
#define TACHOMETER_H

#include <Arduino.h>
#include <elapsedMillis.h>
#include "filters.h"

// Converts a noisy pulse wave signal to a frequency in hertz.
class Tachometer {

  // === Parameters.

  // The the time between voltage reads in microseconds.
  const int deltaT = 200;

  const float minHertz = 0.2;

  // Amount of smoothing for the low-pass filter, removing high-frequency noise.
  const float maxHertz = 200;

  // Amount of smoothing for ambient lighting.
  const float maxAmbientHertz = 2;

  // Expected amount of voltage change above ambient lighting.
  const float pulseHeight = 750.0;

  // When counting ticks to compute the frequency, how much time to average over.
  // In seconds.
  const float tickAveragePeriod = .250;

  // == Low pass filters to smooth the ambient and lit voltages.
  LowPassFilter smoothAmbient = LowPassFilter(maxAmbientHertz, deltaT);
  LowPassFilter smoothV = LowPassFilter(maxHertz, deltaT);

  HighPassFilter slopeV = HighPassFilter(maxHertz, deltaT);
  ZeroLevelTracker zeroV = ZeroLevelTracker(25.0, minHertz, deltaT, pulseHeight / 2);

  // === Finds when the pulse changed between low and high.

  bool pulseHigh;

  // Takes a voltage (v), centered on the midpoint of the signal.
  // Returns true if the voltage has changed between the high and low regions of the pulse.
  bool crossed(int v) {
    bool crossed = (pulseHigh && v < -0.1 * pulseHeight) || (!pulseHigh && v > 0.1 * pulseHeight);
    if (crossed) pulseHigh = v > 0;
    return crossed;
  }

  // === Finds the pulse frequency by adjusting an estimate after each crossing.

  const float minPeriod = 1000000.0 / maxHertz;
  const float maxPeriod = 1000000.0 / minHertz;

  // An estimate of ticks seen within the previous tickAveragePeriod.
  float ticks = 0;

  // Milliseconds between the previous two ticks.
  float prevPeriod = maxPeriod;

  // Milliseconds since the previous tick.
  int sinceTick = 0;

  // Amount of the next tick to be added.
  float nextTickFraction = 0;

  // The period over which to add the next tick.
  float nextTickTime = 0;

  // Takes a flag for whether a tick was seen (sawTick) and microseconds since the previous read (deltaT).
  // Returns the new estimated frequency in hertz.
  float findFrequency(bool sawTick) {
    sinceTick += deltaT;

    ticks -= (ticks / tickAveragePeriod) * deltaT * 0.000001;

    if (nextTickTime > 0) {
      float fractionTime = nextTickTime > deltaT ? deltaT : nextTickTime;
      float fraction = nextTickFraction * (fractionTime / nextTickTime);
      ticks += fraction;
      nextTickFraction -= fraction;
      nextTickTime -= fractionTime;
    }

    if (sawTick) {
      nextTickFraction += 1;
      float oldPeriod = prevPeriod;
      prevPeriod = sinceTick;
      // Estimate based on constant speed and two ticks per cycle, possibly of uneven length.
      nextTickTime = oldPeriod;
      sinceTick = 0; 
    }

    return ticks / tickAveragePeriod;
  }

  // The pin to turn the LED on.
  const int lightPin;

  // The analog pin to read the voltage from the phototransistor.
  const int readPin;

public:

  Tachometer(const int ledPin, const int sensorPin) : lightPin(ledPin), readPin(sensorPin) {}

  struct Reading {
    // The voltaage witht the LED turned off (smoothed).
    int ambientV;
    // The raw voltage signal, as returned by analogRead().
    int rawV;
    // The voltage with high frequencies removed and adjusted for ambient lighting.
    float smoothV;
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
  int rawAmbientV;
  Reading lastRead;

public:

  // Reads the next value if enough time has elapsed. Returns true if a new reading is available.
  bool poll() {
    if (sincePoll < deltaT - 100) {
      return false;
    }

    if (!gotAmbient) {
      rawAmbientV = analogRead(readPin);
      digitalWrite(lightPin, HIGH);
      gotAmbient = true;
      return false;
    }

    if (sincePoll < deltaT) {
      return false;
    }

    Reading result;
    result.ambientV = smoothAmbient.update(rawAmbientV);
    result.rawV = analogRead(readPin);
    sincePoll = 0;
    digitalWrite(lightPin, LOW);

    result.smoothV = smoothV.update(result.rawV - result.ambientV);
    result.slopeV = slopeV.update(result.smoothV);
    result.zeroV = zeroV.update(result.smoothV, result.slopeV);
    
    result.frequency = findFrequency(crossed(result.smoothV - result.zeroV));
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
    pinMode(lightPin, OUTPUT);
    digitalWrite(lightPin, LOW);
    smoothAmbient.reset();
    smoothV.reset();
    zeroV.reset();
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
