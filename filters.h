#ifndef FILTERS_H
#define FILTERS_H

// A simple IIR low pass filter.
class LowPassFilter {
  const float weight;
  const float start;
  
  float smoothV;

  public:
  LowPassFilter(const float cutoffHz, const int deltaT, float startV = 0.0) :
    weight(cutoffHz / 1000000.0 * deltaT), // TODO: use correct equation here.
    start(startV),
    smoothV(startV) {}

  void reset() {
    smoothV = start;
  }

  // Given a signal and the elapsed time in microseconds, returns the new value.
  float update(float v) {
    smoothV = (1.0 - weight) * smoothV + weight * v;
    return smoothV;
  }
};

// A simple IIR high-pass filter.
class HighPassFilter {
  LowPassFilter inner;

  public:
  HighPassFilter(const float cutoffHz, const int deltaT, float startV = 0.0)
    : inner(cutoffHz, deltaT, startV) {}

  void reset() {
    inner.reset();
  }

  float update(float v) {
    return v - inner.update(v);
  }
};

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

  float update(float v) {
    sum += v;
    sum -= queue[next];
    queue[next] = v;
    next++;
    if (next >= SIZE) next = 0;
    return sum / SIZE;
  }
};

template<int period, int deltaT>
class MultipassFilter {
  static constexpr float decayHz = 10;

  MovingAverageFilter<period/(3 * deltaT)> a;
  MovingAverageFilter<period/(3 * deltaT)> b;
  MovingAverageFilter<period/(3 * deltaT)> c;
  LowPassFilter last;

public:
  MultipassFilter() : last(decayHz, deltaT) {
    reset();
  }

  void reset() {
    a.reset();
    b.reset();
    c.reset();
    last.reset();
  }

  float update(float v) {
    return last.update(c.update(b.update(a.update(v))));
  }
};

class ZeroLevelTracker {
  const float minSlope;
  LowPassFilter inner;
  const float start;
  float zeroLevel = 0;

  public:
  ZeroLevelTracker(const float minSlopeToAverage, const float cutoffHz, const int deltaT, float startLevel) :
    minSlope(minSlopeToAverage),
    inner(cutoffHz, deltaT, startLevel),
    start(startLevel),
    zeroLevel(start) {} 

  void reset() {
    zeroLevel = start;
  }

  float update(float v, float slope) {
    if (slope < 0) slope = -slope;
    if (slope > minSlope) {
      zeroLevel = inner.update(v);
    }
    return zeroLevel;
  }
};

#endif // FILTERS_H
