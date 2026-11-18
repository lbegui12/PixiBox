// Matrice LED 9x10 (WS2812B) pilotée par un ESP32.
//
//   matrix   : tableau de LEDs, conversion (x, y) -> index
//   effects/ : effets visuels         games/ : Snake, Pong
//   animator : animation courante, transitions, réglages sauvegardés
//   live     : mode Direct (caméra / dessin / photo depuis le navigateur)
//   net      : WiFi, mDNS, mise à jour OTA
//   web      : page web (HTTP) + commandes temps réel (WebSocket)

#include <Arduino.h>

#include "animator.h"
#include "live.h"
#include "matrix.h"
#include "net.h"
#include "web.h"

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LED Matrix ===");

  matrix::begin();
  live::begin();
  net::begin();
  web::begin();
  animator::begin();
}

void loop() {
  net::loop();
  web::loop();
  animator::loop();
}
