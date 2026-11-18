#pragma once
#include "../animation.h"

// Envol : un oiseau tombe en permanence, chaque appui le fait remonter.
// Il faut passer dans les trous des colonnes qui défilent.
namespace flyer {

extern const Animation game;

void flap();  // démarre la partie, puis donne un coup d'aile

uint32_t score();
uint32_t best();

}  // namespace flyer
