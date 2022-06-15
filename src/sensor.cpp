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

static Report queuedSample;

void begin() {
  rp2040.fifo.push(READY);
}

Report __not_in_flash_func(next)() {
  while (rp2040.fifo.pop() != SENT) {}
  Report result = queuedSample;
  rp2040.fifo.push(READY);
  return result;
}

static void __not_in_flash_func(push)(Report &r) {
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

static void __not_in_flash_func(countLaps)(Report &rep) {
  int thetaChange = rep.last.theta - prevTheta;
  if (thetaChange < -halfTurn) {
    laps++;
    thetaChange += ticksPerTurn;
  } else if (thetaChange > halfTurn) {
    laps--;
    thetaChange -= ticksPerTurn;
  }
  rep.last.laps = laps;
  prevTheta = rep.last.theta;
  rep.thetaChange += thetaChange;
}

static int __not_in_flash_func(calculatePhase)(int x, int y) {
  int theta = approximatePhase(x, y);
  if (theta < 0) theta += ticksPerTurn;
  return theta;
}

elapsedMicros sinceIdle;

static void __not_in_flash_func(readAndCalculate)(long nextReadTime, Report& rep) {
  while (((long)now) - nextReadTime < -110) {}
  int idle = sinceIdle;
  takeTimedReading(nextReadTime, rep.last);

  rep.last.theta = calculatePhase(rep.last.a - 300, rep.last.b - 300);
  countLaps(rep);
  rep.samples++;

  if (rep.last.jitter > rep.maxJitter) rep.maxJitter = rep.last.jitter;
  if (idle < rep.minIdle) rep.minIdle = idle;

  sinceIdle = 0;
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
    Report rep;
    rep.samples = 0;
    rep.thetaChange = 0;
    rep.maxJitter = 0;
    rep.minIdle = samplePeriod;
    while(rep.samples < samplesPerReport) {
      readAndCalculate(nextReadTime, rep);
      nextReadTime += samplePeriod;
    }
    push(rep);
  }
}

} // sensor
