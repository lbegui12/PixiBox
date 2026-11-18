// Pluie colorée : chaque colonne descend d'un cran une image sur deux en
// moyenne, et la ligne du haut est régénérée aléatoirement.
#include "effects.h"
#include "../matrix.h"

namespace {

uint8_t drops[MATRIX_H][MATRIX_W];  // 0 = vide, sinon goutte

void begin() { memset(drops, 0, sizeof(drops)); }

void update(uint32_t frame) {
  for (uint8_t x = 0; x < MATRIX_W; x++) {
    if (!(random8() & 1)) continue;  // 1 chance sur 2 que la colonne bouge

    // Décalage vers le bas, en partant du bas pour ne rien écraser
    for (uint8_t y = MATRIX_H - 1; y > 0; y--) drops[y][x] = drops[y - 1][x];
    drops[0][x] = (random8() & 1) ? 255 : 0;
  }

  for (uint8_t x = 0; x < MATRIX_W; x++) {
    // Cast en uint8_t : la teinte boucle proprement sur 0..255
    const CRGB color = CHSV((uint8_t)(3 * frame - 10 * x), 255, 255);
    for (uint8_t y = 0; y < MATRIX_H; y++) {
      leds[XY(x, y)] = drops[y][x] ? color : CRGB::Black;
    }
  }
  FastLED.show();
}

}  // namespace

const Animation effects::rain = {
    "rain", "Pluie", "🌧️", AnimType::Effect, begin, update, nullptr};
