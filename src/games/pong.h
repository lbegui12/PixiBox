#pragma once
#include "../animation.h"

// Pong à 2 joueurs. Joueur 1 = raquette du haut (bleue), joueur 2 = raquette
// du bas (verte). Une raquette sans joueur est pilotée par l'ordinateur : on
// peut donc jouer seul, et sans joueur le jeu tourne en démo.
namespace pong {

extern const Animation game;

// `client` = identifiant de la connexion WebSocket.
// join() renvoie 1 ou 2, ou 0 si les deux places sont prises.
uint8_t join(uint8_t client);
void leave(uint8_t client);
void releaseAll();

void setPaddle(uint8_t client, uint16_t permille);  // 0 = gauche, 1000 = droite
void nudgePaddle(uint8_t client, int8_t step);      // -1 / +1 case (clavier)

uint8_t score(uint8_t player);   // player = 1 ou 2
bool isHuman(uint8_t player);

}  // namespace pong
