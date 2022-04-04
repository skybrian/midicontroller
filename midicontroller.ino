#include <Adafruit_TinyUSB.h>
#include <elapsedMillis.h>

#include <MIDI.h>

// Converts a noisy pulse wave signal to a frequency in hertz.
namespace wheel {

  // === Parameters.

  // Time in seconds for calibration.
  const float calibrationPeriod = 1000;

  // We use a high-pass-filter to center the signal around zero.
  const float minHertz = 0.2;
  const float defaultOffset = 2000;

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

  float averageV = 0;

  // Takes the latest voltage (v) and milliseconds since previous read (deltaT).
  // Returns the filtered voltage.
  // If deltaT is too high, no filtering will be done.
  float lowPass(int v, int deltaT) {
    float weight = deltaT / (1000.0 / maxHertz);
    if (weight > 1.0) weight = 1.0;
    averageV = (1.0 - weight) * averageV + weight * v;
    return averageV;
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
  
  // The last reading of the raw voltage signal.
  int rawV;
  elapsedMillis sinceRead;
  
  // Reads the current pulse frequency.
  // If not called frequently enough, this will miss pulses and undercount. 
  float read() {
    rawV = analogRead(A0);
    int deltaT = sinceRead;
    sinceRead = 0;

    float v = highPass((float)rawV, deltaT);
    v = lowPass(v, deltaT);
    return findFrequency(crossed(v), deltaT);
  }

  // Clears the filters and reads enough data to get them going again.
  void calibrate() {  
    float v = analogRead(A0);
    offset = 0;
    averageV = v - defaultOffset;
    pulseHigh = averageV>0;
        
    delay(1);
    
    elapsedMillis sinceStart;
    while (sinceStart < calibrationPeriod) {
      read();
      delay(1);
    }
  }
  
} // end wheel

Adafruit_USBD_MIDI midiDev;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, midiDev, MID);

midi::DataByte prevControlValue = -1;

void sendControlChange(int value) {
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

void setup() {
  MID.begin();
  
  Serial.begin(115200);
  while (!Serial) delay(1);
  
  
  Serial.print("\nCalibrating...");
  Serial.flush();
  wheel::calibrate();

  while( !USBDevice.mounted() ) delay(1);
  sendControlChange(0); 
  Serial.println("Ready.");
}

elapsedMillis sincePrint;

void loop() {
  MID.read();

  float f = wheel::read();
  sendControlChange((int)f);
  
  if (sincePrint >= 25) {
    sincePrint = 0;
    //Serial.print(wheel::rawV); Serial.print(", ");
    //Serial.print(wheel::averageV / 10.0); Serial.print(", ");
    Serial.print(wheel::pulseHigh * -10.0); Serial.print(", ");
    Serial.println(f);
  }
  delay(1);
}
