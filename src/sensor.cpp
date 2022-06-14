#include "sensor.h"

namespace sensor {

const int aSensorPin = A0;
const int bSensorPin = A1;

// times in microseconds
const int samplePeriod = 1000;
const int samplesPerReport = 5;

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

static void __not_in_flash_func(readSample)(Reading& out) {
  elapsedMicros now = 0;

  out.a = analogRead(aSensorPin);
  out.aReadTime = now;

  out.b = analogRead(bSensorPin);
  out.bReadTime = now - out.aReadTime;
}

const int halfTurn = ticksPerTurn/2;
const int quarterTurn = ticksPerTurn/4;
const int eighthTurn = ticksPerTurn/8;

static int __not_in_flash_func(calculatePhase)(int x, int y) {
  // A very rough approximation of atan2. It will be adjusted via calibration later so it shouldn't matter.
  if (x >= abs(y)) {
    return (eighthTurn * y)/x;
  } else if (y >= abs(x)) {
    return quarterTurn + (-eighthTurn * x)/y;
  } else if (-y >= abs(x)) {
    return -quarterTurn + (-eighthTurn * x)/y;
  } else {
    return halfTurn + (eighthTurn * y)/x;
  }
}

elapsedMicros now;
elapsedMicros sinceIdle;

int prevTheta = 0;
int laps = 0;
int samplesSinceReport = 0;
int thetaChangeSinceReport = 0;

static void __not_in_flash_func(readIntoQueue)(long nextReadTime) {
  Reading r;
  r.idle = sinceIdle;
  rp2040.idleOtherCore();
  readSample(r); // warmup

  long readStart;
  int jitter;
  do {
    readStart = now;
    jitter = ((long)now) - nextReadTime;
  }
  while (jitter < 0);

  readSample(r);
  r.jitter = jitter;
  rp2040.resumeOtherCore();
  r.totalReadTime = ((long)now) - readStart;
  r.theta = calculatePhase(r.a - 300, r.b - 300);
  if (r.theta < 0) r.theta += ticksPerTurn;
  r.thetaChange = r.theta - prevTheta;
  if (r.thetaChange < -halfTurn) {
    laps++;
    r.thetaChange += ticksPerTurn;
  } else if (r.thetaChange > halfTurn) {
    laps--;
    r.thetaChange -= ticksPerTurn;
  }
  r.laps = laps;
  prevTheta = r.theta;

  samplesSinceReport++;
  thetaChangeSinceReport += r.thetaChange;
  sinceIdle = 0;

  if (samplesSinceReport >= samplesPerReport) {
    r.thetaChange = thetaChangeSinceReport;
    samplesSinceReport = 0;
    thetaChangeSinceReport = 0;
    while (rp2040.fifo.pop() != READY) {}
    queuedSample = r;
    rp2040.fifo.push(SENT);
  }
}

void __not_in_flash_func(runReadLoop)() {

  // warmup
  Reading r;
  rp2040.idleOtherCore();
  for (int i =0; i <10; i++) {
    sensor::readSample(r);
  }
  rp2040.resumeOtherCore();
  prevTheta = calculatePhase(r.a - 300, r.b - 300);

  now = -1000;
  long nextReadTime = 0;
  while (true) {
    while (((long)now) - nextReadTime < -110) {}
    readIntoQueue(nextReadTime);
    nextReadTime += samplePeriod;
  }
}

} // sensor
