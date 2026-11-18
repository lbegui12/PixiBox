#include "live.h"
#include "matrix.h"

namespace live {
namespace {

// Les images de caméra sont en sRGB (gamma ~2.2) alors que les LEDs sont
// linéaires : sans correction, les couleurs paraissent délavées.
uint8_t gamma8[256];

}  // namespace

void begin() {
  for (int i = 0; i < 256; i++) {
    gamma8[i] = (uint8_t)(powf(i / 255.0f, 2.2f) * 255.0f + 0.5f);
  }
}

bool showFrame(const uint8_t* rgb, size_t length) {
  if (length != FRAME_SIZE) return false;

  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    // L'image arrive en ordre de lecture, la matrice est câblée en serpentin
    const uint8_t x = i % MATRIX_W;
    const uint8_t y = i / MATRIX_W;
    const uint8_t* p = rgb + i * 3;
    leds[XY(x, y)] = CRGB(gamma8[p[0]], gamma8[p[1]], gamma8[p[2]]);
  }
  FastLED.show();
  return true;
}

}  // namespace live
