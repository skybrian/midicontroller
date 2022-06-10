#ifndef SENSOR_H
#define SENSOR_H

#include <elapsedMillis.h>

namespace sensor {

struct Reading {
  int a;
  int b;

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
