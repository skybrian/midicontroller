#ifndef BASSBOARD_H_
#define BASSBOARD_H_

#include <Arduino.h>
#include <Wire.h>
#include <elapsedMillis.h>

#include "music.h"

namespace bassboard {

enum Register : uint8_t {
  DirectionA = 0x00, // IO direction. Set to 1 for read (is default).
  DirectionB = 0x01,
  PullupA = 0x0C, // Set bit to 1 to enable pullup for a pin.
  PullupB = 0x0D,
  PortA = 0x12, // When read, returns the value of the pins.
  PortB = 0x13
};

// Communicates with a MPC23017 I/O extender over i2c.
class Device {
public:
  Device(TwoWire& i2cBus, int i2caddr) : bus(i2cBus), addr(i2caddr) {}

  // Configures the device. Returns true if successful.
  bool begin() {
    bus.begin();
    if (!ping()) {
      return false;
    }

    if (!writeTwoRegisters(DirectionA, 0xff, 0xff)) {
      Serial.println("Couldn't set direction registers");
      return false;
    }

    if (!writeTwoRegisters(PullupA, 0xff, 0xff)) {
      Serial.println("Couldn't set pullup registers");
      return false;
    }

    return true;
  }

  // Returns true if device is responding.
  bool ping() {
    bus.beginTransmission(addr);
    int err = bus.endTransmission();
    return err == 0;
  }

  // Reads the value of each pin. Returns true if successful. On error, the values won't be changed.
  bool readAll(uint16_t* values) {
    bus.beginTransmission(addr);
    bus.write(PortA);
    if (bus.endTransmission() != 0) return false;

    if (bus.requestFrom(addr, 2) != 2) return false;

    uint8_t low = bus.read();
    uint8_t hi = bus.read();
    *values = low | (hi << 8);
    return true;
  }

private:
  // Writes two sequential registers. Returns true if successful.
  bool writeTwoRegisters(Register reg, uint8_t val1, uint8_t val2) {
    bus.beginTransmission(addr);
    bus.write(reg);
    bus.write(val1);
    bus.write(val2);
    return bus.endTransmission() == 0;
  }

  int addr;
  TwoWire& bus;
};

const int buttonCount = 16;

struct KeyMap {
  music::Chord toNote[buttonCount];
};

struct Reading {
  music::Chord chord;
  music::Chord bass;
  long readTime;
  bool valid;
};

class Board {
public:
  const char *name;
  bool ready = false;

  Board(const char *boardName, TwoWire& bus, int i2cAddr, KeyMap *chord, KeyMap *bass) :
    name(boardName), device(bus, i2cAddr), chordMap(chord), bassMap(bass) {}

  // Returns true if successful.
  bool begin() {
   ready = device.begin();
   return ready;
  }

  bool reset() {
    return begin();
  }

  Reading poll() {
    elapsedMicros sinceStart;

    Reading result;
    result.chord = music::Chord();
    result.bass = music::Chord();

    uint16_t bits = 0;
    result.valid = device.readAll(&bits) && bits != 0; // all buttons down is probably a read error
    result.readTime = sinceStart;

    if (result.valid) {
      for (int i = 0; i < buttonCount; i++) {
        bool buttonDown = (bits & 1) == 0;
        if (buttonDown) {
          result.chord = result.chord + chordMap->toNote[i];
          result.bass = result.bass + bassMap->toNote[i];
        }
        bits = bits >> 1;
      }
    }

    return result;
  }

private:
  Device device;
  const KeyMap *chordMap;
  const KeyMap *bassMap;
  bool pressed[buttonCount];
};

} // namespace

#endif // BASSBOARD_H_
