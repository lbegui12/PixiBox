#include "pong.h"
#include "../matrix.h"

namespace pong {
namespace {

constexpr uint8_t PADDLE_SIZE = 3;
constexpr uint8_t MAX_PADDLE_X = MATRIX_W - PADDLE_SIZE;
constexpr uint8_t WIN_SCORE = 5;

constexpr uint16_t TICK_MS = 40;
constexpr float START_SPEED_Y = 0.30f;  // cases par tick
constexpr float MAX_SPEED_Y = 0.70f;    // < 1 pour ne jamais "sauter" une raquette
constexpr float MAX_SPEED_X = 0.45f;
constexpr float ACCELERATION = 1.05f;   // à chaque renvoi
constexpr float SPIN = 0.12f;           // effet donné selon l'endroit de la raquette touché

constexpr uint16_t SERVE_DELAY_MS = 900;
constexpr uint16_t POINT_FLASH_MS = 400;
constexpr uint16_t POINT_PAUSE_MS = 1600;
constexpr uint16_t GAME_OVER_MS = 3000;
constexpr uint8_t AI_MOVE_EVERY = 3;  // l'ordi bouge d'une case tous les N ticks

constexpr int16_t NO_CLIENT = -1;
constexpr uint8_t TOP = 0, BOTTOM = 1;

enum class State : uint8_t { Serve, Playing, Point, GameOver };

struct Player {
  int16_t client = NO_CLIENT;  // connexion WebSocket, NO_CLIENT = ordinateur
  int8_t x = MAX_PADDLE_X / 2;  // case la plus à gauche de la raquette
  uint8_t score = 0;
  uint8_t aiCounter = 0;
  int8_t aiError = 0;  // petite erreur de visée de l'ordi, change à chaque service
};

Player players[2];
const CRGB COLORS[2] = {CRGB(CRGB::Blue), CRGB(CRGB::Green)};

float ballX, ballY, speedX, speedY;
State state;
uint32_t stateSince;
uint32_t lastTick;
uint8_t lastScorer;

Player* playerOf(uint8_t client) {
  for (Player& p : players)
    if (p.client == client) return &p;
  return nullptr;
}

void setState(State s) {
  state = s;
  stateSince = millis();
}

void serve(uint8_t towards) {
  ballX = MATRIX_W / 2;
  ballY = (MATRIX_H - 1) / 2.0f;
  speedY = (towards == TOP) ? -START_SPEED_Y : START_SPEED_Y;
  speedX = (random8(2) ? 1 : -1) * (0.10f + random8(15) / 100.0f);
  for (Player& p : players) p.aiError = (int8_t)random8(3) - 1;
  setState(State::Serve);
}

void reset() {
  for (Player& p : players) {
    p.score = 0;
    p.x = MAX_PADDLE_X / 2;
  }
  lastTick = millis();
  serve(random8(2));
}

void runAI() {
  for (Player& p : players) {
    if (p.client != NO_CLIENT) continue;
    if (++p.aiCounter < AI_MOVE_EVERY) continue;
    p.aiCounter = 0;
    const int target = constrain((int)lroundf(ballX) - 1 + p.aiError, 0, MAX_PADDLE_X);
    if (p.x < target) p.x++;
    else if (p.x > target) p.x--;
  }
}

bool covers(const Player& p, int col) {
  return col >= p.x && col < p.x + PADDLE_SIZE;
}

void bounce(Player& p, int col) {
  const int offset = col - (p.x + PADDLE_SIZE / 2);  // -1 gauche, 0 centre, +1 droite
  speedX = constrain(speedX + offset * SPIN, -MAX_SPEED_X, MAX_SPEED_X);
  speedY = constrain(-speedY * ACCELERATION, -MAX_SPEED_Y, MAX_SPEED_Y);
}

void scorePoint(uint8_t scorer) {
  players[scorer].score++;
  lastScorer = scorer;
  setState(State::Point);
}

void physics() {
  const float prevY = ballY;
  ballX += speedX;
  ballY += speedY;

  // Murs gauche / droite : rebond miroir
  if (ballX < 0) {
    ballX = -ballX;
    speedX = -speedX;
  } else if (ballX > MATRIX_W - 1) {
    ballX = 2 * (MATRIX_W - 1) - ballX;
    speedX = -speedX;
  }
  const int col = constrain((int)lroundf(ballX), 0, MATRIX_W - 1);

  // Raquette du haut (ligne 0) : la balle rebondit en franchissant y = 1
  const float topLine = 1.0f;
  if (speedY < 0 && prevY > topLine && ballY <= topLine && covers(players[TOP], col)) {
    ballY = 2 * topLine - ballY;
    bounce(players[TOP], col);
  }
  // Raquette du bas (ligne H-1) : rebond en franchissant y = H-2
  const float bottomLine = MATRIX_H - 2.0f;
  if (speedY > 0 && prevY < bottomLine && ballY >= bottomLine && covers(players[BOTTOM], col)) {
    ballY = 2 * bottomLine - ballY;
    bounce(players[BOTTOM], col);
  }

  // Point marqué quand la balle sort complètement (symétrique haut / bas)
  if (ballY < -0.5f) scorePoint(BOTTOM);
  else if (ballY > MATRIX_H - 0.5f) scorePoint(TOP);
}

void drawPaddles() {
  for (uint8_t i = 0; i < PADDLE_SIZE; i++) {
    setPixel(players[TOP].x + i, 0, COLORS[TOP]);
    setPixel(players[BOTTOM].x + i, MATRIX_H - 1, COLORS[BOTTOM]);
  }
}

void renderGame() {
  FastLED.clear();
  drawPaddles();
  setPixel(lroundf(ballX), lroundf(ballY), CRGB::White);
  FastLED.show();
}

void renderPoint(uint32_t elapsed) {
  FastLED.clear();
  if (elapsed < POINT_FLASH_MS) {
    // Flash de la couleur du joueur qui marque
    if ((elapsed / 100) % 2 == 0) fill_solid(leds, NUM_LEDS, COLORS[lastScorer]);
  } else {
    // Affichage du score : un point par manche gagnée
    drawPaddles();
    for (uint8_t i = 0; i < players[TOP].score && i < MATRIX_W; i++)
      setPixel(i, MATRIX_H / 2 - 2, COLORS[TOP]);
    for (uint8_t i = 0; i < players[BOTTOM].score && i < MATRIX_W; i++)
      setPixel(i, MATRIX_H / 2 + 1, COLORS[BOTTOM]);
  }
  FastLED.show();
}

void renderGameOver(uint32_t elapsed) {
  CRGB color = COLORS[lastScorer];
  color.nscale8_video(sin8(elapsed / 4));  // "respiration" de la couleur gagnante
  fill_solid(leds, NUM_LEDS, color);
  FastLED.show();
}

void update(uint32_t) {
  const uint32_t now = millis();
  if (now - lastTick < TICK_MS) return;
  lastTick = now;

  runAI();
  const uint32_t elapsed = now - stateSince;

  switch (state) {
    case State::Serve:
      renderGame();
      if (elapsed >= SERVE_DELAY_MS) setState(State::Playing);
      break;

    case State::Playing:
      physics();
      if (state == State::Playing) renderGame();
      else renderPoint(0);
      break;

    case State::Point:
      renderPoint(elapsed);
      if (elapsed >= POINT_PAUSE_MS) {
        if (players[lastScorer].score >= WIN_SCORE) setState(State::GameOver);
        else serve(lastScorer == TOP ? BOTTOM : TOP);  // service vers celui qui a perdu le point
      }
      break;

    case State::GameOver:
      renderGameOver(elapsed);
      if (elapsed >= GAME_OVER_MS) reset();
      break;
  }
}

}  // namespace

uint8_t join(uint8_t client) {
  for (uint8_t i = 0; i < 2; i++)
    if (players[i].client == client) return i + 1;  // déjà inscrit
  for (uint8_t i = 0; i < 2; i++) {
    if (players[i].client == NO_CLIENT) {
      players[i].client = client;
      return i + 1;
    }
  }
  return 0;
}

void leave(uint8_t client) {
  if (Player* p = playerOf(client)) p->client = NO_CLIENT;
}

void releaseAll() {
  for (Player& p : players) p.client = NO_CLIENT;
}

void setPaddle(uint8_t client, uint16_t permille) {
  Player* p = playerOf(client);
  if (!p) return;
  if (permille > 1000) permille = 1000;
  p->x = ((uint32_t)permille * MAX_PADDLE_X + 500) / 1000;
}

void nudgePaddle(uint8_t client, int8_t step) {
  Player* p = playerOf(client);
  if (!p) return;
  p->x = constrain(p->x + step, 0, MAX_PADDLE_X);
}

uint8_t score(uint8_t player) {
  return (player == 1 || player == 2) ? players[player - 1].score : 0;
}

bool isHuman(uint8_t player) {
  return (player == 1 || player == 2) && players[player - 1].client != NO_CLIENT;
}

const Animation game = {
    "pong", "Pong", "🏓", AnimType::Game, reset, update, releaseAll};

}  // namespace pong
