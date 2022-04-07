#ifndef TACHOMETER_H
#define TACHOMETER_H

#include <Arduino.h>
#include <elapsedMillis.h>
#include "filters.h"

// Converts a noisy pulse wave signal to a frequency in hertz.
class Tachometer {

  // === Parameters.

  const float minHertz = 0.2;

  // Amount of smoothing for the low-pass filter, removing high-frequency noise.
  const float maxHertz = 100;

  // Amount of smoothing for ambient lighting.
  const float maxAmbientHertz = 2;

  // Expected amount of voltage change above ambient lighting.
  const float pulseHeight = 500.0;

  // When counting ticks to compute the frequency, how much time to average over.
  // In milliseconds.
  const float tickAveragePeriod = 250;

  // == Low pass filters to smooth the ambient and lit voltages.
  LowPassFilter smoothAmbient = LowPassFilter(maxAmbientHertz);
  LowPassFilter smoothV = LowPassFilter(maxHertz);

  HighPassFilter slopeV = HighPassFilter(maxHertz);
  ZeroLevelTracker zeroV = ZeroLevelTracker(25.0, minHertz, 0.5 * pulseHeight);

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

  const float minPeriod = 1000.0 / maxHertz;
  const float maxPeriod = 1000.0 / minHertz;

  // A count of ticks seen within the previous tickAveragePeriod. (Estimate.)
  float ticks = 0;

  // Milliseconds between the previous two ticks.
  float prevPeriod = maxPeriod;

  // Milliseconds since the previous tick.
  int sinceTick = 0;

  // Amount of the next tick to be added.
  float nextTickFraction = 0;

  // The period over which to add the next tick.
  float nextTickTime = 0;

  // Takes a flag for whether a tick was seen (sawTick) and milliseconds since the previous read (deltaT).
  // Returns the new estimated frequency.
  float findFrequency(bool sawTick, int deltaT) {
    sinceTick += deltaT;

    ticks -= (ticks / tickAveragePeriod) * deltaT;

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

    return 1000.0 * ticks / tickAveragePeriod;
  }

  // The pin to turn the LED on.
  const int lightPin;

  // The analog pin to read the voltage from the phototransistor.
  const int readPin;

  elapsedMillis sinceReadV, sinceReadAmbient;

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
 
  // Reads the current pulse frequency.
  // If not called frequently enough, this will miss pulses and undercount. 
  Reading read() {
    Reading result;

    float rawAmbientV = analogRead(readPin);
    int deltaTAmbient = sinceReadAmbient;
    sinceReadAmbient = 0;
    result.ambientV = smoothAmbient.update(rawAmbientV, deltaTAmbient);
    
    digitalWrite(lightPin, HIGH);
    delayMicroseconds(100);

    result.rawV = analogRead(readPin);
    int deltaT = sinceReadV;
    sinceReadV = 0;

    digitalWrite(lightPin, LOW);

    result.smoothV = smoothV.update(result.rawV - result.ambientV, deltaT);
    result.slopeV = slopeV.update(result.smoothV, deltaT);
    result.zeroV = zeroV.update(result.smoothV, deltaT, result.slopeV);
    
    result.frequency = findFrequency(crossed(result.smoothV - result.zeroV), deltaT);
    result.pulseHigh = pulseHigh;
    
    return result;
  }

  // Clears the filters and waits until we get a pulse.
  void begin() {
    pinMode(lightPin, OUTPUT);
    digitalWrite(lightPin, LOW);
    smoothAmbient.reset();
    smoothV.reset();
    zeroV.reset();
    pulseHigh = false;

    // Wait for the start of the first pulse.
    while (true) {
      Reading val = read();
      if (val.rawV > 0.5 * pulseHeight) {
        return;
      }
      delay(1);
    }
  }
  
}; // end tachometer

#endif // TACHOMETER_H
