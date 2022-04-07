#ifndef FILTERS_H
#define FILTERS_H

class LowPassFilter {
  const float cutoff;
  
  float smoothV = 0;

  public:
  LowPassFilter(const float cutoffHz) : cutoff(cutoffHz / 1000.0) {}

  void reset() {
    smoothV = 0;
  }

  float update(int v, int deltaT) {
    float weight = cutoff * deltaT; // bad approximation?
    if (weight > 1.0) weight = 1.0;
    smoothV = (1.0 - weight) * smoothV + weight * v;
    return smoothV;
  }
};

class HighPassFilter {
  LowPassFilter inner;

  public:
  HighPassFilter(const float cutoffHz) : inner(cutoffHz) {}

  void reset() {
    inner.reset();
  }

  float update(int v, int deltaT) {
    return v - inner.update(v, deltaT);
  }
};

#endif // FILTERS_H
