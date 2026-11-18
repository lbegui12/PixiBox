#include "flyer.h"
#include "../matrix.h"

namespace flyer {
namespace {

constexpr uint8_t BIRD_X = 2;
constexpr uint16_t TICK_MS = 30;
constexpr float GRAVITY = 0.028f;      // cases / tick²
constexpr float FLAP_SPEED = -0.34f;   // ~2 cases de remontée par coup d'aile
constexpr float MAX_FALL_SPEED = 0.35f;

constexpr uint16_t START_SCROLL_MS = 200;  // défilement des colonnes
constexpr uint16_t MIN_SCROLL_MS = 100;
constexpr uint8_t SCROLL_STEP_MS = 5;      // accélération par point
constexpr uint8_t PIPE_SPACING = 5;        // colonnes entre deux obstacles
constexpr uint8_t MAX_PIPES = 3;
constexpr uint8_t NARROW_AFTER = 10;       // le trou rétrécit après 10 points
constexpr uint8_t MAX_GAP_JUMP = 3;        // écart max entre deux trous successifs
constexpr uint16_t DEAD_MS = 1500;

enum class State : uint8_t { Ready, Playing, Dead };

struct Pipe {
  int8_t x;
  int8_t gapY;   // première case libre
  uint8_t gapH;  // hauteur du trou
  bool scored;
  bool active;
};

Pipe pipes[MAX_PIPES];
float birdY, speedY;
State state;
uint32_t stateSince, lastTick, lastScroll;
uint8_t untilNextPipe;
int8_t lastGapY;
uint32_t scoreValue, bestValue;

uint16_t scrollMs() {
  return max<int>(MIN_SCROLL_MS, START_SCROLL_MS - (int)scoreValue * SCROLL_STEP_MS);
}

void setState(State s) {
  state = s;
  stateSince = millis();
}

void reset() {
  for (Pipe& p : pipes) p.active = false;
  birdY = MATRIX_H / 2 - 1;
  speedY = 0;
  lastGapY = 3;
  setState(State::Ready);
}

void spawnPipe() {
  for (Pipe& p : pipes) {
    if (p.active) continue;
    p.gapH = scoreValue < NARROW_AFTER ? 4 : 3;
    // Le trou ne saute pas trop loin du précédent pour rester atteignable
    const int hi = min<int>(MATRIX_H - 1 - p.gapH, lastGapY + MAX_GAP_JUMP);
    const int lo = min<int>(hi, max<int>(1, lastGapY - MAX_GAP_JUMP));
    p.gapY = lo + random8(hi - lo + 1);
    p.x = MATRIX_W - 1;
    p.scored = false;
    p.active = true;
    lastGapY = p.gapY;
    return;
  }
}

int birdRow() { return constrain((int)lroundf(birdY), 0, MATRIX_H - 1); }

bool collides() {
  const int y = birdRow();
  for (const Pipe& p : pipes) {
    if (p.active && p.x == BIRD_X && (y < p.gapY || y >= p.gapY + p.gapH)) return true;
  }
  return false;
}

void die() { setState(State::Dead); }

void scroll() {
  for (Pipe& p : pipes) {
    if (!p.active) continue;
    p.x--;
    if (p.x < 0) p.active = false;
    if (p.active && !p.scored && p.x < BIRD_X) {
      p.scored = true;
      scoreValue++;
      if (scoreValue > bestValue) bestValue = scoreValue;
    }
  }
  if (untilNextPipe == 0) {
    spawnPipe();
    untilNextPipe = PIPE_SPACING;
  }
  untilNextPipe--;
}

void render(uint32_t now) {
  FastLED.clear();
  for (const Pipe& p : pipes) {
    if (!p.active) continue;
    for (int y = 0; y < MATRIX_H; y++) {
      if (y < p.gapY || y >= p.gapY + p.gapH) setPixel(p.x, y, CRGB(0, 170, 30));
    }
  }

  CRGB bird = CRGB::Yellow;
  if (state == State::Dead) {
    bird = ((now - stateSince) / 120) % 2 ? CRGB::Black : CRGB::Red;
  }
  setPixel(BIRD_X, birdRow(), bird);
  FastLED.show();
}

void update(uint32_t) {
  const uint32_t now = millis();
  if (now - lastTick < TICK_MS) return;
  lastTick = now;

  switch (state) {
    case State::Ready:
      // L'oiseau flotte en attendant le premier appui
      birdY = MATRIX_H / 2 - 1 + ((int)sin8(now / 6) - 128) / 200.0f;
      break;

    case State::Playing:
      speedY = min(speedY + GRAVITY, MAX_FALL_SPEED);
      birdY += speedY;
      if (birdY < 0) {  // plafond : on bute dessus sans mourir
        birdY = 0;
        speedY = 0;
      }
      if (birdY > MATRIX_H - 0.5f) {  // sol
        die();
        break;
      }
      if (now - lastScroll >= scrollMs()) {
        lastScroll = now;
        scroll();
      }
      if (collides()) die();
      break;

    case State::Dead:
      if (now - stateSince >= DEAD_MS) reset();
      break;
  }

  render(now);
}

}  // namespace

void flap() {
  if (state == State::Ready) {
    scoreValue = 0;
    untilNextPipe = 3;
    lastScroll = millis();
    setState(State::Playing);
  }
  if (state == State::Playing) speedY = FLAP_SPEED;
}

uint32_t score() { return scoreValue; }
uint32_t best() { return bestValue; }

const Animation game = {
    "flyer", "Envol", "🐦", AnimType::Game, reset, update, nullptr};

}  // namespace flyer
