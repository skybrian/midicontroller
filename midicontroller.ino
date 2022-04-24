#include <Adafruit_TinyUSB.h>
#include <elapsedMillis.h>

#include <MIDI.h>

#include "tachometer.h"

Adafruit_USBD_MIDI midiDev;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, midiDev, MID);

midi::DataByte prevControlValue = -1;

Tachometer wheel(22, A0);

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

bool logEnabled = false;

void setup() {
  MID.begin();
  
  Serial.begin(115200);
  if (Serial.dtr()) {
    Serial.print("\nCalibrating...");
    Serial.flush();
  }
  wheel.begin();

  while( !USBDevice.mounted() ) delay(1);
  sendControlChange(0);
  if (Serial.dtr()) {
    Serial.println("Ready.");
    printHeader();
  } else {
    logEnabled = false;
  }
}

elapsedMillis sincePrint;
elapsedMillis flushTime;

void printHeader() {
    Serial.println("PulseHigh,Frequency");
}

void loop() {
  if (!wheel.poll()) {
    return; // Busy wait until next read.
  }

  MID.read();

  Tachometer::Reading vals = wheel.read();
  sendControlChange((int)vals.frequency * 2);

  if (sincePrint < 25) return;
  sincePrint = 0;

  if (!Serial.dtr()) {
    logEnabled = false;
    return;
  }

  if (!logEnabled) {
    printHeader();
    logEnabled = true;
  }

//    Serial.print(vals.ambientV); Serial.print(", ");
//    Serial.print(vals.rawV); Serial.print(", ");
//    Serial.print(vals.smoothV); Serial.print(", ");
//    Serial.print(vals.slopeV); Serial.print(", ");
//    Serial.print(vals.zeroV); Serial.print(", ");
  Serial.print(vals.pulseHigh * -10.0); Serial.print(", ");
  Serial.println(vals.frequency);
  Serial.flush();
}
