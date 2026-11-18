#pragma once
#include "../animation.h"

namespace snake {

enum class Dir : uint8_t { Up, Down, Left, Right };

extern const Animation game;

// Mémorise un virage (jusqu'à 2 d'avance, pour enchaîner deux virages rapides
// entre deux déplacements). Les demi-tours sont ignorés.
void queueDirection(Dir dir);

uint8_t length();
uint8_t best();

}  // namespace snake
