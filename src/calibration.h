#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <math.h>

namespace calibration {

const int binCount = 36;

class Weights {
public:
  float bin[binCount];
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

  void __not_in_flash_func(add)(int i, float val) {
      bin[((i % binCount) + binCount) % binCount] += val;
      total += val;
  }

  void __not_in_flash_func(update)(Weights& next, float nextWeight) {
    float decay = 1 - nextWeight;
    nextWeight = nextWeight / next.total;
    for (int i = 0; i < binCount; i++) {
      bin[i] = bin[i] * decay + next.bin[i] * nextWeight;
    }
  }

  // returns range of bins changed
  float __not_in_flash_func(addRange)(float start, float end, float val) {
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
      return end - start;
    }
    float valPerBin = val/(end - start);
    add(startbin, valPerBin * (ceil(start) - start));
    add(endbin, valPerBin * (end - floor(end)));
    for (int i = startbin + 1; i < endbin; i++) {
      add(i, valPerBin);
    }
    return end - start;
  }
};

class LookupTable {
public:
  float bin[binCount + 1];
  LookupTable() {
    float scale = 1.0/binCount;
    for (int i = 0; i <= binCount; i++) {
      bin[i] = scale * i;
    }
  }

  void __not_in_flash_func(setWeights)(float weights[binCount]) {
    float sum = 0;
    for (int i = 0; i <= binCount; i++) {
      bin[i] = sum;
      sum += weights[i];
    }
  }

  float __not_in_flash_func(adjust)(float laps) {
    float count = floor(laps);
    float fraction = laps - count;
    int leftBin = floor(fraction * binCount);
    float left = bin[leftBin];
    float right = bin[leftBin + 1];
    float rightWeight = fraction * binCount - leftBin;
    return count + left * (1 - rightWeight) + right * rightWeight;
  }
};

const int minSamples = binCount/2;

Weights weights(1.0/binCount);
LookupTable lookupTable;

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

void __not_in_flash_func(updateWeights)(float laps) {
  if (foundLap) {
    if (partial.total >= minSamples) {
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
  float rangeChanged = partial.addRange(prevPos, laps, 1);
  if (rangeChanged < 0.5) {
    // too slow
    lapDirection = none;
    return;
  }
  prevPos = laps;
}

struct WeightMetrics {
  int updateCount;
  int bin;
  float binWeight;
  float binAdjustment;
};

WeightMetrics __not_in_flash_func(adjustWeights)(float laps) {
  updateWeights(laps);
  WeightMetrics result;
  result.updateCount = weightUpdateCount;
  result.bin = nextBin;
  result.binWeight = weights.get(nextBin);
  result.binAdjustment = lookupTable.bin[nextBin];
  nextBin = (nextBin + 1) % binCount;
  return result;
}

int seenWeightUpdates = 0;

float __not_in_flash_func(adjustLaps)(float laps) {
  if (seenWeightUpdates < weightUpdateCount) {
      lookupTable.setWeights(weights.bin);
      seenWeightUpdates = weightUpdateCount;
  }
  return lookupTable.adjust(laps);
}

} // calibration

#endif // CALIBRATION_H
