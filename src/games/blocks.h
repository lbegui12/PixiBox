#pragma once
#include "../animation.h"

// Blocs : des pièces de 4 cases tombent, on complète des lignes pour les effacer.
namespace blocks {

enum class Action : uint8_t { Left, Right, Rotate, SoftDrop, HardDrop };

extern const Animation game;

void input(Action action);

uint32_t score();
uint32_t best();
uint8_t level();

}  // namespace blocks
