#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>

#include <elapsedMillis.h>
#include <math.h>

#include "pins.h"

#include "sensor.h"
#include "calibration.h"

#include "bassboard.h"
#include "bassmaps.h"
#include "midi_out.h"

const int boardCount = 2;

struct LapMetrics {
  float laps;
  float adjustedLaps;
  float adjustedDelta;
  float airflow;
  int midiValue;
};

const float maxLeakage = 0.01;
const float pressureDecay = 0.96;

float pressure = 0;
float prevAdjustedLaps = nanf("");

float __not_in_flash_func(bellowsResponse)(float x) {
  if (x >= 12) return 127;
  x -= 12;
  return 127-0.7*(x*x)+2*x;
}

LapMetrics __not_in_flash_func(calculateLaps)(sensor::Reading reading) {
    LapMetrics lm;
    lm.laps = reading.laps + reading.theta / ((float)sensor::ticksPerTurn);

    lm.adjustedLaps = calibration::adjustLaps(lm.laps);
    float lapsChange = lm.adjustedLaps - prevAdjustedLaps;
    lm.adjustedDelta = lapsChange * 360.0;
    prevAdjustedLaps = lm.adjustedLaps;

    if (lapsChange > -1 && lapsChange < 1) {
      pressure += lapsChange;
    }

    if (fabs(pressure) < maxLeakage) {
      pressure = 0;
    } else {
      pressure += (pressure > 0) ? -maxLeakage : maxLeakage;
    }

    pressure *= pressureDecay;
    lm.airflow = fabs(pressure);

    lm.midiValue = calibration::calibrated() ? floor(bellowsResponse(fabs(lm.airflow))) : 0;
    if (lm.midiValue > 127) {
      lm.midiValue = 127;
    }
    return lm;
}

struct BassReadings {
  bassboard::Reading reading[boardCount];
  music::Chord chord;
  music::Chord bass;
};

midi::DataByte chordVelocity(music::Note n) {
  return 99;
}

midi::DataByte bassVelocity(music::Note n) {
  // Fade out the low and high end of the two octave range starting with bottomBass.
  // Assumes that bass notes are actually part of an octave interval in that range.
  // This makes jumping up or down an octave smoother.
  // See: https://www.accordionists.info/threads/shepard-tone-illusion.5295/
  int bottomFade = (bassmaps::bottomBass + 5) - n;
  if (bottomFade > 0) {
    return 100 - bottomFade * 18;
  }
  int topFade = n - (bassmaps::bottomBass + 24 + 7);
  if (topFade > 0) {
    return 100 - topFade * 18;
  }
  return 100;
}

midiOut::Channel<chordVelocity> trebleChannel(1);
midiOut::Channel<chordVelocity> chordChannel(2);
midiOut::Channel<bassVelocity> bassChannel(3);

const int bellowsControl = 1; // mod wheel

void printHeader() {
  Serial.println("\nMIDIValue,Airflow,AdjustedDelta,AdjustedLaps,Laps,WeightUpdates,Bin,binWeight,binAdjustment,a,b,theta,thetaChange,"
      "chordNotesOn,bassNotesOn,"
      "jitter,aReadTime,bReadTime,totalReadTime,maxJitter,minIdle,sendTime");
  Serial.flush();
}

void __not_in_flash_func(printLine)(LapMetrics lm, calibration::WeightMetrics wm, sensor::Report r, BassReadings& br) {
  Serial.print(lm.midiValue); Serial.print(", ");
  Serial.print(lm.airflow, 4); Serial.print(", ");
  Serial.print(lm.adjustedDelta); Serial.print(", ");
  Serial.print(lm.adjustedLaps, 4); Serial.print(", ");
  Serial.print(lm.laps, 4); Serial.print(", ");

  Serial.print(wm.updateCount); Serial.print(", ");
  Serial.print(wm.bin); Serial.print(", ");
  Serial.print(wm.binWeight, 4); Serial.print(", ");
  Serial.print(wm.binAdjustment, 4); Serial.print(", ");

  Serial.print(r.last.a); Serial.print(", ");
  Serial.print(r.last.b); Serial.print(", ");
  Serial.print(r.last.theta * 360.0 / sensor::ticksPerTurn); Serial.print(", ");
  Serial.print(r.thetaChange * 360.0 / sensor::ticksPerTurn); Serial.print(", ");

  Serial.print(br.chord.countNotes()); Serial.print(",");
  Serial.print(br.bass.countNotes()); Serial.print(",");

  Serial.print(r.last.jitter); Serial.print(", ");
  Serial.print(r.last.aReadTime); Serial.print(", ");
  Serial.print(r.last.bReadTime); Serial.print(", ");
  Serial.print(r.last.totalReadTime); Serial.print(", ");
  Serial.print(r.maxJitter); Serial.print(", ");
  Serial.print(r.minIdle); Serial.print(", ");
  Serial.println(r.sendTime);
  Serial.flush();
}

bassboard::Board boards[boardCount] = {
  bassboard::Board("lower", Wire, 32, &bassmaps::lowerChord, &bassmaps::lowerBass),
  bassboard::Board("upper", Wire, 33, &bassmaps::upperChord, &bassmaps::upperBass)
};

elapsedMillis sinceValidRead;

BassReadings pollBoards() {
  BassReadings result;
  result.chord = music::Chord();
  result.bass = music::Chord();
  bool allValid = true;
  for (int b = 0; b < boardCount; b++) {
    result.reading[b] = boards[b].poll();
    result.chord = result.chord + result.reading[b].chord;
    result.bass = result.bass + result.reading[b].bass;
    if (!result.reading[b].valid) {
      allValid = false;
    }
  }
  if (allValid) {
    sinceValidRead = 0;
  }
  return result;
}

bool logging = false;

void setup() {
  midiOut::begin();
  sensor::begin();

  Wire.setSDA(dataPin);
  Wire.setSCL(clockPin);
  Wire.setTimeout(50);
  pinMode(powerPin, OUTPUT);
  digitalWrite(powerPin, HIGH);

  for (int b = 0; b < boardCount; b++) {
    boards[b].begin();
  }
}

sensor::Report buffer;
sensor::Report* current = &buffer;

void loop() {
  if (!logging && Serial && Serial.dtr()) {
    printHeader();
  }
  logging = Serial.dtr();

  current = sensor::takeReport(current);
  LapMetrics lm = calculateLaps(current->last);
  trebleChannel.sendControlChange(bellowsControl, lm.midiValue);
  chordChannel.sendControlChange(bellowsControl, lm.midiValue);
  bassChannel.sendControlChange(bellowsControl, lm.midiValue);
  calibration::WeightMetrics wm = calibration::adjustWeights(lm.laps);

  BassReadings readings = pollBoards();
  bool noteChanged = chordChannel.sendChord(readings.chord) || bassChannel.sendChord(readings.bass);

  if (logging) {
    printLine(lm, wm, *current, readings);

    if (noteChanged) {
      Serial.print("# ");
      readings.chord.printTo(Serial);
      Serial.println();
      Serial.flush();
    }
  }
}

void loop1() {
  delay(100);
  sensor::runReadLoop();
}
