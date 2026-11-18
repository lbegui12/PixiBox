#pragma once
#include <stdint.h>

// =====================================================================
//  MATÉRIEL
// =====================================================================

// Broche data des LEDs.
// Note : GPIO2 est une broche de "strapping" (et la LED bleue de beaucoup de
// cartes). Ça marche en général, mais si le flash USB échoue quand la matrice
// est branchée, passe sur une broche neutre (GPIO 16, 17, 18, 19, 23...).
#define LED_PIN 2
#define LED_TYPE WS2812B
#define LED_COLOR_ORDER GRB

constexpr uint8_t MATRIX_W = 9;    // nombre de colonnes (x)
constexpr uint8_t MATRIX_H = 10;   // nombre de lignes   (y), y = 0 en haut
constexpr uint16_t NUM_LEDS = MATRIX_W * MATRIX_H;

// Limite de consommation : FastLED baisse automatiquement la luminosité si
// l'image demandée dépasse ce budget (à adapter à ton alimentation).
constexpr uint8_t LED_VOLTS = 5;
constexpr uint32_t LED_MAX_MILLIAMPS = 2000;

// =====================================================================
//  RÉGLAGES PAR DÉFAUT (modifiables depuis l'interface, sauvegardés en flash)
// =====================================================================
constexpr uint8_t DEFAULT_BRIGHTNESS = 100;
constexpr uint16_t DEFAULT_EFFECT_SPEED_MS = 50;
constexpr uint16_t MIN_EFFECT_SPEED_MS = 10;
constexpr uint16_t MAX_EFFECT_SPEED_MS = 500;

// =====================================================================
//  RÉSEAU
// =====================================================================
#define HOSTNAME "esp32"            // => http://esp32.local
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;

// Si le WiFi n'est pas joignable au démarrage, l'ESP crée son propre réseau
// (interface accessible sur http://192.168.4.1).
#define FALLBACK_AP_SSID "LedMatrix"
#define FALLBACK_AP_PASSWORD "ledmatrix"   // 8 caractères minimum

constexpr uint16_t HTTP_PORT = 80;
constexpr uint16_t WS_PORT = 81;
