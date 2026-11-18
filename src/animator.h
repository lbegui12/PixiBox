#pragma once
// Chef d'orchestre : choisit l'animation courante, gère la transition entre
// deux animations, le mode Direct et les réglages (sauvegardés en flash).
#include <stddef.h>
#include <stdint.h>
#include "animation.h"

namespace animator {

void begin();  // charge les réglages sauvegardés et lance la dernière animation
void loop();

size_t count();
const Animation& at(size_t index);

int current();                // index de l'animation courante (ou à venir)
bool select(int index);       // false si l'index est invalide

bool isLive();
void enterLive();
void exitLive();              // reprend l'animation courante

uint16_t speed();
void setSpeed(int ms);
uint8_t brightness();
void setBrightness(int value);

// Incrémenté à chaque changement d'état : l'interface web s'en sert pour
// savoir quand prévenir les navigateurs connectés.
uint32_t version();

}  // namespace animator
