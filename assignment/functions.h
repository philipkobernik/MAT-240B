#pragma once
namespace diy {

// functions
//
// these should tend to...
// - be stateless
// - be useful for musical mappings
// - borrow from an existing system's lexicon

// like Max's [scale] and processing's map
inline float map(float value, float low, float high, float Low, float High) {
  return Low + (High - Low) * ((value - low) / (high - low));
}
inline float norm(float value, float low, float high) {
  return (value - low) / (high - low);
}

// like Max's [mix]
inline float lerp(float a, float b, float t) { return (1.0f - t) * a + t * b; }

inline float mtof(float m) { return 8.175799f * powf(2.0f, m / 12.0f); }
inline float ftom(float f) { return 12.0f * log2f(f / 8.175799f); }
inline float dbtoa(float db) { return 1.0f * powf(10.0f, db / 20.0f); }
inline float atodb(float a) { return 20.0f * log10f(a / 1.0f); }

// kind of like tanh: takes large numbers and puts them on (-1, 1) gently
inline float sigmoid(float x) { return 2.0f / (1.0f + expf(-x)) - 1.0f; }

// these expect a phase on (0, 1)
inline float saw(float phase) { return phase * 2 - 1; }
inline float rect(float phase) { return phase < 0.5 ? 1 : -1; }
inline float tri(float phase) {
  // can you make this fewer instructions???
  float f = 2 * phase - 1;
  f = (f < -0.5) ? -1 - f : (f > 0.5 ? 1 - f : f);
  return 2 * f;
}

inline float pulse(float phase, float width = 0.5) {
  return phase < width ? 1 : -1;
}

}  // namespace diy
