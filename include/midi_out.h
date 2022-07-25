#ifndef MIDI_OUT_H
#define MIDI_OUT_H

#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
#include <music.h>

namespace midiOut {

Adafruit_USBD_MIDI midiDev;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, midiDev, MID);

midi::DataByte prevControlValue = -1;

void begin() {
  MID.begin();
}

typedef midi::DataByte (*VelocityFunc)(music::Note n);

template<VelocityFunc velocity> class Channel  {
  midi::Channel chan;
  music::Chord prev;

public:
  Channel(midi::Channel channelNumber): chan(channelNumber) {}

  bool sendChord(music::Chord chord) {
    bool changed = false;

    for (music::Note n = music::ChordBase; n < music::ChordLimit; n = n + 1) {
          if (chord.has(n) && !prev.has(n)) {
            MID.sendNoteOn(n.toMidiNumber(), velocity(n), chan);
            changed = true;
          } else if (prev.has(n) && !chord.has(n)) {
            MID.sendNoteOff(n.toMidiNumber(), 0, chan);
            changed = true;
          }
      }
      prev = chord;
      return changed;
  }

  void sendAllNotesOff() {
    MID.sendControlChange(123, 0, chan); // all notes off
    prev = music::Chord();
  }
};

const int highControl = 1;
const int lowControl = highControl + 32;
const int maxControlValue = (1 << 14) - 1;

void __not_in_flash_func(sendControlChange)(int value) {
  // Sends a 14-bit control change.
  // This is sent in two messages with 7 bits each, highest bits first.
  // (This may cause a glitch in some cases.)
  // See discussion at: https://community.vcvrack.com/t/14-bit-midi-in-1-0/1779/86
  if (value < 0) value = 0;
  if (value > maxControlValue) value = maxControlValue;
  if (value == prevControlValue) {
    return;
  }
  //Serial.println(val);

  midi::DataByte lo = value & 0x7f;
  midi::DataByte hi = value >> 7;
  MID.sendControlChange(highControl, hi, 1);
  MID.sendControlChange(lowControl, lo, 1);
  prevControlValue = value;
}

} // namespace

#endif // MIDI_OUT_H
