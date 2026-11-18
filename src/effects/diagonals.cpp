// Fines barres diagonales qui glissent, couleur qui tourne lentement.
#include "effects.h"
#include "../matrix.h"

namespace {

constexpr uint8_t SPACING = 3;  // une diagonale allumée sur SPACING

void render(uint32_t frame, bool rising) {
  FastLED.clear();
  const CRGB color = CHSV((uint8_t)(frame * 4), 255, 255);

  for (uint8_t y = 0; y < MATRIX_H; y++) {
    for (uint8_t x = 0; x < MATRIX_W; x++) {
      // x + y regroupe les pixels d'une même diagonale "/", x - y ceux d'une
      // diagonale "\" (décalé de MATRIX_H pour rester positif).
      const uint32_t diag = rising ? (x + y) : (MATRIX_H + x - y);
      if ((diag + frame) % SPACING == 0) leds[XY(x, y)] = color;
    }
  }
  FastLED.show();
}

void updateUp(uint32_t frame) { render(frame, true); }
void updateDown(uint32_t frame) { render(frame, false); }

}  // namespace

const Animation effects::diagUp = {
    "diag+", "Diagonales /", "📐", AnimType::Effect, nullptr, updateUp, nullptr};
const Animation effects::diagDown = {
    "diag-", "Diagonales \\", "📏", AnimType::Effect, nullptr, updateDown, nullptr};
