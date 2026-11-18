#include "snake.h"
#include "../matrix.h"

namespace snake {
namespace {

constexpr uint8_t START_LENGTH = 3;
constexpr uint16_t START_TICK_MS = 100;  // délai entre deux déplacements
constexpr uint16_t MIN_TICK_MS = 50;
constexpr uint16_t TICK_DECREMENT_MS = 3;  // accélération à chaque pomme
constexpr uint16_t DEATH_ANIM_MS = 900;
constexpr uint16_t WIN_ANIM_MS = 3000;

struct Cell {
  int8_t x, y;
  bool operator==(const Cell& o) const { return x == o.x && y == o.y; }
};

enum class State : uint8_t { Playing, Dead, Won };

Cell body[NUM_LEDS];  // body[0] = tête
uint8_t len;
Dir dir;
Dir queue[2];
uint8_t queued;
Cell apple;
uint16_t tickMs;
uint32_t lastTick;
State state;
uint32_t stateSince;
uint8_t hue;
uint8_t bestLen = START_LENGTH;

bool opposite(Dir a, Dir b) {
  return (a == Dir::Up && b == Dir::Down) || (a == Dir::Down && b == Dir::Up) ||
         (a == Dir::Left && b == Dir::Right) || (a == Dir::Right && b == Dir::Left);
}

Cell moved(Cell c, Dir d) {
  switch (d) {
    case Dir::Up:    c.y--; break;
    case Dir::Down:  c.y++; break;
    case Dir::Left:  c.x--; break;
    case Dir::Right: c.x++; break;
  }
  // Les bords se rejoignent
  if (c.x < 0) c.x = MATRIX_W - 1; else if (c.x >= MATRIX_W) c.x = 0;
  if (c.y < 0) c.y = MATRIX_H - 1; else if (c.y >= MATRIX_H) c.y = 0;
  return c;
}

// Place la pomme sur une case libre tirée au hasard.
// Renvoie false si le serpent occupe toute la matrice (partie gagnée).
bool placeApple() {
  bool occupied[MATRIX_H][MATRIX_W] = {};
  for (uint8_t i = 0; i < len; i++) occupied[body[i].y][body[i].x] = true;

  const uint16_t freeCells = NUM_LEDS - len;
  if (freeCells == 0) return false;

  uint16_t k = random16(freeCells);  // on prend la k-ième case libre
  for (int8_t y = 0; y < MATRIX_H; y++) {
    for (int8_t x = 0; x < MATRIX_W; x++) {
      if (occupied[y][x]) continue;
      if (k-- == 0) {
        apple = {x, y};
        return true;
      }
    }
  }
  return false;
}

void reset() {
  len = START_LENGTH;
  for (uint8_t i = 0; i < len; i++) {
    body[i] = {(int8_t)(MATRIX_W / 2 - i), (int8_t)(MATRIX_H / 2)};
  }
  dir = Dir::Right;
  queued = 0;
  tickMs = START_TICK_MS;
  lastTick = millis();
  state = State::Playing;
  hue = random8();
  placeApple();
}

void render() {
  FastLED.clear();
  leds[XY(apple.x, apple.y)] = CRGB::Red;
  for (uint8_t i = 0; i < len; i++) {
    // Dégradé de luminosité de la tête (255) vers la queue (60)
    const uint8_t level = (len > 1) ? 255 - (uint16_t)i * 195 / (len - 1) : 255;
    leds[XY(body[i].x, body[i].y)] = CHSV(hue, 255, level);
  }
  FastLED.show();
}

void renderDeath(uint32_t elapsed) {
  // Le serpent clignote en rouge
  const bool on = (elapsed / 150) % 2 == 0;
  FastLED.clear();
  if (on) {
    for (uint8_t i = 0; i < len; i++) leds[XY(body[i].x, body[i].y)] = CRGB::Red;
  }
  FastLED.show();
}

void renderWin(uint32_t elapsed) {
  for (uint8_t y = 0; y < MATRIX_H; y++)
    for (uint8_t x = 0; x < MATRIX_W; x++)
      leds[XY(x, y)] = CHSV(elapsed / 8 + (x + y) * 16, 255, 255);
  FastLED.show();
}

void update(uint32_t) {
  const uint32_t now = millis();

  if (state != State::Playing) {
    const uint32_t elapsed = now - stateSince;
    const uint32_t duration = (state == State::Dead) ? DEATH_ANIM_MS : WIN_ANIM_MS;
    if (elapsed >= duration) {
      reset();
      return;
    }
    // ~30 images/s suffisent pour ces animations
    if (now - lastTick < 33) return;
    lastTick = now;
    (state == State::Dead) ? renderDeath(elapsed) : renderWin(elapsed);
    return;
  }

  if (now - lastTick < tickMs) return;
  lastTick = now;

  if (queued) {
    dir = queue[0];
    queue[0] = queue[1];
    queued--;
  }

  const Cell head = moved(body[0], dir);
  const bool eats = (head == apple);

  // Collision avec le corps. Si on ne mange pas, la queue va se libérer
  // pendant ce déplacement : on peut donc avancer sur sa case.
  const uint8_t checked = eats ? len : len - 1;
  for (uint8_t i = 0; i < checked; i++) {
    if (body[i] == head) {
      state = State::Dead;
      stateSince = now;
      return;
    }
  }

  if (eats && len < NUM_LEDS) len++;
  for (uint8_t i = len - 1; i > 0; i--) body[i] = body[i - 1];
  body[0] = head;

  if (eats) {
    hue += 25;
    if (len > bestLen) bestLen = len;
    tickMs = max<int>(MIN_TICK_MS, tickMs - TICK_DECREMENT_MS);
    if (!placeApple()) {  // plus aucune case libre : gagné !
      state = State::Won;
      stateSince = now;
      return;
    }
  }

  render();
}

}  // namespace

void queueDirection(Dir d) {
  if (queued >= 2) return;
  const Dir last = queued ? queue[queued - 1] : dir;
  if (d == last || opposite(d, last)) return;
  queue[queued++] = d;
}

uint8_t length() { return len; }
uint8_t best() { return bestLen; }

const Animation game = {
    "snake", "Snake", "🐍", AnimType::Game, reset, update, nullptr};

}  // namespace snake
