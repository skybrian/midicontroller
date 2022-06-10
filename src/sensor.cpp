#include "sensor.h"

namespace sensor {

const int aSensorPin = A0;
const int bSensorPin = A1;

// times in microseconds
const int samplePeriod = 5000;

// Messages for multi-core fifo.
// See: https://arduino-pico.readthedocs.io/en/latest/multicore.html#communicating-between-cores
enum { RECEIVED, SENT };

static Reading queuedSample;

Reading next() {
  while (rp2040.fifo.pop() != SENT) {}

  Reading result = queuedSample;
  rp2040.fifo.push(RECEIVED);
  return result;
}

static Reading readSample() {
  Reading out;

  elapsedMicros now = 0;
  out.a = analogRead(aSensorPin);
  out.aReadTime = now;

  out.b = analogRead(bSensorPin);
  out.bReadTime = now - out.aReadTime;

  return out;
}

elapsedMicros now;

static void readIntoQueue(long nextReadTime) {
  rp2040.idleOtherCore();
  readSample(); // warmup

  long readStart;
  int jitter;
  do {
    readStart = now;
    jitter = ((long)now) - nextReadTime;
  }
  while (jitter < 0);

  queuedSample = readSample();
  queuedSample.jitter = jitter;
  queuedSample.totalReadTime = ((long)now) - readStart;

  rp2040.fifo.push(SENT);
  rp2040.resumeOtherCore();

  while (rp2040.fifo.pop() != RECEIVED) {}
}

void runReadLoop() {

  // warmup
  rp2040.idleOtherCore();
  for (int i =0; i <10; i++) sensor::readSample();
  rp2040.resumeOtherCore();

  now = -1000;
  long nextReadTime = 0;
  while (true) {
    yield();
    while (((long)now) - nextReadTime < -110) {}
    readIntoQueue(nextReadTime);
    nextReadTime += samplePeriod;
  }
}

} // sensor
