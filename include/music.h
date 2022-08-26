#ifndef MUSIC_H_
#define MUSIC_H_

#include <cstdint>
#include "Print.h"

// Some basic music theory, defined using midi note values.

namespace music {

class Note {
public:
  constexpr Note(const unsigned char midiNoteNumber) : num(midiNoteNumber) {}

  constexpr Note operator +(const unsigned char offset) const {
    return Note(num + offset);
  }

  constexpr Note operator -(const unsigned char offset) const {
    return Note(num - offset);
  }

  constexpr signed char operator -(const Note other) const {
    return num - other.num;
  }

  constexpr unsigned char toMidiNumber() const {
    return num;
  }

  constexpr const char* const name() {
    return names[num % 12];
  }

  constexpr int octave() {
    return (num / 12) -1;
  }

  constexpr Note toOctaveRangeFrom(Note base) const {
    int remainder = ((num + 144) - base.num) % 12;
    return Note(base + (unsigned char)remainder);
  }

  bool operator <(const Note other) const {
    return num < other.num;
  }

  bool operator <=(const Note other) const {
    return num <= other.num;
  }

private:
  unsigned char num;

  static constexpr const char* const names[] =
     { "C", "C#", "D", "D#","E", "F", "F#", "G", "G#", "A", "A#", "B" };
};

// Octave number in constants increments at C.
// See: https://en.wikipedia.org/wiki/Scientific_pitch_notation
// Midi charts may label notes differently! (Yamaha uses C3 for middle C.)

constexpr Note MiddleC = 60;
constexpr Note C4 = MiddleC;

constexpr Note C1 = MiddleC - 12*3;
constexpr Note D1 = C1 + 2;
constexpr Note E1 = D1 + 2;
constexpr Note F1 = E1 + 1;
constexpr Note G1 = F1 + 2;
constexpr Note A1 = G1 + 2;
constexpr Note B1 = A1 + 2;

constexpr Note C2 = C1 + 12;
constexpr Note D2 = C2 + 2;
constexpr Note E2 = D2 + 2;
constexpr Note F2 = E2 + 1;
constexpr Note G2 = F2 + 2;
constexpr Note A2 = G2 + 2;
constexpr Note B2 = A2 + 2;

constexpr Note C3 = C2 + 12;
constexpr Note D3 = C3 + 2;
constexpr Note E3 = D3 + 2;
constexpr Note F3 = E3 + 1;
constexpr Note G3 = F3 + 2;
constexpr Note A3 = G3 + 2;
constexpr Note B3 = A3 + 2;

// 128 bits would cover the entire MIDI range, but just using 64 will do.

constexpr Note ChordBase = C1;
constexpr Note ChordLimit = ChordBase + 64;

class Chord {
private:
  uint64_t bits;

  static constexpr uint64_t toBit(Note n) {
    return ((uint64_t)1) << (n - ChordBase);
  }

  static constexpr uint64_t octaveBits(Note n) {
    return toBit(n) | toBit(n + 12) | toBit(n + 24);
  }

  constexpr Chord(uint64_t bitset) : bits(bitset) {}
public:
  constexpr Chord() : bits(0) {}
  constexpr Chord(Note n) : bits(toBit(n)) {}
  constexpr Chord(Note n1, Note n2) : bits(toBit(n1) | toBit(n2)) {}
  constexpr Chord(Note n1, Note n2, Note n3) : bits(toBit(n1) | toBit(n2) | toBit(n3)) {}

  constexpr Chord static doubleOctave(Note n) {
    return Chord(octaveBits(n));
  }

  constexpr static Chord majorUp(Note n) {
    return Chord(toBit(n) | toBit(n + 4) | toBit(n + 7));
  }

  constexpr static Chord majorDown(Note n) {
    return Chord(toBit(n) | toBit(n - 8) | toBit(n - 5));
  }

  constexpr static Chord majorMid(Note n) {
    return Chord(toBit(n) | toBit(n + 4) | toBit(n - 5));
  }

  constexpr static Chord minorUp(Note n) {
    return Chord(toBit(n) | toBit(n + 3) | toBit(n + 7));
  }

  constexpr static Chord minorDown(Note n) {
    return Chord(toBit(n) | toBit(n - 9) | toBit(n - 5));
  }

  constexpr static Chord minorMid(Note n) {
    return Chord(toBit(n) | toBit(n + 3) | toBit(n - 5));
  }

  // Returns the union of the notes in both chords.
  constexpr Chord operator +(const Chord other) const {
    return Chord(bits | other.bits);
  }

  constexpr bool has(Note n) {
    return (Chord(n).bits & bits) != 0;
  }

  int countNotes() {
    int notes = 0;
    for (Note n = ChordBase ; n < ChordLimit; n = n + 1) {
      if (has(n)) {
        notes++;
      }
    }
    return notes;
  }

  void printTo(Print& out) {
    out.print("Chord(");
    int notes = 0;
    for (Note n = ChordBase ; n < ChordLimit; n = n + 1) {
      if (has(n)) {
          if (notes>0) {
            out.print(", ");
          }
          out.print(n.name());
          out.print(n.octave());
          notes++;
      }
    }
    out.print(")");
  }
};

} // namespace music

#endif // MUSIC_H_
