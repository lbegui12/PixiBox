#pragma once
#include "../animation.h"

// Invasion : une formation d'envahisseurs descend en zigzag, on la détruit avec
// le canon du bas avant qu'elle n'arrive en bas. 3 vies, vagues de plus en plus rapides.
namespace invasion {

extern const Animation game;

void setPosition(uint16_t permille);  // 0 = gauche, 1000 = droite
void nudge(int8_t step);              // -1 / +1 case (clavier)
void fire();

uint32_t score();
uint32_t best();
uint8_t lives();

}  // namespace invasion
