#pragma once
// Connexion WiFi (avec point d'accès de secours), mDNS et mise à jour OTA.

namespace net {

void begin();  // bloquant jusqu'à la connexion ou au délai max (animation d'attente)
void loop();

}  // namespace net
