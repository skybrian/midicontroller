#ifndef FILTERS_H
#define FILTERS_H

class LowPassFilter {
  const float cutoff;
  const float start;
  
  float smoothV;

  public:
  LowPassFilter(const float cutoffHz, float startV = 0.0) :
    cutoff(cutoffHz / 1000.0),
    start(startV),
    smoothV(startV) {}

  void reset() {
    smoothV = start;
  }

  float update(float v, int deltaT) {
    float weight = cutoff * deltaT; // bad approximation?
    if (weight > 1.0) weight = 1.0;
    smoothV = (1.0 - weight) * smoothV + weight * v;
    return smoothV;
  }
};

class HighPassFilter {
  LowPassFilter inner;

  public:
  HighPassFilter(const float cutoffHz, float startV = 0.0) : inner(cutoffHz, startV) {}

  void reset() {
    inner.reset();
  }

  float update(float v, int deltaT) {
    return v - inner.update(v, deltaT);
  }
};

class ZeroLevelTracker {
  const float minSlope;
  LowPassFilter inner;
  const float start;
  float zeroLevel = 0;

  public:
  ZeroLevelTracker(const float minSlopeToAverage, const float cutoffHz, float startLevel) :
    minSlope(minSlopeToAverage),
    inner(cutoffHz, startLevel),
    start(startLevel),
    zeroLevel(start) {} 

  void reset() {
    zeroLevel = start;
  }

  float update(float v, int deltaV, float slope) {
    if (slope < 0) slope = -slope;
    if (slope > minSlope) {
      zeroLevel = inner.update(v, deltaV);
    }
    return zeroLevel;
  }
};

#endif // FILTERS_H
