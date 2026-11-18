#pragma once
// Mode "Direct" : la matrice affiche des images envoyées par le navigateur
// (caméra, dessin, photo).
#include <stddef.h>
#include <stdint.h>
#include "config.h"

namespace live {

constexpr size_t FRAME_SIZE = NUM_LEDS * 3;

void begin();

// `rgb` = NUM_LEDS triplets R,G,B en ordre de lecture (ligne par ligne,
// de gauche à droite, en commençant en haut). Renvoie false si la taille est
// incorrecte.
bool showFrame(const uint8_t* rgb, size_t length);

}  // namespace live
