#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <math.h>

namespace calibration {

const int binCount = 36;

class Weights {
  float bin[binCount];
public:
  float total;

  Weights(float val) {
    fill(val);
  }

  double get(int i) {
    return bin[((i % binCount) + binCount) % binCount];
  }

  void fill(float val) {
    for (int i = 0; i < binCount; i++) {
      bin[i] = val;
    }
    total = val * binCount;
  }

  void add(int i, float val) {
      bin[((i % binCount) + binCount) % binCount] += val;
      total += val;
  }

  void update(Weights& next, float nextWeight) {
    float oldWeight = 1 - nextWeight;
    nextWeight = nextWeight / next.total;
    for (int i = 0; i < binCount; i++) {
      bin[i] = bin[i] * oldWeight + next.bin[i] * nextWeight;
    }
  }

  void addRange(float start, float end, float val) {
    if (end < start) {
      float tmp = start;
      start = end;
      end = tmp;
    }
    start *= binCount;
    end *= binCount;
    int startbin = floor(start);
    int endbin = floor(end);
    if (startbin == endbin) {
      add(startbin, val);
      return;
    }
    float weight = val/(end - start);
    add(startbin, weight * (ceil(start) - start));
    add(endbin, weight * (end - floor(end)));
    for (int i = startbin + 1; i < endbin; i++) {
      add(i, weight);
    }
  }
};

const int minSamples = 10;
const int maxSamples = 50;

Weights weights(1.0/binCount);

enum LapDirection {
  none = 0,
  down = -1,
  up = 1,
};

LapDirection lapDirection = none;
Weights partial(0);
double prevPos = nanf("");
double finishLine;
int weightUpdateCount = 0;
int nextBin = 0;
bool foundLap = false;

void updateWeights(float laps) {
  if (foundLap) {
    if (partial.total >= minSamples && partial.total <= maxSamples) {
      weights.update(partial, 0.02);
      weightUpdateCount++;
    }
    foundLap = false;
    return;
  }

  float delta = laps - prevPos;

  LapDirection dir = delta < 0 ? down : (delta > 0) ? up : none;
  if (lapDirection == none || dir != lapDirection) {
    lapDirection = dir;
    partial.fill(0);
    prevPos = laps;
    finishLine = laps + lapDirection;
    return;
  }

  if (dir == up) {
    if (laps >= finishLine) {
      partial.addRange(prevPos, finishLine, finishLine - prevPos);
      foundLap = true;
      lapDirection = none;
      return;
    }
  } else {
    if (laps <= finishLine) {
      partial.addRange(finishLine, prevPos, prevPos - finishLine);
      foundLap = true;
      lapDirection = none;
      return;
    }
  }
  partial.addRange(prevPos, laps, 1);
  prevPos = laps;
}

struct WeightMetrics {
  int updateCount;
  int bin;
  float binValue;
};

WeightMetrics calculateWeights(float laps) {
  updateWeights(laps);
  WeightMetrics result;
  result.updateCount = weightUpdateCount;
  result.bin = nextBin;
  result.binValue = weights.get(nextBin);
  nextBin = (nextBin + 1) % binCount;
  return result;
}

} // calibration

#endif // CALIBRATION_H
