#pragma once
// Interface commune à tous les effets et jeux.
//
// Pour ajouter une animation :
//   1. crée un fichier dans src/effects/ (ou src/games/)
//   2. définis un `const Animation monEffet = {...}`
//   3. déclare-le dans effects.h et ajoute-le à la table de animator.cpp
// L'interface web le récupère automatiquement.

#include <stdint.h>

enum class AnimType : uint8_t { Effect, Game };

struct Animation {
  const char* id;      // identifiant stable (sauvegarde des réglages, logs)
  const char* label;   // nom affiché dans l'interface
  const char* icon;    // emoji affiché dans l'interface
  AnimType type;

  // Appelée à chaque fois que l'animation démarre : remet l'état à zéro.
  // Peut être nullptr.
  void (*begin)();

  // Effets : appelée toutes les `vitesse` ms, avec un compteur d'images.
  // Jeux   : appelée à chaque tour de loop() ; le jeu gère son propre rythme.
  void (*update)(uint32_t frame);

  // Appelée quand on quitte l'animation. Peut être nullptr.
  void (*end)();
};
