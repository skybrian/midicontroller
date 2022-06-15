#ifndef SENSOR_H
#define SENSOR_H

#include <elapsedMillis.h>

namespace sensor {

const int ticksPerTurn = 720;

struct Reading {
  int a;
  int b;
  int theta;
  int laps;

  int jitter;
  int aReadTime;
  int bReadTime;
  int totalReadTime;
};

struct Report {
  Reading last;
  int samples;
  int thetaChange;
  int maxJitter;
  int minIdle;
};

void begin();

Report next();

void runReadLoop();

} // sensor

#endif // SENSOR_H
