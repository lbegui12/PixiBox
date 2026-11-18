# Matrice LED 9×10 – ESP32

Une matrice de 90 LEDs WS2812B (9 colonnes × 10 lignes) pilotée par un ESP32 et
contrôlée depuis un téléphone ou un ordinateur, sans application à installer : il suffit
d'ouvrir une page web.

- **9 effets lumineux** : vitesse réglable.
- **5 jeux** : Snake, Pong à 2 joueurs, Blocs, Invasion, Envol.
- **Mode Direct** : la matrice affiche la caméra du téléphone, un dessin fait au doigt
  ou une photo.
- **Plusieurs téléphones en même temps** : tous voient le même état en temps réel.

---

## Sommaire

1. [Matériel](#1-matériel)
2. [Installation](#2-installation)
3. [Utilisation](#3-utilisation)
4. [Les jeux](#4-les-jeux)
5. [Les effets](#5-les-effets)
6. [Mode Direct](#6-mode-direct)
7. [Structure du code](#7-structure-du-code)
8. [Ajouter un effet ou un jeu](#8-ajouter-un-effet-ou-un-jeu)
9. [Protocole WebSocket](#9-protocole-websocket)
10. [Dépannage](#10-dépannage)

---

## 1. Matériel

| Élément | Détail |
|---|---|
| Carte | ESP32 (testé en compilation pour `esp32dev`) |
| LEDs | 90 × WS2812B, câblées en **colonnes serpentin** |
| Alimentation | 5 V, ≥ 3 A conseillé (voir `LED_MAX_MILLIAMPS`) |
| Data | GPIO 2 (modifiable dans `include/config.h`) |

### Câblage de la matrice

La LED n°0 est en **haut à gauche**. La colonne 0 descend, la colonne 1 remonte, la
colonne 2 descend, etc. :

```
x →   0   1   2  …   8
y=0   0  19  20  …  80
y=1   1  18  21  …  81
 …    …   …   …  …   …
y=9   9  10  29  …  89
```

Si ta matrice est câblée autrement, seule la fonction `XY()` de `src/matrix.h` est à
adapter.

### Conseils

- **Alimentation** : une WS2812B tire jusqu'à 60 mA en blanc, soit ~5,4 A pour 90 LEDs.
  `LED_MAX_MILLIAMPS` (dans `config.h`, 2000 mA par défaut) fait baisser
  automatiquement la luminosité pour ne jamais dépasser ce budget. Règle-le selon ton
  alimentation.
- Mets un condensateur de 500 à 1000 µF entre +5 V et GND près de la matrice et une
  résistance de 330 Ω en série sur la ligne data. **Relie les masses** de l'ESP32 et de
  l'alimentation.
- Les WS2812B attendent un signal 5 V. Les 3,3 V de l'ESP32 suffisent souvent, mais un
  *level shifter* (74AHCT125…) évite les scintillements aléatoires.
- **GPIO 2** est une broche de *strapping* : si le flash USB échoue quand la matrice est
  branchée, passe sur GPIO 16, 17 ou 18.

---

## 2. Installation

Le projet utilise [PlatformIO](https://platformio.org/), sous forme d'extension VS Code
ou en ligne de commande.

1. **Identifiants WiFi** : copie `include/secrets.example.h` en `include/secrets.h` et
   remplis-le :
   ```cpp
   #define WIFI_SSID "mon-wifi"
   #define WIFI_PASSWORD "mon-mot-de-passe"
   #define OTA_PASSWORD "change-moi"
   ```
   `secrets.h` est ignoré par git : tes identifiants ne seront jamais publiés.

2. **Firmware** :
   ```
   pio run -t upload
   ```

3. **Interface web** (dossier `data/`, stocké en LittleFS) :
   ```
   pio run -t uploadfs
   ```
   À refaire à chaque modification d'un fichier de `data/`.

4. Ouvre **http://esp32.local**, ou l'adresse IP affichée par `pio device monitor`.

### Au démarrage

| La matrice affiche… | Signification |
|---|---|
| un point bleu qui tourne | connexion au WiFi en cours (20 s max) |
| un flash vert | connecté |
| un flash orange | WiFi introuvable : l'ESP32 crée son propre réseau **`LedMatrix`** (mot de passe `ledmatrix`), interface sur **http://192.168.4.1** |

### Mise à jour sans câble (OTA)

Après le premier flash par USB, décommente ces lignes dans `platformio.ini` :

```ini
upload_protocol = espota
upload_port = esp32.local
upload_flags = --auth=MOT_DE_PASSE_OTA
```

`upload` et `uploadfs` passent alors par le WiFi. Une barre de progression s'affiche sur
la matrice pendant la mise à jour.

---

## 3. Utilisation

L'accueil propose trois entrées : **Animations**, **Jeux** et **Direct**, ainsi que le
réglage de **luminosité**.

- La pastille en haut indique la connexion à la matrice : verte = connecté, rouge =
  reconnexion automatique en cours.
- La carte de l'animation en cours est surlignée en vert.
- La **vitesse** des effets se règle dans la liste des animations.
- **Luminosité, vitesse et dernière animation sont sauvegardées** et restaurées au
  redémarrage.
- Si quelqu'un change d'animation depuis un autre téléphone, ton écran de jeu se ferme
  et un message t'en informe.

---

## 4. Les jeux

Tous les jeux se contrôlent au doigt sur la zone tactile, avec des boutons, ou au
clavier sur ordinateur. Quand une partie est perdue, une courte animation se joue puis
une nouvelle partie commence.

### 🐍 Snake

Mange les pommes rouges pour grandir. Les bords de la matrice se rejoignent : en
sortant à droite, on réapparaît à gauche. Le serpent accélère à chaque pomme. Si tu
remplis toute la matrice, c'est gagné.

| Action | Tactile | Clavier |
|---|---|---|
| Tourner | glisser (on peut enchaîner les virages sans lever le doigt) ou D-pad | flèches, ZQSD ou WASD |

### 🏓 Pong (1 ou 2 joueurs)

Le premier téléphone qui ouvre Pong prend la raquette du **haut (bleue)**, le deuxième
celle du **bas (verte)**. Une raquette sans joueur est **jouée par l'ordinateur** : on
peut donc jouer seul, et sans joueur le jeu tourne en démo.

- La balle accélère à chaque renvoi.
- Elle part en biais selon l'endroit où elle touche la raquette (bord gauche, centre ou
  bord droit).
- La partie se joue en **5 points**. Le score s'affiche sur la matrice après chaque
  point, et dans l'interface.
- Une raquette se libère dès que son joueur quitte la page ou que son téléphone se met
  en veille.

| Action | Tactile | Clavier |
|---|---|---|
| Déplacer la raquette | glisser horizontalement (position absolue) | ← → |

### 🧱 Blocs

Des pièces de 4 cases tombent. Complète des lignes horizontales pour les effacer avant
que la pile n'atteigne le haut.

- Une **ombre** discrète montre où la pièce va atterrir.
- Les 7 formes arrivent dans un ordre mélangé : pas de longue série sans la barre.
- Un niveau toutes les **5 lignes**. Les pièces tombent de plus en plus vite.
- Points : 100 / 300 / 500 / 800 pour 1 / 2 / 3 / 4 lignes à la fois, multipliés par
  le niveau, plus un bonus quand on fait descendre la pièce soi-même.

| Action | Tactile | Clavier |
|---|---|---|
| Déplacer | glisser ◀ ▶ (une case tous les ~28 px) ou boutons ◀ ▶ | ← → |
| Tourner | appui simple ou bouton ⟳ | ↑ |
| Descendre d'une case | bouton ▼ | ↓ |
| Lâcher la pièce | glisser vers le bas ou bouton ⤓ | espace |

### 👾 Invasion

Une formation de 12 envahisseurs avance en zigzag et descend d'une ligne à chaque bord,
en lâchant des bombes rouges. Détruis-la avec ton canon vert avant qu'elle n'arrive à
son niveau.

- Un seul tir à la fois : vise bien !
- **3 vies**. Une bombe sur le canon coûte une vie, et les vies restantes s'affichent en
  haut de la matrice.
- La formation accélère quand elle perd des membres, et à chaque nouvelle vague.
- La rangée du haut rapporte plus de points, et les points augmentent à chaque vague.

| Action | Tactile | Clavier |
|---|---|---|
| Viser | glisser horizontalement | ← → |
| Tirer | toucher la zone ou bouton 🔥 | espace ou ↑ |

### 🐦 Envol

Ton oiseau jaune tombe en permanence et chaque appui le fait remonter. Passe dans les
trous des colonnes vertes qui défilent.

- La partie commence au premier appui.
- Le plafond bloque l'oiseau sans le tuer, mais toucher le sol fait perdre.
- Le défilement accélère à chaque point, et après 10 points les trous rétrécissent
  (4 cases puis 3).
- Deux trous successifs ne sont jamais trop éloignés : il y a toujours un passage
  possible.

| Action | Tactile | Clavier |
|---|---|---|
| Coup d'aile | toucher la zone | espace ou ↑ |

---

## 5. Les effets

| Effet | Description |
|---|---|
| 🎲 Aléatoire | pixels de couleur qui s'allument puis s'estompent |
| 🌧️ Pluie | colonnes de gouttes colorées qui tombent à des rythmes différents |
| 📐 Diagonales / | fines diagonales qui glissent, couleur qui tourne |
| 📏 Diagonales \\ | idem dans l'autre sens |
| 🌀 Tunnel | rectangles concentriques dont les couleurs défilent |
| 💧 Onde | simulation physique d'une onde lancée depuis le centre (rouge = +, bleu = −) |
| 🌊 Vagues | même simulation, onde plane qui monte depuis le bas |
| 🔥 Feu | flammes de cheminée (simulation de chaleur par colonne) |
| 🧬 Vie (Conway) | jeu de la vie de John Conway, relancé automatiquement quand il se fige |

---

## 6. Mode Direct

La matrice affiche ce que le navigateur lui envoie. Trois onglets :

- **📷 Caméra** : environ 15 images/s, caméra avant ou arrière (bouton 🔄). L'image est
  recadrée au format de la matrice et corrigée en gamma.
- **🎨 Dessin** : dessine au doigt directement sur l'aperçu, avec une couleur, une
  gomme, et des boutons pour remplir ou effacer.
- **🖼️ Image** : choisis une photo, elle est réduite à 9×10 pixels.

### ⚠️ Caméra et HTTPS

Les navigateurs n'autorisent la caméra que sur les pages HTTPS, ce qui n'est pas le cas
de `http://esp32.local`. Sur Chrome (Android ou PC), tu peux l'autoriser quand même :

1. ouvre `chrome://flags/#unsafely-treat-insecure-origin-as-secure` ;
2. ajoute `http://esp32.local` (ou l'adresse IP de l'ESP32) ;
3. passe l'option à *Enabled* et relance Chrome.

Les onglets **Dessin** et **Image** fonctionnent partout, sans cette manipulation.

---

## 7. Structure du code

```
platformio.ini            configuration PlatformIO + bibliothèques (versions fixées)
include/
  config.h                broche, dimensions, limite de courant, réseau, réglages par défaut
  secrets.h               identifiants WiFi / OTA (non versionné)
  secrets.example.h       modèle de secrets.h
src/
  main.cpp                setup() / loop() : appelle les modules ci-dessous
  matrix.h / .cpp         tableau `leds`, XY() (câblage serpentin), setPixel() protégé
  animation.h             interface commune effets / jeux
  animator.h / .cpp       table des animations, transitions, mode Direct, réglages sauvegardés
  live.h / .cpp           mode Direct : images reçues du navigateur (+ gamma)
  net.h / .cpp            WiFi, point d'accès de secours, mDNS, OTA
  web.h / .cpp            serveur HTTP (port 80) + WebSocket (port 81)
  effects/                un fichier par effet (+ effects.h)
  games/                  snake, pong, blocks, invasion, flyer (.h / .cpp)
data/                     interface web, envoyée en LittleFS
  index.html              les vues : accueil, listes, jeux, direct
  style.css
  app.js                  WebSocket, navigation, commandes tactiles et clavier
_original/                la première version du projet, pour comparer
```

### Principes

- **Rien ne bloque `loop()`** : pas de `delay()` dans les animations. Chaque jeu est une
  petite machine à états (en jeu / point marqué / perdu…) rythmée par `millis()`. Le
  WiFi, le serveur et les autres joueurs restent donc réactifs.
- **Une animation = une structure `Animation`** (`src/animation.h`) avec :
  - `begin()` : remise à zéro au démarrage ;
  - `update()` : une image ;
  - `end()` : appelée quand on quitte l'animation.
- **L'interface web n'a aucune liste en dur** : elle reçoit la liste des animations
  (nom, icône, type) depuis l'ESP32.
- **Une seule boucle, un seul fil d'exécution** : les messages WebSocket sont traités
  dans `loop()`, donc aucun accès concurrent aux variables des jeux.

---

## 8. Ajouter un effet ou un jeu

### Un effet

```cpp
// src/effects/mon_effet.cpp
#include "effects.h"
#include "../matrix.h"

namespace {

void begin() {
  // remise à zéro de l'état (optionnel : mettre nullptr dans la structure)
}

void update(uint32_t frame) {
  FastLED.clear();
  setPixel(frame % MATRIX_W, frame / MATRIX_W % MATRIX_H, CRGB::Red);
  FastLED.show();
}

}  // namespace

const Animation effects::monEffet = {
    "mon-effet",        // identifiant (sauvegarde des réglages)
    "Mon effet",        // nom affiché
    "⭐",               // icône
    AnimType::Effect,
    begin, update, nullptr};
```

Ensuite :

1. déclare-le dans `src/effects/effects.h` : `extern const Animation monEffet;`
2. ajoute `&effects::monEffet` dans la table `ANIMATIONS` de `src/animator.cpp`.

L'interface web l'affiche automatiquement. `update()` est appelée toutes les *vitesse* ms
(curseur de l'interface).

### Un jeu

Même principe avec `AnimType::Game`. `update()` est alors appelée **à chaque tour de
`loop()`** : le jeu gère son propre rythme avec `millis()` (voir `games/flyer.cpp`, le
plus simple).

Pour les commandes et le score :

1. **Commandes** : ajoute un cas dans `handleCommand()` de `src/web.cpp`, par exemple
   `monjeu saut`.
2. **Score** : ajoute une ligne dans `SCORE_SOURCES` de `src/web.cpp`. Le score est
   alors envoyé automatiquement à chaque changement.
3. **Interface** : dans `data/` :
   - une `<section id="view-monjeu" class="view" hidden>` dans `index.html` ;
   - la clé `'monjeu'` dans `GAME_VIEWS` de `app.js` ;
   - les gestes ou boutons qui envoient `send('monjeu saut')` ;
   - un format d'affichage dans `SCORE_FORMAT`.

---

## 9. Protocole WebSocket

Port **81**. La description complète est en tête de `src/web.h`.

**Navigateur → ESP32** (texte) :

| Message | Effet |
|---|---|
| `set <index>` | choisir une animation |
| `speed <ms>` / `bright <0-255>` | vitesse des effets / luminosité |
| `snake <u\|d\|l\|r>` | direction du serpent |
| `pong join` / `pong leave` | prendre / libérer une raquette |
| `pong pos <0-1000>` / `pong nudge <-1\|1>` | position de la raquette |
| `blocks <l\|r\|rot\|down\|drop>` | Blocs |
| `inv pos <0-1000>` / `inv nudge <-1\|1>` / `inv fire` | Invasion |
| `flyer flap` | Envol |
| `live on` / `live off` | mode Direct |

**Navigateur → ESP32** (binaire) : une image de 270 octets (90 × R, G, B), ligne par
ligne de gauche à droite en partant du haut. N'est accepté qu'en mode Direct.

**ESP32 → navigateur** (JSON) : `list` (animations disponibles), `state` (animation,
mode Direct, vitesse, luminosité), `snake`, `pong`, `pongJoin`, `score` (Blocs, Invasion,
Envol).

---

## 10. Dépannage

| Problème | Piste |
|---|---|
| Page « Interface absente » | lance `pio run -t uploadfs` |
| `esp32.local` introuvable | utilise l'adresse IP (moniteur série). Certains Android ne gèrent pas le mDNS |
| Pastille rouge en permanence | le port 81 (WebSocket) est bloqué ou l'ESP32 a redémarré : recharge la page |
| Flash orange au démarrage | SSID ou mot de passe erroné dans `secrets.h`, ou WiFi 5 GHz (l'ESP32 ne gère que le 2,4 GHz) |
| Couleurs fausses (rouge ↔ vert) | change `LED_COLOR_ORDER` dans `config.h` (GRB, RGB…) |
| Image en miroir / à l'envers | adapte `XY()` dans `src/matrix.h` à ton câblage |
| Scintillements | masse commune, résistance sur la data, *level shifter*, condensateur |
| Luminosité qui baisse toute seule | c'est la limite `LED_MAX_MILLIAMPS` qui protège l'alimentation |
| Le flash USB échoue | débranche la matrice ou change `LED_PIN` (GPIO 2 = broche de strapping) |

Moniteur série : `pio device monitor` (115200 bauds). Chaque connexion, changement
d'animation et sauvegarde des réglages y est affiché.
