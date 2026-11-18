// Jeu de la vie de Conway sur un tore (les bords se rejoignent).
// La couleur dépend de l'âge des cellules, les cellules mortes s'estompent.
// Quand la population se fige (ou oscille sur 2 états), on relance au hasard.
#include "effects.h"
#include "../matrix.h"

namespace {

constexpr uint8_t SEED_DENSITY = 90;      // sur 255 (~35 % de cellules vivantes)
constexpr uint16_t MAX_GENERATIONS = 400; // relance de toute façon après ça
constexpr uint8_t STAGNANT_FRAMES = 20;   // images affichées avant de relancer

uint8_t age[MATRIX_H][MATRIX_W];          // 0 = morte, sinon âge (saturé à 255)
uint8_t older[MATRIX_H][MATRIX_W];        // génération n-1 (vivant/mort)
uint8_t baseHue;
uint16_t generation;
uint8_t stagnantFor;

void seed() {
  for (uint8_t y = 0; y < MATRIX_H; y++)
    for (uint8_t x = 0; x < MATRIX_W; x++)
      age[y][x] = (random8() < SEED_DENSITY) ? 1 : 0;
  memset(older, 0, sizeof(older));
  baseHue = random8();
  generation = 0;
  stagnantFor = 0;
}

void begin() {
  FastLED.clear();
  seed();
}

uint8_t neighbours(int x, int y) {
  uint8_t count = 0;
  for (int dy = -1; dy <= 1; dy++) {
    for (int dx = -1; dx <= 1; dx++) {
      if (dx == 0 && dy == 0) continue;
      const int nx = (x + dx + MATRIX_W) % MATRIX_W;
      const int ny = (y + dy + MATRIX_H) % MATRIX_H;
      if (age[ny][nx]) count++;
    }
  }
  return count;
}

// Calcule la génération suivante. Renvoie true si l'état est figé ou
// oscille avec une période de 2.
bool step() {
  uint8_t next[MATRIX_H][MATRIX_W];
  bool sameAsCurrent = true, sameAsOlder = true;
  uint16_t population = 0;

  for (int y = 0; y < MATRIX_H; y++) {
    for (int x = 0; x < MATRIX_W; x++) {
      const uint8_t n = neighbours(x, y);
      const bool alive = age[y][x];
      const bool willLive = (n == 3) || (alive && n == 2);
      next[y][x] = willLive ? qadd8(age[y][x], 1) : 0;

      if (willLive != alive) sameAsCurrent = false;
      if (willLive != (bool)older[y][x]) sameAsOlder = false;
      if (willLive) population++;
    }
  }

  for (int y = 0; y < MATRIX_H; y++)
    for (int x = 0; x < MATRIX_W; x++) older[y][x] = age[y][x] ? 1 : 0;
  memcpy(age, next, sizeof(age));
  generation++;

  return population == 0 || sameAsCurrent || sameAsOlder;
}

void update(uint32_t) {
  const bool stagnant = step();
  if (stagnant || generation >= MAX_GENERATIONS) stagnantFor++;
  if (stagnantFor >= STAGNANT_FRAMES) seed();

  fadeToBlackBy(leds, NUM_LEDS, 120);  // traînée des cellules mortes
  for (uint8_t y = 0; y < MATRIX_H; y++) {
    for (uint8_t x = 0; x < MATRIX_W; x++) {
      const uint8_t a = age[y][x];
      if (!a) continue;
      // Naissance = presque blanc, puis la couleur se sature avec l'âge
      const uint8_t sat = a == 1 ? 80 : 255;
      leds[XY(x, y)] = CHSV(baseHue + min<int>(a, 40) * 3, sat, 255);
    }
  }
  FastLED.show();
}

}  // namespace

const Animation effects::life = {
    "life", "Vie (Conway)", "🧬", AnimType::Effect, begin, update, nullptr};
