#pragma once
// Accès bas niveau à la matrice : tableau de LEDs + conversion (x, y) -> index.

#include <Arduino.h>
#include <FastLED.h>
#include "config.h"

extern CRGB leds[NUM_LEDS];

// Câblage en colonnes "serpentin" :
//   colonne 0 de haut en bas, colonne 1 de bas en haut, colonne 2 de haut en bas...
// (0, 0) = coin haut-gauche, x vers la droite, y vers le bas.
inline uint16_t XY(uint8_t x, uint8_t y) {
  return (x & 1) ? (x * MATRIX_H) + (MATRIX_H - 1 - y)
                 : (x * MATRIX_H) + y;
}

inline bool inBounds(int x, int y) {
  return x >= 0 && x < MATRIX_W && y >= 0 && y < MATRIX_H;
}

// Version protégée : ignore silencieusement les coordonnées hors matrice.
inline void setPixel(int x, int y, const CRGB& color) {
  if (inBounds(x, y)) leds[XY(x, y)] = color;
}

namespace matrix {

void begin();

// Remplit la matrice ligne par ligne (haut -> bas) proportionnellement à
// `permille` (0..1000). Sert de barre de progression (WiFi, OTA...).
void drawProgress(uint16_t permille, const CRGB& color);

}  // namespace matrix
