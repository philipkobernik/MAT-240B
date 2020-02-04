#include "everything.h"
using namespace diy;

float sawtooth_harmonic(float harmonic_index, float frequency, double phase) {

  float sin_term = (sin(phase * harmonic_index))/harmonic_index;

  return std::pow(-1, harmonic_index) * sin_term;

}

int main(int argc, char* argv[]) {

  double phase = 0;
  float amplitude = 0.1f;
  float nyquist = SAMPLE_RATE/2.0f;

  for (float note = 127; note > 0; note -= 0.001) {
    float frequency = mtof(note);
    float sin_sum = 0.0f;

    for(int k=1; k*frequency < nyquist; k++) {
      // compute the sample
      sin_sum += sawtooth_harmonic(k, frequency, phase);
    }

    say((amplitude/2.0f) - (amplitude/pi)*sin_sum - (amplitude/2.0f));

    // increment phase
    phase += 2 * pi * frequency / SAMPLE_RATE;
    // wrap around
    if (phase > 2 * pi) phase -= 2 * pi;
  }
}
