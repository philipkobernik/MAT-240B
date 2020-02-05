#include "everything.h"
using namespace diy;

float square_harmonic(float harmonic_index, double phase) {

  float sin_term = sin(phase * (2.0*harmonic_index-1.0))/(2.0*harmonic_index-1.0);

  return sin_term;

}
// 22050 / 10k = 2.205
// so first partial (f0) would be #1
// second partial (f1) would be #2
// and third partial would be ramped in with scale of .205
int main(int argc, char* argv[]) {

  double phase = 0.0;
  float amplitude = 0.2f;
  float nyquist = SAMPLE_RATE/2.0f;

  for (float note = 127.0f; note > 0.0f; note -= 0.001f) {
    float frequency = mtof(note);
    float sin_sum = 0.0f;
    float harmonic_count = nyquist/frequency;
    int harmonic_whole = harmonic_count;
    float fade_ramp = harmonic_count - harmonic_whole;

    // fade in highest partial
    sin_sum += square_harmonic(harmonic_whole+1, phase) * amplitude * fade_ramp;

    for(float k=1.0f; k < harmonic_whole + 1.0f; k++) {
      // compute the sample
      sin_sum += square_harmonic(k, phase);
    }

    say((4/pi) * sin_sum * amplitude);

    // increment phase
    phase += 2 * pi * frequency / SAMPLE_RATE;
    // wrap around
    if (phase > 2 * pi) phase -= 2 * pi;
  }
}
