#include "sensor.h"

namespace sensor {

const int aSensorPin = A0;
const int bSensorPin = A1;

// times in microseconds
const int samplePeriod = 5000;

// Messages for multi-core fifo.
// See: https://arduino-pico.readthedocs.io/en/latest/multicore.html#communicating-between-cores
enum { READY, SENT };

static Reading queuedSample;

void begin() {
  rp2040.fifo.push(READY);
}

Reading __not_in_flash_func(next)() {
  while (rp2040.fifo.pop() != SENT) {}
  Reading result = queuedSample;
  rp2040.fifo.push(READY);
  return result;
}

static Reading __not_in_flash_func(readSample)() {
  Reading out;

  elapsedMicros now = 0;
  out.a = analogRead(aSensorPin);
  out.aReadTime = now;

  out.b = analogRead(bSensorPin);
  out.bReadTime = now - out.aReadTime;

  return out;
}

elapsedMicros now;

static void __not_in_flash_func(readIntoQueue)(long nextReadTime) {
  rp2040.idleOtherCore();
  readSample(); // warmup

  long readStart;
  int jitter;
  do {
    readStart = now;
    jitter = ((long)now) - nextReadTime;
  }
  while (jitter < 0);

  Reading r = readSample();
  r.jitter = jitter;
  r.totalReadTime = ((long)now) - readStart;
  rp2040.resumeOtherCore();

  while (rp2040.fifo.pop() != READY) {}
  queuedSample = r;
  rp2040.fifo.push(SENT);
}

void __not_in_flash_func(runReadLoop)() {

  // warmup
  rp2040.idleOtherCore();
  for (int i =0; i <10; i++) sensor::readSample();
  rp2040.resumeOtherCore();

  now = -1000;
  long nextReadTime = 0;
  while (true) {
    while (((long)now) - nextReadTime < -110) {}
    readIntoQueue(nextReadTime);
    nextReadTime += samplePeriod;
  }
}

} // sensor
