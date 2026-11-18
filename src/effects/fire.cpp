// Feu de cheminée : adaptation de "Fire2012" (exemple FastLED) à une matrice,
// une simulation de chaleur par colonne, les flammes montent depuis le bas.
#include "effects.h"
#include "../matrix.h"

namespace {

constexpr uint8_t COOLING = 70;    // plus grand = flammes plus courtes
constexpr uint8_t SPARKING = 130;  // plus grand = feu plus vif

uint8_t heat[MATRIX_W][MATRIX_H];  // heat[x][0] = bas de la matrice

void begin() { memset(heat, 0, sizeof(heat)); }

void update(uint32_t) {
  for (uint8_t x = 0; x < MATRIX_W; x++) {
    uint8_t* column = heat[x];

    // 1. Chaque cellule refroidit un peu
    for (uint8_t i = 0; i < MATRIX_H; i++) {
      column[i] = qsub8(column[i], random8(0, (COOLING * 10) / MATRIX_H + 2));
    }
    // 2. La chaleur monte et se diffuse
    for (uint8_t i = MATRIX_H - 1; i >= 2; i--) {
      column[i] = (column[i - 1] + column[i - 2] + column[i - 2]) / 3;
    }
    // 3. Nouvelles étincelles en bas
    if (random8() < SPARKING) {
      const uint8_t i = random8(2);
      column[i] = qadd8(column[i], random8(160, 255));
    }
    // 4. Affichage (i = 0 correspond à la ligne du bas)
    for (uint8_t i = 0; i < MATRIX_H; i++) {
      leds[XY(x, MATRIX_H - 1 - i)] = HeatColor(column[i]);
    }
  }
  FastLED.show();
}

}  // namespace

const Animation effects::fire = {
    "fire", "Feu", "🔥", AnimType::Effect, begin, update, nullptr};
