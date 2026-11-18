#include "blocks.h"
#include "../matrix.h"

namespace blocks {
namespace {

constexpr uint16_t START_DROP_MS = 800;  // délai de chute au niveau 1
constexpr uint16_t MIN_DROP_MS = 120;
constexpr uint16_t DROP_STEP_MS = 70;    // accélération par niveau
constexpr uint8_t LINES_PER_LEVEL = 5;
constexpr uint16_t CLEAR_ANIM_MS = 350;
constexpr uint16_t GAME_OVER_MS = 2500;
constexpr uint16_t RENDER_MS = 16;
constexpr uint16_t LINE_POINTS[5] = {0, 100, 300, 500, 800};  // × niveau

struct Cell {
  int8_t x, y;
};

// Les 7 pièces de 4 cases, dans une boîte de `size` × `size`
struct PieceDef {
  uint8_t size;
  Cell cells[4];
};
const PieceDef PIECES[] = {
    {4, {{0, 1}, {1, 1}, {2, 1}, {3, 1}}},  // barre
    {2, {{0, 0}, {1, 0}, {0, 1}, {1, 1}}},  // carré
    {3, {{1, 0}, {0, 1}, {1, 1}, {2, 1}}},  // T
    {3, {{1, 0}, {2, 0}, {0, 1}, {1, 1}}},  // S
    {3, {{0, 0}, {1, 0}, {1, 1}, {2, 1}}},  // Z
    {3, {{0, 0}, {0, 1}, {1, 1}, {2, 1}}},  // J
    {3, {{2, 0}, {0, 1}, {1, 1}, {2, 1}}},  // L
};
constexpr uint8_t PIECE_COUNT = sizeof(PIECES) / sizeof(PIECES[0]);
constexpr uint8_t SQUARE = 1;

enum class State : uint8_t { Playing, Clearing, GameOver };

struct Piece {
  uint8_t type;
  Cell cells[4];  // relatives à (x, y)
  int8_t x, y;
};

uint8_t board[MATRIX_H][MATRIX_W];  // 0 = vide, sinon 1 + type de pièce
Piece cur;
uint8_t bag[PIECE_COUNT];           // tirage "sac" : les 7 pièces dans le désordre
uint8_t bagLeft;
bool fullRow[MATRIX_H];

State state;
uint32_t stateSince, lastDrop, lastRender;
bool dirty;

uint32_t scoreValue, bestValue;
uint16_t lines;

CRGB colorOf(uint8_t type) { return CHSV(type * (256 / PIECE_COUNT), 255, 255); }

uint8_t currentLevel() { return 1 + lines / LINES_PER_LEVEL; }

uint16_t dropMs() {
  return max<int>(MIN_DROP_MS, START_DROP_MS - (currentLevel() - 1) * DROP_STEP_MS);
}

void addScore(uint32_t points) {
  scoreValue += points;
  if (scoreValue > bestValue) bestValue = scoreValue;
}

uint8_t nextType() {
  if (bagLeft == 0) {
    for (uint8_t i = 0; i < PIECE_COUNT; i++) bag[i] = i;
    bagLeft = PIECE_COUNT;
  }
  const uint8_t i = random8(bagLeft);
  const uint8_t type = bag[i];
  bag[i] = bag[--bagLeft];
  return type;
}

bool fits(const Cell* cells, int x, int y) {
  for (uint8_t i = 0; i < 4; i++) {
    const int cx = x + cells[i].x, cy = y + cells[i].y;
    if (cx < 0 || cx >= MATRIX_W || cy >= MATRIX_H) return false;
    if (cy >= 0 && board[cy][cx]) return false;  // au-dessus du haut : autorisé
  }
  return true;
}

void setState(State s) {
  state = s;
  stateSince = millis();
  dirty = true;
}

void spawn() {
  cur.type = nextType();
  const PieceDef& def = PIECES[cur.type];
  memcpy(cur.cells, def.cells, sizeof(cur.cells));

  int8_t minY = 127;
  for (const Cell& c : cur.cells) minY = min(minY, c.y);
  cur.x = (MATRIX_W - def.size) / 2;
  cur.y = -minY;  // la pièce apparaît collée en haut

  lastDrop = millis();
  dirty = true;
  if (!fits(cur.cells, cur.x, cur.y)) setState(State::GameOver);
}

bool tryMove(int dx, int dy) {
  if (!fits(cur.cells, cur.x + dx, cur.y + dy)) return false;
  cur.x += dx;
  cur.y += dy;
  dirty = true;
  return true;
}

void rotate() {
  if (cur.type == SQUARE) return;
  const uint8_t size = PIECES[cur.type].size;

  Cell rotated[4];  // rotation horaire dans la boîte : (x, y) -> (size-1-y, x)
  for (uint8_t i = 0; i < 4; i++) {
    rotated[i] = {(int8_t)(size - 1 - cur.cells[i].y), cur.cells[i].x};
  }
  // Si la place manque, on essaie de décaler un peu la pièce
  const int8_t kicks[][2] = {{0, 0}, {-1, 0}, {1, 0}, {-2, 0}, {2, 0}, {0, -1}};
  for (const auto& k : kicks) {
    if (fits(rotated, cur.x + k[0], cur.y + k[1])) {
      memcpy(cur.cells, rotated, sizeof(rotated));
      cur.x += k[0];
      cur.y += k[1];
      dirty = true;
      return;
    }
  }
}

void lock() {
  for (const Cell& c : cur.cells) {
    const int cy = cur.y + c.y;
    if (cy < 0) {  // la pile dépasse le haut
      setState(State::GameOver);
      return;
    }
    board[cy][cur.x + c.x] = cur.type + 1;
  }

  bool anyFull = false;
  for (uint8_t y = 0; y < MATRIX_H; y++) {
    fullRow[y] = true;
    for (uint8_t x = 0; x < MATRIX_W; x++) fullRow[y] &= board[y][x] != 0;
    anyFull |= fullRow[y];
  }
  if (anyFull) setState(State::Clearing);
  else spawn();
}

void removeFullRows() {
  uint8_t cleared = 0;
  int dst = MATRIX_H - 1;
  for (int src = MATRIX_H - 1; src >= 0; src--) {
    if (fullRow[src]) {
      cleared++;
      continue;
    }
    if (dst != src) memcpy(board[dst], board[src], MATRIX_W);
    dst--;
  }
  for (; dst >= 0; dst--) memset(board[dst], 0, MATRIX_W);

  addScore((uint32_t)LINE_POINTS[min<int>(cleared, 4)] * currentLevel());
  lines += cleared;
}

void reset() {
  memset(board, 0, sizeof(board));
  bagLeft = 0;
  scoreValue = 0;
  lines = 0;
  setState(State::Playing);
  spawn();
}

void render(uint32_t now) {
  FastLED.clear();
  const uint32_t elapsed = now - stateSince;

  for (uint8_t y = 0; y < MATRIX_H; y++) {
    for (uint8_t x = 0; x < MATRIX_W; x++) {
      if (board[y][x]) leds[XY(x, y)] = colorOf(board[y][x] - 1).nscale8_video(200);
    }
  }

  if (state == State::Playing) {
    // Fantôme : où la pièce va atterrir
    int ghostY = cur.y;
    while (fits(cur.cells, cur.x, ghostY + 1)) ghostY++;
    CRGB ghost = colorOf(cur.type);
    ghost.nscale8_video(45);
    for (const Cell& c : cur.cells) setPixel(cur.x + c.x, ghostY + c.y, ghost);
    for (const Cell& c : cur.cells) setPixel(cur.x + c.x, cur.y + c.y, colorOf(cur.type));
  } else if (state == State::Clearing) {
    // Les lignes complètes clignotent en blanc
    if ((elapsed / 70) % 2 == 0) {
      for (uint8_t y = 0; y < MATRIX_H; y++)
        if (fullRow[y])
          for (uint8_t x = 0; x < MATRIX_W; x++) leds[XY(x, y)] = CRGB::White;
    }
  } else {
    // Fin de partie : la pile se remplit de gris depuis le bas
    const uint16_t rows = min<uint32_t>(MATRIX_H, elapsed * MATRIX_H / (GAME_OVER_MS * 2 / 3));
    for (uint8_t i = 0; i < rows; i++)
      for (uint8_t x = 0; x < MATRIX_W; x++) leds[XY(x, MATRIX_H - 1 - i)] = CRGB(40, 40, 40);
  }
  FastLED.show();
}

void update(uint32_t) {
  const uint32_t now = millis();

  switch (state) {
    case State::Playing:
      if (now - lastDrop >= dropMs()) {
        lastDrop = now;
        if (!tryMove(0, 1)) lock();
        dirty = true;
      }
      break;

    case State::Clearing:
      if (now - stateSince >= CLEAR_ANIM_MS) {
        removeFullRows();
        setState(State::Playing);
        spawn();
      }
      dirty = true;
      break;

    case State::GameOver:
      if (now - stateSince >= GAME_OVER_MS) reset();
      dirty = true;
      break;
  }

  if (dirty && now - lastRender >= RENDER_MS) {
    lastRender = now;
    dirty = false;
    render(now);
  }
}

}  // namespace

void input(Action action) {
  if (state != State::Playing) return;
  switch (action) {
    case Action::Left:  tryMove(-1, 0); break;
    case Action::Right: tryMove(1, 0); break;
    case Action::Rotate: rotate(); break;
    case Action::SoftDrop:
      if (tryMove(0, 1)) addScore(1);
      else lock();
      lastDrop = millis();
      break;
    case Action::HardDrop: {
      uint8_t fall = 0;
      while (tryMove(0, 1)) fall++;
      addScore(2 * fall);
      lock();
      break;
    }
  }
}

uint32_t score() { return scoreValue; }
uint32_t best() { return bestValue; }
uint8_t level() { return currentLevel(); }

const Animation game = {
    "blocks", "Blocs", "🧱", AnimType::Game, reset, update, nullptr};

}  // namespace blocks
