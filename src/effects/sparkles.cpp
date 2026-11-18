// Pixels aléatoires qui s'allument puis s'estompent.
#include "effects.h"
#include "../matrix.h"

namespace {

void update(uint32_t) {
  fadeToBlackBy(leds, NUM_LEDS, 90);
  for (uint8_t i = 0; i < 6; i++) {
    leds[random16(NUM_LEDS)] = CHSV(random8(), 255, 255);
  }
  FastLED.show();
}

}  // namespace

const Animation effects::sparkles = {
    "randy", "Aléatoire", "🎲", AnimType::Effect, nullptr, update, nullptr};
