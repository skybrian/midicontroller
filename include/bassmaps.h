#ifndef BASSMAPS_H_
#define BASSMAPS_H_

#include <Arduino.h>
#include <Wire.h>
#include <elapsedMillis.h>

#include "music.h"
#include "bassboard.h"

namespace bassmaps {

typedef music::Chord Chord;
typedef music::Note Note;

Note bottomBass = music::G1;

Chord bass(music::Note note) {
  return Chord::doubleOctave(note.toOctaveRangeFrom(bottomBass));
}

bassboard::KeyMap lowerChord = {{
  Chord(), Chord(), Chord(), Chord(), Chord(), Chord(), Chord(), Chord(),

  Chord::majorUp(music::E3), Chord::majorDown(music::A3), Chord::majorUp(music::D3), Chord::majorMid(music::G3),
  Chord::majorUp(music::C3), Chord::majorMid(music::F3), Chord::majorDown(music::B3-1), Chord::majorUp(music::E3-1),
}};

bassboard::KeyMap lowerBass = {{
  bass(music::G2), bass(music::D2), bass(music::A2), bass(music::E2),
  bass(music::B2), bass(music::F2 + 1), bass(music::C2 + 1), bass(music::G2 + 1),

  Chord(), Chord(), Chord(), Chord(), Chord(), Chord(), Chord(), Chord(),
}};


bassboard::KeyMap upperChord = {{
  Chord::minorUp(music::E3), Chord::minorDown(music::A3), Chord::minorUp(music::D3), Chord::minorMid(music::G3),
  Chord::minorUp(music::C3), Chord::minorMid(music::F3), Chord::minorDown(music::B3-1), Chord::minorUp(music::E3-1),

  Chord(), Chord(), Chord(), Chord(), Chord(), Chord(), Chord(), Chord(),
}};

bassboard::KeyMap upperBass = {{
  Chord(), Chord(), Chord(), Chord(), Chord(), Chord(), Chord(), Chord(),

  bass(music::E2-1), bass(music::B2-1), bass(music::F2), bass(music::C2),
  bass(music::G2), bass(music::D2), bass(music::A2), bass(music::E2),
}};

} // namespace

#endif // BASSMAPS_H_
