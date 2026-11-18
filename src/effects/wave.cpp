// Simulation d'onde 2D (équation des ondes discrète, schéma "ripple").
//   - "Onde"   : source sinusoïdale au centre
//   - "Vagues" : source sur toute la ligne du bas (onde plane qui monte)
// Rouge = amplitude positive, bleu = amplitude négative.
#include "effects.h"
#include "../matrix.h"

namespace {

enum class Source : uint8_t { Center, Bottom };

constexpr int16_t LIMIT = 255;
constexpr uint8_t DAMPING_SHIFT = 5;  // perte de ~3% par image (évite la saturation)

int16_t current[MATRIX_H][MATRIX_W];
int16_t previous[MATRIX_H][MATRIX_W];

void reset() {
  memset(current, 0, sizeof(current));
  memset(previous, 0, sizeof(previous));
}

int16_t at(int x, int y) {
  return inBounds(x, y) ? current[y][x] : 0;  // bords réfléchissants
}

void step(uint32_t frame, Source source) {
  int16_t next[MATRIX_H][MATRIX_W];

  for (int y = 0; y < MATRIX_H; y++) {
    for (int x = 0; x < MATRIX_W; x++) {
      const int32_t neighbours = at(x, y - 1) + at(x, y + 1) + at(x - 1, y) + at(x + 1, y);
      int32_t v = neighbours / 2 - previous[y][x];
      v -= v >> DAMPING_SHIFT;  // amortissement (fonctionne aussi pour v < 0)
      next[y][x] = constrain(v, -LIMIT, LIMIT);
    }
  }

  const int16_t excitation = ((int16_t)sin8(frame * 8) - 128) * 2;  // -256..254
  if (source == Source::Center) {
    next[MATRIX_H / 2][MATRIX_W / 2] = constrain(excitation, -LIMIT, LIMIT);
  } else {
    for (int x = 0; x < MATRIX_W; x++) {
      next[MATRIX_H - 1][x] = constrain(excitation, -LIMIT, LIMIT);
    }
  }

  memcpy(previous, current, sizeof(current));
  memcpy(current, next, sizeof(current));
}

void render() {
  for (uint8_t y = 0; y < MATRIX_H; y++) {
    for (uint8_t x = 0; x < MATRIX_W; x++) {
      const int16_t v = current[y][x];
      const uint8_t level = min<int>(255, abs(v) * 2);
      leds[XY(x, y)] = CHSV(v >= 0 ? 0 : 160, 255, level);
    }
  }
  FastLED.show();
}

void updateCenter(uint32_t frame) {
  step(frame, Source::Center);
  render();
}

void updateBottom(uint32_t frame) {
  step(frame, Source::Bottom);
  render();
}

}  // namespace

const Animation effects::ripple = {
    "ripple", "Onde", "💧", AnimType::Effect, reset, updateCenter, nullptr};
const Animation effects::waves = {
    "waves", "Vagues", "🌊", AnimType::Effect, reset, updateBottom, nullptr};
