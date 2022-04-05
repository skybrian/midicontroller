#ifndef TACHOMETER_H
#define TACHOMETER_H

#include <Arduino.h>
#include <elapsedMillis.h>

// Converts a noisy pulse wave signal to a frequency in hertz.
class tachometer {

  // === Parameters.

  // Time in seconds for calibration.
  const float calibrationPeriod = 1000;

  // We use a high-pass-filter to center the signal around zero.
  const float minHertz = 0.2;
  const float defaultOffset = 500;

  // The low-pass filter smooths the signal, removing high-frequency noise.
  const float maxHertz = 100;

  // A pulse should be at least this high (peak to peak) to count as a tick.
  const float minPulseHeight = 200.0;

  // When counting ticks to compute the frequency, how much time to average over.
  // In milliseconds.
  const float tickAveragePeriod = 250;

  // === The high-pass filter finds the center of the signal using a running average.
  
  float offset = 0;

  // Takes the latest voltage (v) and milliseconds since the previous read (deltaT).
  // Returns the voltage, subtracting an offset so that zero is in the middle.
  // If deltaT is too high, the default offset will be used.
  float highPass(int v, int deltaT) {
    float weight = deltaT / (1000.0 / minHertz);
    if (weight > 1.0) weight = 1.0;
    offset = (1.0 - weight) * offset + weight * (v - defaultOffset);
    return v - offset - defaultOffset;
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
  
  elapsedMillis sinceRead;

public:

  struct reading {
    // The last reading of the raw voltage signal, as returned by analogRead().
    int rawV;
    // The voltage after smoothing and centering around zero.
    float smoothV;
    // The voltage after converting to a binary signal.
    bool pulseHigh;
    // The estimated frequency.
    float frequency;
  };
 
  // Reads the current pulse frequency.
  // If not called frequently enough, this will miss pulses and undercount. 
  reading read() {
    int rawV = analogRead(A0);
    int deltaT = sinceRead;
    sinceRead = 0;

    float centeredV = highPass((float)rawV, deltaT);
    float smoothV = lowPass(centeredV, deltaT);
    float f = findFrequency(crossed(smoothV), deltaT);

    reading result;

    result.rawV = rawV;
    result.smoothV = smoothV;
    result.pulseHigh = pulseHigh;
    result.frequency = f;
    
    return result;
  }

  // Clears the filters and reads enough data to get them going again.
  void calibrate() {  
    float v = analogRead(A0);
    offset = 0;
    smoothV = v - defaultOffset;
    pulseHigh = smoothV>0;
        
    delay(1);
    
    elapsedMillis sinceStart;
    while (sinceStart < calibrationPeriod) {
      read();
      delay(1);
    }
  }
  
}; // end tachometer

#endif // TACHOMETER_H
