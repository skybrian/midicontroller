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

static void __not_in_flash_func(push)(Reading &r) {
  while (rp2040.fifo.pop() != READY) {}
  queuedSample = r;
  rp2040.fifo.push(SENT);
}

static void __not_in_flash_func(takeReading)(Reading& out) {
  elapsedMicros now = 0;

  out.a = analogRead(aSensorPin);
  out.aReadTime = now;

  out.b = analogRead(bSensorPin);
  out.bReadTime = now - out.aReadTime;
}

elapsedMicros now;

static void __not_in_flash_func(takeTimedReading)(int nextReadTime, Reading& out) {
  rp2040.idleOtherCore();
  takeReading(out); // warmup

  long readStart;
  int jitter;
  do {
    readStart = now;
    jitter = ((long)now) - nextReadTime;
  }
  while (jitter < 0);

  takeReading(out);
  out.jitter = jitter;
  rp2040.resumeOtherCore();
  out.totalReadTime = ((long)now) - readStart;
}

const int halfTurn = ticksPerTurn/2;
const int quarterTurn = ticksPerTurn/4;
const int eighthTurn = ticksPerTurn/8;

static int __not_in_flash_func(approximatePhase)(int x, int y) {
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

int laps = 0;
int prevTheta = 0;

static void __not_in_flash_func(countLaps)(Reading &r) {
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
}

static int __not_in_flash_func(calculatePhase)(int x, int y) {
  int theta = approximatePhase(x, y);
  if (theta < 0) theta += ticksPerTurn;
  return theta;
}

elapsedMicros sinceIdle;

int samplesSinceReport = 0;
int thetaChangeSinceReport = 0;
int maxIdle = 0;
int minIdle = samplePeriod;

static void __not_in_flash_func(readAndSend)(long nextReadTime) {
  while (((long)now) - nextReadTime < -110) {}
  int idle = sinceIdle;
  Reading r;
  takeTimedReading(nextReadTime, r);

  // Do calculations on the new reading

  r.theta = calculatePhase(r.a - 300, r.b - 300);
  countLaps(r);

  samplesSinceReport++;
  thetaChangeSinceReport += r.thetaChange;

  if (idle > maxIdle) maxIdle = idle;
  if (idle < minIdle) minIdle = idle;

  sinceIdle = 0;

  // Maybe send it

  if (samplesSinceReport >= samplesPerReport) {
    r.thetaChange = thetaChangeSinceReport;
    r.minIdle = minIdle;
    r.maxIdle = maxIdle;
    push(r);
    thetaChangeSinceReport = 0;
    maxIdle = 0;
    minIdle = samplePeriod;

    samplesSinceReport = 0;
  }
}

void __not_in_flash_func(runReadLoop)() {

  // warmup
  Reading r;
  rp2040.idleOtherCore();
  for (int i =0; i <10; i++) {
    sensor::takeReading(r);
  }
  rp2040.resumeOtherCore();
  prevTheta = calculatePhase(r.a - 300, r.b - 300);

  // take readings at fixed intervals
  now = -1000;
  long nextReadTime = 0;
  while (true) {
    readAndSend(nextReadTime);
    nextReadTime += samplePeriod;
  }
}

} // sensor
