#include "invasion.h"
#include "../matrix.h"

namespace invasion {
namespace {

constexpr uint8_t COLS = 4;     // envahisseurs par ligne
constexpr uint8_t ROWS = 3;
constexpr uint8_t SPACING = 2;  // une case vide entre deux envahisseurs
constexpr uint8_t FORMATION_W = (COLS - 1) * SPACING + 1;
static_assert(FORMATION_W <= MATRIX_W, "formation trop large pour la matrice");

constexpr uint8_t START_LIVES = 3;
constexpr uint8_t MAX_BOMBS = 3;
constexpr int8_t BASE_Y = MATRIX_H - 1;  // base du canon (3 cases)
constexpr int8_t TIP_Y = MATRIX_H - 2;   // pointe du canon (1 case)

constexpr uint16_t SHOT_MS = 45;    // vitesse du tir du joueur (ms par case)
constexpr uint16_t BOMB_MS = 160;   // vitesse des bombes ennemies
constexpr uint16_t HIT_MS = 1000;
constexpr uint16_t WAVE_CLEAR_MS = 1000;
constexpr uint16_t GAME_OVER_MS = 2500;
constexpr uint16_t RENDER_MS = 16;

enum class State : uint8_t { Playing, Hit, WaveClear, GameOver };

struct Bullet {
  int8_t x, y;
  bool active;
};

bool alive[ROWS][COLS];
uint8_t aliveCount;
int8_t formationX, formationY, formationDir;
bool marchPhase;  // petite animation de "marche"

int8_t cannonX;  // centre du canon
Bullet shot;
Bullet bombs[MAX_BOMBS];

State state;
uint32_t stateSince, lastStep, lastShotMove, lastBombMove, lastRender;
uint8_t livesLeft, wave;
uint32_t scoreValue, bestValue;

int alienX(uint8_t c) { return formationX + c * SPACING; }
int alienY(uint8_t r) { return formationY + r; }

// La formation accélère quand il reste peu d'envahisseurs, et à chaque vague
uint16_t stepMs() { return max<int>(100, 250 + aliveCount * 40 - (wave - 1) * 40); }

void setState(State s) {
  state = s;
  stateSince = millis();
}

void clearBullets() {
  shot.active = false;
  for (Bullet& b : bombs) b.active = false;
}

void newWave() {
  for (auto& row : alive)
    for (bool& a : row) a = true;
  aliveCount = ROWS * COLS;
  formationX = 0;
  formationY = 0;
  formationDir = 1;
  clearBullets();
  lastStep = millis();
  setState(State::Playing);
}

void reset() {
  livesLeft = START_LIVES;
  wave = 1;
  scoreValue = 0;
  cannonX = MATRIX_W / 2;
  newWave();
}

bool hitAlienAt(int x, int y) {
  for (uint8_t r = 0; r < ROWS; r++) {
    for (uint8_t c = 0; c < COLS; c++) {
      if (alive[r][c] && alienX(c) == x && alienY(r) == y) {
        alive[r][c] = false;
        aliveCount--;
        scoreValue += 10 * (ROWS - r) * wave;  // la rangée du haut rapporte plus
        if (scoreValue > bestValue) bestValue = scoreValue;
        return true;
      }
    }
  }
  return false;
}

bool hitsCannon(int x, int y) {
  return (y == BASE_Y && abs(x - cannonX) <= 1) || (y == TIP_Y && x == cannonX);
}

void playerHit() {
  if (livesLeft) livesLeft--;
  clearBullets();
  setState(State::Hit);
}

void dropBomb() {
  Bullet* slot = nullptr;
  for (Bullet& b : bombs)
    if (!b.active) slot = &b;
  if (!slot) return;

  // Une colonne au hasard parmi celles qui ont encore des envahisseurs
  uint8_t columns[COLS], n = 0;
  for (uint8_t c = 0; c < COLS; c++)
    for (uint8_t r = 0; r < ROWS; r++)
      if (alive[r][c]) {
        columns[n++] = c;
        break;
      }
  if (!n) return;
  const uint8_t c = columns[random8(n)];

  int8_t lowest = -1;
  for (uint8_t r = 0; r < ROWS; r++)
    if (alive[r][c]) lowest = r;
  *slot = {(int8_t)alienX(c), (int8_t)(alienY(lowest) + 1), true};
}

void stepFormation() {
  int minC = COLS, maxC = -1, maxR = -1;
  for (uint8_t r = 0; r < ROWS; r++)
    for (uint8_t c = 0; c < COLS; c++)
      if (alive[r][c]) {
        minC = min<int>(minC, c);
        maxC = max<int>(maxC, c);
        maxR = max<int>(maxR, r);
      }
  if (maxC < 0) return;

  // Au bord : on descend d'une ligne et on repart dans l'autre sens
  const int left = alienX(minC) + formationDir;
  const int right = alienX(maxC) + formationDir;
  if (left < 0 || right > MATRIX_W - 1) {
    formationY++;
    formationDir = -formationDir;
  } else {
    formationX += formationDir;
  }
  marchPhase = !marchPhase;

  // Arrivés au niveau du canon : c'est perdu
  if (alienY(maxR) >= TIP_Y) {
    livesLeft = 0;
    clearBullets();
    setState(State::GameOver);
    return;
  }

  if (shot.active && hitAlienAt(shot.x, shot.y)) shot.active = false;
  if (random8() < min(200, 60 + wave * 25)) dropBomb();
}

void updatePlaying(uint32_t now) {
  if (shot.active && now - lastShotMove >= SHOT_MS) {
    lastShotMove = now;
    shot.y--;
    if (shot.y < 0 || hitAlienAt(shot.x, shot.y)) shot.active = false;
  }

  if (now - lastBombMove >= BOMB_MS) {
    lastBombMove = now;
    for (Bullet& b : bombs) {
      if (!b.active) continue;
      b.y++;
      if (b.y >= MATRIX_H) b.active = false;
    }
  }
  // Vérifié à chaque tour : le canon peut aussi se déplacer sous une bombe
  for (Bullet& b : bombs) {
    if (b.active && hitsCannon(b.x, b.y)) {
      playerHit();
      return;
    }
  }

  if (now - lastStep >= stepMs()) {
    lastStep = now;
    stepFormation();
    if (state != State::Playing) return;
  }

  if (aliveCount == 0) {
    wave++;
    clearBullets();
    setState(State::WaveClear);
  }
}

void drawCannon(const CRGB& color) {
  for (int dx = -1; dx <= 1; dx++) setPixel(cannonX + dx, BASE_Y, color);
  setPixel(cannonX, TIP_Y, color);
}

void render(uint32_t now) {
  const uint32_t elapsed = now - stateSince;
  FastLED.clear();

  if (state == State::GameOver) {
    CRGB color = CRGB::Red;
    color.nscale8_video(sin8(elapsed / 4));
    fill_solid(leds, NUM_LEDS, color);
    FastLED.show();
    return;
  }
  if (state == State::WaveClear) {
    // Balayage arc-en-ciel de bas en haut
    const int rows = elapsed * MATRIX_H / WAVE_CLEAR_MS;
    for (int i = 0; i < rows && i < MATRIX_H; i++)
      for (uint8_t x = 0; x < MATRIX_W; x++)
        leds[XY(x, MATRIX_H - 1 - i)] = CHSV(i * 25 + x * 8, 255, 120);
    FastLED.show();
    return;
  }

  for (uint8_t r = 0; r < ROWS; r++) {
    for (uint8_t c = 0; c < COLS; c++) {
      if (!alive[r][c]) continue;
      const uint8_t level = ((c + marchPhase) % 2) ? 255 : 150;
      setPixel(alienX(c), alienY(r), CHSV(160 + r * 40 + wave * 20, 255, level));
    }
  }
  for (const Bullet& b : bombs)
    if (b.active) setPixel(b.x, b.y, CRGB::Red);
  if (shot.active) setPixel(shot.x, shot.y, CRGB::Yellow);

  if (state == State::Hit) {
    if ((elapsed / 120) % 2 == 0) drawCannon(CRGB::Red);
    // Vies restantes en haut à gauche
    for (uint8_t i = 0; i < livesLeft; i++) setPixel(i * 2, 0, CRGB::Green);
  } else {
    drawCannon(CRGB::Green);
  }
  FastLED.show();
}

void update(uint32_t) {
  const uint32_t now = millis();
  const uint32_t elapsed = now - stateSince;

  switch (state) {
    case State::Playing:
      updatePlaying(now);
      break;
    case State::Hit:
      if (elapsed >= HIT_MS) {
        if (livesLeft == 0) setState(State::GameOver);
        else setState(State::Playing);
      }
      break;
    case State::WaveClear:
      if (elapsed >= WAVE_CLEAR_MS) newWave();
      break;
    case State::GameOver:
      if (elapsed >= GAME_OVER_MS) reset();
      break;
  }

  if (now - lastRender >= RENDER_MS) {
    lastRender = now;
    render(now);
  }
}

}  // namespace

void setPosition(uint16_t permille) {
  if (permille > 1000) permille = 1000;
  // Centre du canon de 0 à W-1 : la base déborde (invisible) aux bords, ce qui
  // permet de viser les colonnes extrêmes.
  cannonX = ((uint32_t)permille * (MATRIX_W - 1) + 500) / 1000;
}

void nudge(int8_t step) { cannonX = constrain(cannonX + step, 0, MATRIX_W - 1); }

void fire() {
  if (state != State::Playing || shot.active) return;  // un seul tir à la fois
  shot = {cannonX, (int8_t)(TIP_Y - 1), true};
  lastShotMove = millis();
  if (hitAlienAt(shot.x, shot.y)) shot.active = false;
}

uint32_t score() { return scoreValue; }
uint32_t best() { return bestValue; }
uint8_t lives() { return livesLeft; }

const Animation game = {
    "invasion", "Invasion", "👾", AnimType::Game, reset, update, nullptr};

}  // namespace invasion
