// Rectangles concentriques dont les couleurs défilent : effet tunnel.
#include "effects.h"
#include "../matrix.h"

namespace {

constexpr uint8_t LAYERS = ((MATRIX_W < MATRIX_H ? MATRIX_W : MATRIX_H) + 1) / 2;

void update(uint32_t frame) {
  const uint8_t baseHue = frame * 25;

  for (uint8_t layer = 0; layer < LAYERS; layer++) {
    const uint8_t xMin = layer, xMax = MATRIX_W - 1 - layer;
    const uint8_t yMin = layer, yMax = MATRIX_H - 1 - layer;
    const CRGB color = CHSV(baseHue + layer * 30, 255, 255);

    for (uint8_t x = xMin; x <= xMax; x++) {
      leds[XY(x, yMin)] = color;
      leds[XY(x, yMax)] = color;
    }
    for (uint8_t y = yMin; y <= yMax; y++) {
      leds[XY(xMin, y)] = color;
      leds[XY(xMax, y)] = color;
    }
  }
  FastLED.show();
}

}  // namespace

const Animation effects::tunnel = {
    "tunnel", "Tunnel", "🌀", AnimType::Effect, nullptr, update, nullptr};
