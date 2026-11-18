#include "matrix.h"

CRGB leds[NUM_LEDS];

namespace matrix {

void begin() {
  FastLED.addLeds<LED_TYPE, LED_PIN, LED_COLOR_ORDER>(leds, NUM_LEDS)
      .setCorrection(TypicalLEDStrip);
  FastLED.setMaxPowerInVoltsAndMilliamps(LED_VOLTS, LED_MAX_MILLIAMPS);
  FastLED.setBrightness(DEFAULT_BRIGHTNESS);
  FastLED.clear(true);
}

void drawProgress(uint16_t permille, const CRGB& color) {
  if (permille > 1000) permille = 1000;
  const uint16_t lit = (uint32_t)permille * NUM_LEDS / 1000;
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    const uint8_t x = i % MATRIX_W;
    const uint8_t y = i / MATRIX_W;
    leds[XY(x, y)] = (i < lit) ? color : CRGB::Black;
  }
}

}  // namespace matrix
