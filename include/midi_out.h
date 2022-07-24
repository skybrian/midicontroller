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

} // namespace

#endif // MIDI_OUT_H
