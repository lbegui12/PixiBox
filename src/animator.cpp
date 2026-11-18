#include "animator.h"

#include <Preferences.h>

#include "effects/effects.h"
#include "games/blocks.h"
#include "games/flyer.h"
#include "games/invasion.h"
#include "games/pong.h"
#include "games/snake.h"
#include "matrix.h"

namespace animator {
namespace {

// -------------------------------------------------------------------------
//  Table des animations : l'ordre ici est l'ordre dans l'interface web.
// -------------------------------------------------------------------------
const Animation* const ANIMATIONS[] = {
    // Effets
    &effects::sparkles,
    &effects::rain,
    &effects::diagUp,
    &effects::diagDown,
    &effects::tunnel,
    &effects::ripple,
    &effects::waves,
    &effects::fire,
    &effects::life,
    // Jeux
    &snake::game,
    &pong::game,
    &blocks::game,
    &invasion::game,
    &flyer::game,
};
constexpr int ANIMATION_COUNT = sizeof(ANIMATIONS) / sizeof(ANIMATIONS[0]);

// Transition (flash rouge / bleu / vert) jouée sans bloquer la boucle
const CRGB TRANSITION_COLORS[] = {CRGB(CRGB::Red), CRGB(CRGB::Blue), CRGB(CRGB::Green)};
constexpr uint8_t TRANSITION_STEPS = sizeof(TRANSITION_COLORS) / sizeof(TRANSITION_COLORS[0]);
constexpr uint16_t TRANSITION_STEP_MS = 120;

constexpr uint32_t SAVE_DELAY_MS = 3000;  // regroupe les écritures en flash

int currentIndex = 0;
bool transitioning = false;
uint32_t transitionStart = 0;
int8_t transitionStep = -1;

bool liveMode = false;

uint32_t frame = 0;
uint32_t lastFrame = 0;

uint16_t speedMs = DEFAULT_EFFECT_SPEED_MS;
uint8_t brightnessValue = DEFAULT_BRIGHTNESS;

uint32_t stateVersion = 0;
bool settingsDirty = false;
uint32_t settingsChangedAt = 0;

Preferences prefs;

const Animation& anim() { return *ANIMATIONS[currentIndex]; }

void changed(bool persist) {
  stateVersion++;
  if (persist) {
    settingsDirty = true;
    settingsChangedAt = millis();
  }
}

void startCurrent() {
  frame = 0;
  lastFrame = millis();
  FastLED.clear(true);
  if (anim().begin) anim().begin();
}

void loadSettings() {
  prefs.begin("ledmatrix", false);  // lecture/écriture : crée l'espace au 1er démarrage
  speedMs = constrain(prefs.getUShort("speed", DEFAULT_EFFECT_SPEED_MS),
                      MIN_EFFECT_SPEED_MS, MAX_EFFECT_SPEED_MS);
  brightnessValue = prefs.getUChar("bright", DEFAULT_BRIGHTNESS);
  const String savedId = prefs.getString("anim", "");
  prefs.end();

  for (int i = 0; i < ANIMATION_COUNT; i++) {
    if (savedId == ANIMATIONS[i]->id) currentIndex = i;
  }
}

void saveSettings() {
  prefs.begin("ledmatrix", false);
  prefs.putUShort("speed", speedMs);
  prefs.putUChar("bright", brightnessValue);
  prefs.putString("anim", anim().id);
  prefs.end();
  settingsDirty = false;
  Serial.println("[réglages] sauvegardés");
}

void runTransition(uint32_t now) {
  const int8_t step = (now - transitionStart) / TRANSITION_STEP_MS;
  if (step == transitionStep) return;  // rien de nouveau à afficher
  transitionStep = step;

  if (step < TRANSITION_STEPS) {
    fill_solid(leds, NUM_LEDS, TRANSITION_COLORS[step]);
    FastLED.show();
  } else {
    transitioning = false;
    startCurrent();
  }
}

}  // namespace

void begin() {
  loadSettings();
  FastLED.setBrightness(brightnessValue);
  Serial.printf("[anim] démarrage sur \"%s\"\n", anim().id);
  startCurrent();
}

void loop() {
  const uint32_t now = millis();

  if (settingsDirty && now - settingsChangedAt >= SAVE_DELAY_MS) saveSettings();

  if (liveMode) return;  // la matrice est pilotée par le navigateur

  if (transitioning) {
    runTransition(now);
    return;
  }

  if (anim().type == AnimType::Game) {
    anim().update(frame++);  // les jeux gèrent leur propre rythme
  } else if (now - lastFrame >= speedMs) {
    lastFrame = now;
    anim().update(frame++);
  }
}

size_t count() { return ANIMATION_COUNT; }
const Animation& at(size_t index) { return *ANIMATIONS[index]; }
int current() { return currentIndex; }

bool select(int index) {
  if (index < 0 || index >= ANIMATION_COUNT) return false;

  const bool wasLive = liveMode;
  liveMode = false;

  // Re-sélectionner l'animation en cours (ex : 2e joueur qui rejoint Pong)
  // ne doit pas la relancer.
  if (index == currentIndex && !wasLive) return true;

  if (!transitioning && !wasLive && anim().end) anim().end();
  currentIndex = index;
  transitioning = true;
  transitionStart = millis();
  transitionStep = -1;

  Serial.printf("[anim] -> \"%s\"\n", anim().id);
  changed(true);
  return true;
}

bool isLive() { return liveMode; }

void enterLive() {
  if (liveMode) return;
  if (!transitioning && anim().end) anim().end();
  transitioning = false;
  liveMode = true;
  FastLED.clear(true);
  Serial.println("[anim] mode direct");
  changed(false);
}

void exitLive() {
  if (!liveMode) return;
  liveMode = false;
  startCurrent();
  Serial.println("[anim] fin du mode direct");
  changed(false);
}

uint16_t speed() { return speedMs; }

void setSpeed(int ms) {
  const uint16_t value = constrain(ms, MIN_EFFECT_SPEED_MS, MAX_EFFECT_SPEED_MS);
  if (value == speedMs) return;
  speedMs = value;
  changed(true);
}

uint8_t brightness() { return brightnessValue; }

void setBrightness(int value) {
  const uint8_t v = constrain(value, 0, 255);
  if (v == brightnessValue) return;
  brightnessValue = v;
  FastLED.setBrightness(v);
  FastLED.show();  // effet immédiat, même si l'effet est lent
  changed(true);
}

uint32_t version() { return stateVersion; }

}  // namespace animator
