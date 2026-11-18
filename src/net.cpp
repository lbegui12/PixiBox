#include "net.h"

#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#include "matrix.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copie include/secrets.example.h en include/secrets.h et mets-y tes identifiants WiFi"
#endif

namespace net {
namespace {

// Petit point qui tourne autour de la matrice pendant la connexion
void drawSpinner(uint32_t step) {
  constexpr uint16_t PERIMETER = 2 * (MATRIX_W + MATRIX_H) - 4;
  uint16_t i = step % PERIMETER;
  int x, y;
  if (i < MATRIX_W) { x = i; y = 0; }
  else if ((i -= MATRIX_W) < MATRIX_H - 1) { x = MATRIX_W - 1; y = i + 1; }
  else if ((i -= MATRIX_H - 1) < MATRIX_W - 1) { x = MATRIX_W - 2 - i; y = MATRIX_H - 1; }
  else { i -= MATRIX_W - 1; x = 0; y = MATRIX_H - 2 - i; }

  fadeToBlackBy(leds, NUM_LEDS, 70);
  setPixel(x, y, CRGB::Blue);
  FastLED.show();
}

void flash(const CRGB& color) {
  fill_solid(leds, NUM_LEDS, color);
  FastLED.show();
  delay(250);
  FastLED.clear(true);
}

bool connectStation() {
  WiFi.setHostname(HOSTNAME);  // avant WiFi.mode() sinon ignoré sur certains cores
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.printf("[wifi] connexion à \"%s\"", WIFI_SSID);
  const uint32_t start = millis();
  uint32_t step = 0;
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
      Serial.println(" échec");
      return false;
    }
    drawSpinner(step++);
    if (step % 10 == 0) Serial.print('.');
    delay(50);
  }
  Serial.printf(" OK, IP : %s\n", WiFi.localIP().toString().c_str());
  return true;
}

void startAccessPoint() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(FALLBACK_AP_SSID, FALLBACK_AP_PASSWORD);
  Serial.printf("[wifi] point d'accès \"%s\" (mdp \"%s\"), interface : http://%s\n",
                FALLBACK_AP_SSID, FALLBACK_AP_PASSWORD, WiFi.softAPIP().toString().c_str());
}

void setupOta() {
  ArduinoOTA.setHostname(HOSTNAME);
  if (strlen(OTA_PASSWORD) > 0) ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    Serial.println("[ota] début de la mise à jour");
    FastLED.clear(true);
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    matrix::drawProgress(total ? (uint32_t)done * 1000 / total : 0, CRGB::DarkCyan);
    FastLED.show();
  });
  ArduinoOTA.onEnd([]() { Serial.println("[ota] terminé, redémarrage"); });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[ota] erreur %u\n", error);
    flash(CRGB::Red);
  });

  // ArduinoOTA démarre aussi le mDNS avec HOSTNAME
  ArduinoOTA.begin();
}

}  // namespace

void begin() {
  if (connectStation()) {
    WiFi.setSleep(false);  // latence minimale (le gain d'énergie est négligeable face aux LEDs)
    flash(CRGB::Green);
  } else {
    startAccessPoint();
    flash(CRGB::Orange);
  }

  setupOta();
  MDNS.addService("http", "tcp", HTTP_PORT);
  Serial.printf("[wifi] interface : http://%s.local\n", HOSTNAME);
}

void loop() { ArduinoOTA.handle(); }

}  // namespace net
