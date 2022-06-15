#ifndef SENSOR_H
#define SENSOR_H

#include <elapsedMillis.h>
#include <limits.h>

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
  int sendTime;

  void clear() {
    samples = 0;
    thetaChange = 0;
    maxJitter = 0;
    minIdle = INT_MAX;
  }
};

void begin();

Report* takeReport(Report* nextDest);

void runReadLoop();

} // sensor

#endif // SENSOR_H
