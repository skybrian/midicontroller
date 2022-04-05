#ifndef TACHOMETER_H
#define TACHOMETER_H

#include <Arduino.h>
#include <elapsedMillis.h>

// Converts a noisy pulse wave signal to a frequency in hertz.
class Tachometer {

  // === Parameters.

  // We use a high-pass-filter to center the signal around zero.
  const float minHertz = 0.2;

  // The low-pass filter smooths the signal, removing high-frequency noise.
  const float maxHertz = 100;

  // A pulse should be at least this high (peak to peak) to count as a tick.
  const float minPulseHeight = 200.0;

  // When counting ticks to compute the frequency, how much time to average over.
  // In milliseconds.
  const float tickAveragePeriod = 250;

  // === The high-pass filter centers the signal around 0, using a running average.
  
  float offset = 0;

  // Takes the latest voltage (v) and milliseconds since the previous read (deltaT).
  // Returns the voltage, subtracting an offset so that zero is in the middle.
  // If deltaT is too high, returns 0.
  float highPass(int v, int deltaT) {
    float weight = deltaT / (1000.0 / minHertz);
    if (weight > 1.0) weight = 1.0;
    offset = (1.0 - weight) * offset + weight * v;
    return v - offset;
  }

  // === The low-pass filter smooths the signal using a running average.

  float smoothV = 0;

  // Takes the latest voltage (v) and milliseconds since previous read (deltaT).
  // Returns the filtered voltage.
  // If deltaT is too high, no filtering will be done.
  float lowPass(int v, int deltaT) {
    float weight = deltaT / (1000.0 / maxHertz);
    if (weight > 1.0) weight = 1.0;
    smoothV = (1.0 - weight) * smoothV + weight * v;
    return smoothV;
  }

  // === Finds when the pulse changed between low and high.

  bool pulseHigh;

  // Takes a voltage (v).
  // Returns true if the voltage has changed between the high, middle, and low regions of the pulse.
  bool crossed(int v) {
    bool crossed = (pulseHigh && v < -minPulseHeight / 2) || (!pulseHigh && v > minPulseHeight / 2);
    if (crossed) pulseHigh = v>0;
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
  int lightPin;

  // The analog pin to read the voltage from the phototransistor.
  int readPin;

  elapsedMillis sinceRead;

public:

  Tachometer(int ledPin, int sensorPin) : lightPin(ledPin), readPin(sensorPin) {}

  struct Reading {
    // The raw voltage signal, as returned by analogRead().
    int rawV;
    // The raw voltage with the light turned off.
    int ambientV;
    // The voltage after smoothing and centering around zero.
    float smoothV;
    // The voltage after converting to a binary signal.
    bool pulseHigh;
    // The estimated frequency.
    float frequency;
  };
 
  // Reads the current pulse frequency.
  // If not called frequently enough, this will miss pulses and undercount. 
  Reading read() {
    Reading result;

    result.ambientV = analogRead(readPin);
    digitalWrite(lightPin, HIGH);
    delayMicroseconds(100);
    result.rawV = analogRead(readPin);
    digitalWrite(lightPin, LOW);

    int deltaT = sinceRead;
    sinceRead = 0;

    float centeredV = highPass((float)(result.rawV - result.ambientV), deltaT);
    result.smoothV = lowPass(centeredV, deltaT);

    result.frequency = findFrequency(crossed(smoothV), deltaT);
    result.pulseHigh = pulseHigh;
    
    return result;
  }

  // Clears the filters and waits until we get a pulse.
  void begin() {
    pinMode(lightPin, OUTPUT);
    digitalWrite(lightPin, LOW);
    delay(1);
    offset = analogRead(readPin);
    smoothV = 0;
    pulseHigh = false;
    
    while (true) {
      Reading val = read();
      if (val.rawV > minPulseHeight) {
        return;
      }
      delay(1);
    }
  }
  
}; // end tachometer

#endif // TACHOMETER_H
