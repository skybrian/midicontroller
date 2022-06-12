#ifndef FILTERS_H
#define FILTERS_H

#include <math.h>

template<size_t SIZE>
class MovingAverageFilter {
  float queue[SIZE];
  int next;
  float sum;

  public:
  MovingAverageFilter() {
    reset();
  }

  void reset() {
    for (int i = 0; i < SIZE; i++) {
      queue[i] = 0;
    }
    next = 0;
    sum = 0;
  }

  float __not_in_flash_func(update)(float v) {
    if (isnan(v) || isinf(v)) return v;
    sum += v;
    sum -= queue[next];
    queue[next] = v;
    next++;
    if (next >= SIZE) next = 0;
    return sum / SIZE;
  }
};

#endif // FILTERS_H
