#ifndef SENSOR_H
#define SENSOR_H

#include <elapsedMillis.h>

namespace sensor {

const int ticksPerTurn = 720;

struct Reading {
  int a;
  int b;
  int theta;
  int thetaChange;
  int laps;

  int idle;
  int jitter;
  int aReadTime;
  int bReadTime;
  int totalReadTime;
};

void begin();

Reading next();

void runReadLoop();

} // sensor

#endif // SENSOR_H
