#pragma once
// Serveur HTTP (fichiers de l'interface, port 80) + WebSocket (commandes
// temps réel, port 81).
//
// Protocole WebSocket
// -------------------
// Navigateur -> ESP32, messages texte :
//   set <index>            choisit une animation
//   speed <ms>             vitesse des effets
//   bright <0-255>         luminosité
//   snake <u|d|l|r>        direction du serpent
//   pong join | leave      prendre / libérer une raquette
//   pong pos <0-1000>      position de la raquette (0 = gauche)
//   pong nudge <-1|1>      déplacer la raquette d'une case
//   blocks <l|r|rot|down|drop>   Blocs : gauche, droite, rotation, descendre, lâcher
//   inv pos <0-1000>       Invasion : position du canon
//   inv nudge <-1|1>       Invasion : déplacer le canon d'une case
//   inv fire               Invasion : tirer
//   flyer flap             Envol : coup d'aile (démarre la partie)
//   live on | off          mode Direct
// Navigateur -> ESP32, message binaire :
//   NUM_LEDS * 3 octets RGB en ordre de lecture (mode Direct uniquement)
//
// ESP32 -> navigateur (JSON, champ "t" = type) :
//   {"t":"list","items":[{"id":0,"key":"rain","name":"Pluie","icon":"🌧️","type":"effect"},...]}
//   {"t":"state","anim":0,"live":false,"speed":50,"bright":100}
//   {"t":"snake","len":5,"best":12}
//   {"t":"pong","s1":2,"s2":1,"h1":true,"h2":false}
//   {"t":"pongJoin","player":1}          (0 = partie complète)
//   {"t":"score","key":"blocks","score":1200,"best":3400,"extra":2}
//        extra = niveau (blocks), vies (invasion), 0 (flyer)

namespace web {

void begin();
void loop();

}  // namespace web
