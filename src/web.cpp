#include "web.h"

#include <LittleFS.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

#include "animator.h"
#include "games/blocks.h"
#include "games/flyer.h"
#include "games/invasion.h"
#include "games/pong.h"
#include "games/snake.h"
#include "live.h"

namespace web {
namespace {

WebServer http(HTTP_PORT);
WebSocketsServer ws(WS_PORT);

bool fsMounted = false;
int liveOwner = -1;  // client qui a lancé le mode Direct

// Les changements d'état sont envoyés au plus toutes les 100 ms à tout le monde
constexpr uint16_t BROADCAST_INTERVAL_MS = 100;
uint32_t lastBroadcast = 0;
uint32_t sentVersion = 0;
bool stateSent = false;

struct SnakeInfo {
  uint8_t len, best;
  bool operator!=(const SnakeInfo& o) const { return len != o.len || best != o.best; }
} sentSnake = {0, 0};

struct PongInfo {
  uint8_t s1, s2;
  bool h1, h2;
  bool operator!=(const PongInfo& o) const {
    return s1 != o.s1 || s2 != o.s2 || h1 != o.h1 || h2 != o.h2;
  }
} sentPong = {0, 0, false, false};

// Scores des jeux "simples" : envoyés sous la forme
// {"t":"score","key":"blocks","score":1200,"best":3400,"extra":2}
struct ScoreSource {
  const char* key;
  uint32_t (*score)();
  uint32_t (*best)();
  uint8_t (*extra)();  // niveau, vies... (nullptr si rien)
};
uint8_t noExtra() { return 0; }
const ScoreSource SCORE_SOURCES[] = {
    {"blocks", blocks::score, blocks::best, blocks::level},
    {"invasion", invasion::score, invasion::best, invasion::lives},
    {"flyer", flyer::score, flyer::best, noExtra},
};
constexpr size_t SCORE_SOURCE_COUNT = sizeof(SCORE_SOURCES) / sizeof(SCORE_SOURCES[0]);

struct ScoreInfo {
  uint32_t score, best;
  uint8_t extra;
  bool operator!=(const ScoreInfo& o) const {
    return score != o.score || best != o.best || extra != o.extra;
  }
};
ScoreInfo sentScores[SCORE_SOURCE_COUNT] = {};

// ---------------------------------------------------------------- JSON --

void appendJsonString(String& out, const char* s) {
  out += '"';
  for (; *s; s++) {
    if (*s == '"' || *s == '\\') out += '\\';
    out += *s;
  }
  out += '"';
}

String listJson() {
  String json = "{\"t\":\"list\",\"items\":[";
  for (size_t i = 0; i < animator::count(); i++) {
    const Animation& a = animator::at(i);
    if (i) json += ',';
    json += "{\"id\":";
    json += i;
    json += ",\"key\":";
    appendJsonString(json, a.id);
    json += ",\"name\":";
    appendJsonString(json, a.label);
    json += ",\"icon\":";
    appendJsonString(json, a.icon);
    json += ",\"type\":";
    json += (a.type == AnimType::Game) ? "\"game\"" : "\"effect\"";
    json += '}';
  }
  json += "]}";
  return json;
}

String stateJson() {
  char buf[96];
  snprintf(buf, sizeof(buf), "{\"t\":\"state\",\"anim\":%d,\"live\":%s,\"speed\":%u,\"bright\":%u}",
           animator::current(), animator::isLive() ? "true" : "false",
           animator::speed(), animator::brightness());
  return buf;
}

SnakeInfo snakeInfo() { return {snake::length(), snake::best()}; }

String snakeJson(const SnakeInfo& s) {
  char buf[48];
  snprintf(buf, sizeof(buf), "{\"t\":\"snake\",\"len\":%u,\"best\":%u}", s.len, s.best);
  return buf;
}

PongInfo pongInfo() {
  return {pong::score(1), pong::score(2), pong::isHuman(1), pong::isHuman(2)};
}

String pongJson(const PongInfo& p) {
  char buf[64];
  snprintf(buf, sizeof(buf), "{\"t\":\"pong\",\"s1\":%u,\"s2\":%u,\"h1\":%s,\"h2\":%s}",
           p.s1, p.s2, p.h1 ? "true" : "false", p.h2 ? "true" : "false");
  return buf;
}

ScoreInfo scoreInfo(size_t i) {
  const ScoreSource& src = SCORE_SOURCES[i];
  return {src.score(), src.best(), src.extra()};
}

String scoreJson(size_t i, const ScoreInfo& s) {
  char buf[96];
  snprintf(buf, sizeof(buf), "{\"t\":\"score\",\"key\":\"%s\",\"score\":%lu,\"best\":%lu,\"extra\":%u}",
           SCORE_SOURCES[i].key, (unsigned long)s.score, (unsigned long)s.best, s.extra);
  return buf;
}

void send(uint8_t client, const String& msg) { ws.sendTXT(client, msg.c_str(), msg.length()); }
void broadcast(const String& msg) { ws.broadcastTXT(msg.c_str(), msg.length()); }

// ----------------------------------------------------------- commandes --

void handleCommand(uint8_t client, const uint8_t* payload, size_t length) {
  char buf[48];
  const size_t n = min(length, sizeof(buf) - 1);
  memcpy(buf, payload, n);
  buf[n] = '\0';

  char* rest = nullptr;
  const char* cmd = strtok_r(buf, " ", &rest);
  const char* arg1 = strtok_r(nullptr, " ", &rest);
  const char* arg2 = strtok_r(nullptr, " ", &rest);
  if (!cmd || !arg1) return;

  if (!strcmp(cmd, "set")) {
    animator::select(atoi(arg1));
  } else if (!strcmp(cmd, "speed")) {
    animator::setSpeed(atoi(arg1));
  } else if (!strcmp(cmd, "bright")) {
    animator::setBrightness(atoi(arg1));
  } else if (!strcmp(cmd, "snake")) {
    switch (arg1[0]) {
      case 'u': snake::queueDirection(snake::Dir::Up); break;
      case 'd': snake::queueDirection(snake::Dir::Down); break;
      case 'l': snake::queueDirection(snake::Dir::Left); break;
      case 'r': snake::queueDirection(snake::Dir::Right); break;
    }
  } else if (!strcmp(cmd, "pong")) {
    if (!strcmp(arg1, "join")) {
      char reply[40];
      snprintf(reply, sizeof(reply), "{\"t\":\"pongJoin\",\"player\":%u}", pong::join(client));
      send(client, reply);
    } else if (!strcmp(arg1, "leave")) {
      pong::leave(client);
    } else if (!strcmp(arg1, "pos") && arg2) {
      pong::setPaddle(client, constrain(atoi(arg2), 0, 1000));
    } else if (!strcmp(arg1, "nudge") && arg2) {
      pong::nudgePaddle(client, atoi(arg2) < 0 ? -1 : 1);
    }
  } else if (!strcmp(cmd, "blocks")) {
    if (!strcmp(arg1, "l")) blocks::input(blocks::Action::Left);
    else if (!strcmp(arg1, "r")) blocks::input(blocks::Action::Right);
    else if (!strcmp(arg1, "rot")) blocks::input(blocks::Action::Rotate);
    else if (!strcmp(arg1, "down")) blocks::input(blocks::Action::SoftDrop);
    else if (!strcmp(arg1, "drop")) blocks::input(blocks::Action::HardDrop);
  } else if (!strcmp(cmd, "inv")) {
    if (!strcmp(arg1, "fire")) invasion::fire();
    else if (!strcmp(arg1, "pos") && arg2) invasion::setPosition(constrain(atoi(arg2), 0, 1000));
    else if (!strcmp(arg1, "nudge") && arg2) invasion::nudge(atoi(arg2) < 0 ? -1 : 1);
  } else if (!strcmp(cmd, "flyer")) {
    if (!strcmp(arg1, "flap")) flyer::flap();
  } else if (!strcmp(cmd, "live")) {
    if (!strcmp(arg1, "on")) {
      liveOwner = client;
      animator::enterLive();
    } else if (!strcmp(arg1, "off")) {
      animator::exitLive();
    }
  }
}

void onWsEvent(uint8_t client, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.printf("[ws] client %u connecté (%s)\n", client, ws.remoteIP(client).toString().c_str());
      send(client, listJson());
      send(client, stateJson());
      send(client, snakeJson(snakeInfo()));
      send(client, pongJson(pongInfo()));
      for (size_t i = 0; i < SCORE_SOURCE_COUNT; i++) send(client, scoreJson(i, scoreInfo(i)));
      break;

    case WStype_DISCONNECTED:
      Serial.printf("[ws] client %u déconnecté\n", client);
      pong::leave(client);
      if (liveOwner == client) animator::exitLive();
      break;

    case WStype_TEXT:
      handleCommand(client, payload, length);
      break;

    case WStype_BIN:
      if (animator::isLive() && !live::showFrame(payload, length)) {
        Serial.printf("[ws] image de taille invalide : %u octets\n", (unsigned)length);
      }
      break;

    default:
      break;
  }
}

// ---------------------------------------------------------------- HTTP --

const char* contentType(const String& path) {
  if (path.endsWith(".html")) return "text/html; charset=utf-8";
  if (path.endsWith(".css")) return "text/css";
  if (path.endsWith(".js")) return "application/javascript";
  if (path.endsWith(".json")) return "application/json";
  if (path.endsWith(".png")) return "image/png";
  if (path.endsWith(".svg")) return "image/svg+xml";
  if (path.endsWith(".ico")) return "image/x-icon";
  return "text/plain";
}

bool serveFile(String path) {
  if (!fsMounted) return false;
  if (path.endsWith("/")) path += "index.html";
  if (!LittleFS.exists(path)) return false;

  File file = LittleFS.open(path, "r");
  if (!file) return false;
  http.sendHeader("Cache-Control", "no-cache");
  http.streamFile(file, contentType(path));
  file.close();
  return true;
}

void handleRequest() {
  if (serveFile(http.uri())) return;

  if (http.uri() == "/" || http.uri() == "/index.html") {
    http.send(500, "text/html; charset=utf-8",
              "<h1>Interface absente</h1><p>Le dossier <code>data/</code> n'a pas été "
              "envoyé sur l'ESP32. Lance <code>pio run -t uploadfs</code>.</p>");
  } else {
    http.send(404, "text/plain", "Introuvable");
  }
}

}  // namespace

void begin() {
  fsMounted = LittleFS.begin(false);
  if (!fsMounted) Serial.println("[web] LittleFS non monté : as-tu lancé `pio run -t uploadfs` ?");

  http.onNotFound(handleRequest);  // toutes les URLs passent par les fichiers
  http.begin();

  ws.begin();
  ws.onEvent(onWsEvent);
  // Ping toutes les 5 s, client considéré perdu après 2 pongs manqués : un
  // téléphone qui se met en veille libère sa raquette tout seul.
  ws.enableHeartbeat(5000, 3000, 2);

  Serial.printf("[web] HTTP sur le port %u, WebSocket sur le port %u\n", HTTP_PORT, WS_PORT);
}

void loop() {
  http.handleClient();
  ws.loop();

  if (!animator::isLive()) liveOwner = -1;

  const uint32_t now = millis();
  if (now - lastBroadcast < BROADCAST_INTERVAL_MS) return;
  lastBroadcast = now;

  if (!stateSent || animator::version() != sentVersion) {
    sentVersion = animator::version();
    stateSent = true;
    broadcast(stateJson());
  }
  const SnakeInfo s = snakeInfo();
  if (s != sentSnake) {
    sentSnake = s;
    broadcast(snakeJson(s));
  }
  const PongInfo p = pongInfo();
  if (p != sentPong) {
    sentPong = p;
    broadcast(pongJson(p));
  }
  for (size_t i = 0; i < SCORE_SOURCE_COUNT; i++) {
    const ScoreInfo info = scoreInfo(i);
    if (info != sentScores[i]) {
      sentScores[i] = info;
      broadcast(scoreJson(i, info));
    }
  }
}

}  // namespace web
