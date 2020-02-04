#include "everything.h"
using namespace diy;

float impulse_harmonic(double phase) {

  float sin_term = sin(phase);

  return sin_term;

}

int main(int argc, char* argv[]) {

  double phase = 0;
  float amplitude = 0.3f;
  float nyquist = SAMPLE_RATE/2.0f;

  for (float note = 127; note > 0; note -= 0.001) {
    float frequency = mtof(note);
    float sine_sum = 0.0f;

    for(int k=1; k*frequency < nyquist; k++) {
      // compute the sample
      sine_sum += impulse_harmonic(k*phase)*0.2;
    }

    say(sine_sum);

    // increment phase
    phase += 2 * pi * frequency / SAMPLE_RATE;
    // wrap around
    if (phase > 2 * pi) phase -= 2 * pi;
  }
}
