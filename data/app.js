'use strict';

// Dimensions de la matrice (doivent correspondre à include/config.h)
const W = 9;
const H = 10;

const $ = (id) => document.getElementById(id);

// ======================================================================
//  Utilitaires
// ======================================================================

// Limite la fréquence d'appel de `fn` tout en garantissant que le dernier
// appel est bien exécuté (important pour la position finale d'un curseur).
function throttle(fn, ms) {
  let last = 0, timer = null, pendingArgs = null;
  return (...args) => {
    pendingArgs = args;
    const wait = ms - (Date.now() - last);
    if (wait <= 0) {
      last = Date.now();
      fn(...pendingArgs);
    } else if (!timer) {
      timer = setTimeout(() => {
        timer = null;
        last = Date.now();
        fn(...pendingArgs);
      }, wait);
    }
  };
}

let toastTimer = null;
function toast(message) {
  const el = $('toast');
  el.textContent = message;
  el.hidden = false;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { el.hidden = true; }, 3500);
}

// ======================================================================
//  WebSocket
// ======================================================================

const state = { list: [], anim: -1, live: false, speed: 50, bright: 100 };
let ws = null;
let retries = 0;

function connect() {
  ws = new WebSocket(`ws://${location.hostname || 'esp32.local'}:81/`);
  ws.binaryType = 'arraybuffer';

  ws.onopen = () => {
    retries = 0;
    $('status').classList.add('online');
    onReconnected();
  };
  ws.onclose = () => {
    $('status').classList.remove('online');
    setTimeout(connect, Math.min(5000, 300 * 2 ** retries++));
  };
  ws.onerror = () => ws.close();
  ws.onmessage = (event) => {
    if (typeof event.data !== 'string') return;
    try {
      handleMessage(JSON.parse(event.data));
    } catch (err) {
      console.error('Message invalide :', event.data, err);
    }
  };
}

function send(message) {
  if (ws && ws.readyState === WebSocket.OPEN) ws.send(message);
}

// Après une coupure, l'ESP32 a oublié qui jouait / diffusait : on se réinscrit.
function onReconnected() {
  if (view === 'pong') send('pong join');
  if (view === 'live') {
    liveConfirmed = false;
    send('live on');
    sendFrame();
  }
}

function handleMessage(msg) {
  switch (msg.t) {
    case 'list':
      state.list = msg.items;
      if (view === 'list') renderCards();
      break;

    case 'state':
      Object.assign(state, msg);
      onStateChanged();
      break;

    case 'snake':
      $('snake-score').textContent = `Longueur ${msg.len} · Record ${msg.best}`;
      break;

    case 'pong':
      pongScore = msg;
      renderPong();
      break;

    case 'pongJoin':
      pongPlayer = msg.player;
      renderPong();
      break;

    case 'score':
      renderScore(msg);
      break;
  }
}

// ======================================================================
//  Navigation
// ======================================================================

// Jeux qui ont leur propre vue (section id="view-<key>")
const GAME_VIEWS = ['snake', 'pong', 'blocks', 'invasion', 'flyer'];

let view = 'home';
let category = 'effect';
let gameAnimId = -1;     // animation du jeu ouvert
let gameOpenedAt = 0;

function show(name) {
  view = name;
  for (const section of document.querySelectorAll('.view')) {
    section.hidden = section.id !== `view-${name}`;
  }
}

function showHome() {
  show('home');
}

function showList(type) {
  category = type;
  $('list-title').textContent = type === 'game' ? 'Jeux' : 'Animations';
  $('speed-box').hidden = type === 'game';
  renderCards();
  show('list');
}

function renderCards() {
  const container = $('cards');
  container.innerHTML = '';
  for (const anim of state.list.filter((a) => a.type === category)) {
    const card = document.createElement('button');
    card.className = 'card' + (anim.id === state.anim && !state.live ? ' active' : '');
    card.dataset.id = anim.id;
    card.innerHTML = '<span class="icon"></span><span class="name"></span>';
    card.querySelector('.icon').textContent = anim.icon;
    card.querySelector('.name').textContent = anim.name;
    card.onclick = () => selectAnimation(anim);
    container.appendChild(card);
  }
}

function highlightActiveCard() {
  for (const card of document.querySelectorAll('#cards .card')) {
    card.classList.toggle('active', Number(card.dataset.id) === state.anim && !state.live);
  }
}

function selectAnimation(anim) {
  send(`set ${anim.id}`);
  state.anim = anim.id;
  state.live = false;
  highlightActiveCard();

  if (anim.type !== 'game') return;
  gameAnimId = anim.id;
  gameOpenedAt = Date.now();

  if (!GAME_VIEWS.includes(anim.key)) return;
  if (anim.key === 'pong') {
    pongPlayer = -1;  // en attente de la réponse
    renderPong();
    send('pong join');
  }
  show(anim.key);
}

function goBack() {
  if (view === 'pong') send('pong leave');
  if (view === 'live') stopLive();

  if (GAME_VIEWS.includes(view)) showList('game');
  else showHome();
}

// Appelé à chaque message "state" de l'ESP32
let lastLocalSlider = 0;
function onStateChanged() {
  // Ne pas faire sauter un curseur que l'utilisateur est en train de bouger
  if (Date.now() - lastLocalSlider > 800) {
    $('bright').value = state.bright;
    $('speed').value = state.speed;
  }
  $('bright-val').textContent = Math.round((state.bright / 255) * 100) + ' %';
  $('speed-val').textContent = $('speed').value;
  highlightActiveCard();

  // Quelqu'un d'autre a changé d'animation pendant qu'on jouait
  const inGame = GAME_VIEWS.includes(view);
  const grace = Date.now() - gameOpenedAt < 1500;
  if (inGame && !grace && (state.anim !== gameAnimId || state.live)) {
    toast("Le jeu a été arrêté depuis un autre appareil.");
    showList('game');
  }

  // Idem pour le mode Direct
  if (view === 'live') {
    if (state.live) liveConfirmed = true;
    else if (liveConfirmed) {
      toast('Le mode Direct a été arrêté depuis un autre appareil.');
      stopLive(false);
      showHome();
    }
  }
}

// ======================================================================
//  Réglages
// ======================================================================

const sendBrightness = throttle((v) => send(`bright ${v}`), 60);
const sendSpeed = throttle((v) => send(`speed ${v}`), 60);

$('bright').addEventListener('input', (e) => {
  lastLocalSlider = Date.now();
  $('bright-val').textContent = Math.round((e.target.value / 255) * 100) + ' %';
  sendBrightness(e.target.value);
});

$('speed').addEventListener('input', (e) => {
  lastLocalSlider = Date.now();
  $('speed-val').textContent = e.target.value;
  sendSpeed(e.target.value);
});

// ======================================================================
//  Snake
// ======================================================================

const SWIPE_THRESHOLD = 25;  // pixels

function snakeDir(dir) { send(`snake ${dir}`); }

(() => {
  const zone = $('snake-zone');
  let start = null;

  zone.addEventListener('pointerdown', (e) => {
    zone.setPointerCapture(e.pointerId);
    zone.classList.add('touching');
    start = { x: e.clientX, y: e.clientY };
  });

  zone.addEventListener('pointermove', (e) => {
    if (!start) return;
    const dx = e.clientX - start.x;
    const dy = e.clientY - start.y;
    if (Math.abs(dx) < SWIPE_THRESHOLD && Math.abs(dy) < SWIPE_THRESHOLD) return;

    if (Math.abs(dx) > Math.abs(dy)) snakeDir(dx > 0 ? 'r' : 'l');
    else snakeDir(dy > 0 ? 'd' : 'u');
    // On repart de la position actuelle : on peut enchaîner les virages
    // sans lever le doigt.
    start = { x: e.clientX, y: e.clientY };
  });

  const end = () => { start = null; zone.classList.remove('touching'); };
  zone.addEventListener('pointerup', end);
  zone.addEventListener('pointercancel', end);

  for (const button of document.querySelectorAll('.dpad button')) {
    button.addEventListener('click', () => snakeDir(button.dataset.dir));
  }
})();

// ======================================================================
//  Pong
// ======================================================================

let pongPlayer = 0;  // 1 = haut, 2 = bas, 0 = spectateur, -1 = en attente
let pongScore = { s1: 0, s2: 0, h1: false, h2: false };

function renderPong() {
  const role = $('pong-role');
  if (pongPlayer === 1) role.textContent = 'Tu es le joueur 1 : raquette du haut 🔵';
  else if (pongPlayer === 2) role.textContent = 'Tu es le joueur 2 : raquette du bas 🟢';
  else if (pongPlayer === 0) role.textContent = 'Partie complète : tu regardes 👀';
  else role.textContent = 'Connexion à la partie…';

  const name1 = pongScore.h1 ? '🔵' : '🤖🔵';
  const name2 = pongScore.h2 ? '🟢' : '🟢🤖';
  $('pong-score').textContent = `${name1} ${pongScore.s1} – ${pongScore.s2} ${name2}`;
}

(() => {
  const zone = $('pong-zone');
  const sendPos = throttle((permille) => send(`pong pos ${permille}`), 30);

  const move = (e) => {
    const rect = zone.getBoundingClientRect();
    const x = Math.min(1, Math.max(0, (e.clientX - rect.left) / rect.width));
    sendPos(Math.round(x * 1000));
  };

  zone.addEventListener('pointerdown', (e) => {
    zone.setPointerCapture(e.pointerId);
    zone.classList.add('touching');
    move(e);
  });
  zone.addEventListener('pointermove', (e) => {
    if (zone.hasPointerCapture(e.pointerId)) move(e);
  });
  const end = () => zone.classList.remove('touching');
  zone.addEventListener('pointerup', end);
  zone.addEventListener('pointercancel', end);
})();

// ======================================================================
//  Scores des autres jeux
// ======================================================================

const SCORE_FORMAT = {
  blocks: (m) => `Score ${m.score} · Niveau ${m.extra} · Record ${m.best}`,
  invasion: (m) => `Score ${m.score} · ${'❤️'.repeat(m.extra) || '💀'} · Record ${m.best}`,
  flyer: (m) => `Score ${m.score} · Record ${m.best}`,
};

function renderScore(msg) {
  const format = SCORE_FORMAT[msg.key];
  const el = document.getElementById(`${msg.key}-score`);
  if (format && el) el.textContent = format(msg);
}

// ======================================================================
//  Blocs
// ======================================================================

const BLOCKS_STEP = 28;        // pixels de glissé pour décaler d'une case
const BLOCKS_DROP_SWIPE = 50;  // glissé vers le bas pour lâcher la pièce

function blocksAction(action) { send(`blocks ${action}`); }

(() => {
  const zone = $('blocks-zone');
  let start = null, moved = false, dropped = false;

  zone.addEventListener('pointerdown', (e) => {
    zone.setPointerCapture(e.pointerId);
    zone.classList.add('touching');
    start = { x: e.clientX, y: e.clientY };
    moved = dropped = false;
  });

  zone.addEventListener('pointermove', (e) => {
    if (!start || dropped) return;
    // Glissé horizontal : une case tous les BLOCKS_STEP pixels
    let dx = e.clientX - start.x;
    while (Math.abs(dx) >= BLOCKS_STEP) {
      blocksAction(dx > 0 ? 'r' : 'l');
      start.x += Math.sign(dx) * BLOCKS_STEP;
      dx = e.clientX - start.x;
      moved = true;
    }
    // Grand glissé vers le bas (sans avoir déplacé la pièce) : on la lâche
    if (!moved && e.clientY - start.y > BLOCKS_DROP_SWIPE) {
      blocksAction('drop');
      dropped = true;
    }
  });

  zone.addEventListener('pointerup', () => {
    if (start && !moved && !dropped) blocksAction('rot');  // simple appui : rotation
    start = null;
    zone.classList.remove('touching');
  });
  zone.addEventListener('pointercancel', () => {
    start = null;
    zone.classList.remove('touching');
  });

  for (const button of document.querySelectorAll('[data-blocks]')) {
    button.addEventListener('click', () => blocksAction(button.dataset.blocks));
  }
})();

// ======================================================================
//  Invasion
// ======================================================================

(() => {
  const zone = $('invasion-zone');
  const sendPos = throttle((permille) => send(`inv pos ${permille}`), 30);

  const aim = (e) => {
    const rect = zone.getBoundingClientRect();
    const x = Math.min(1, Math.max(0, (e.clientX - rect.left) / rect.width));
    sendPos(Math.round(x * 1000));
  };

  zone.addEventListener('pointerdown', (e) => {
    zone.setPointerCapture(e.pointerId);
    zone.classList.add('touching');
    aim(e);
    send('inv fire');
  });
  zone.addEventListener('pointermove', (e) => {
    if (zone.hasPointerCapture(e.pointerId)) aim(e);
  });
  const end = () => zone.classList.remove('touching');
  zone.addEventListener('pointerup', end);
  zone.addEventListener('pointercancel', end);

  // pointerdown plutôt que click : plus réactif sur mobile
  $('invasion-fire').addEventListener('pointerdown', (e) => {
    e.preventDefault();
    send('inv fire');
  });
})();

// ======================================================================
//  Envol
// ======================================================================

(() => {
  const zone = $('flyer-zone');
  zone.addEventListener('pointerdown', () => {
    send('flyer flap');
    zone.classList.add('touching');
    setTimeout(() => zone.classList.remove('touching'), 80);
  });
})();

// ======================================================================
//  Clavier
// ======================================================================

const BLOCKS_KEYS = { ArrowLeft: 'l', ArrowRight: 'r', ArrowUp: 'rot', ArrowDown: 'down', ' ': 'drop' };

const SNAKE_KEYS = {
  ArrowUp: 'u', ArrowDown: 'd', ArrowLeft: 'l', ArrowRight: 'r',
  z: 'u', s: 'd', q: 'l', d: 'r', w: 'u', a: 'l',
};

document.addEventListener('keydown', (e) => {
  if (view === 'snake' && SNAKE_KEYS[e.key]) {
    e.preventDefault();
    snakeDir(SNAKE_KEYS[e.key]);
  } else if (view === 'pong' && (e.key === 'ArrowLeft' || e.key === 'ArrowRight')) {
    e.preventDefault();
    send(`pong nudge ${e.key === 'ArrowLeft' ? -1 : 1}`);
  } else if (view === 'blocks' && BLOCKS_KEYS[e.key]) {
    e.preventDefault();
    blocksAction(BLOCKS_KEYS[e.key]);
  } else if (view === 'invasion') {
    if (e.key === 'ArrowLeft' || e.key === 'ArrowRight') {
      e.preventDefault();
      send(`inv nudge ${e.key === 'ArrowLeft' ? -1 : 1}`);
    } else if (e.key === ' ' || e.key === 'ArrowUp') {
      e.preventDefault();
      if (!e.repeat) send('inv fire');
    }
  } else if (view === 'flyer' && (e.key === ' ' || e.key === 'ArrowUp')) {
    e.preventDefault();
    if (!e.repeat) send('flyer flap');
  }
});

// ======================================================================
//  Direct : caméra / dessin / image
// ======================================================================

const frame = new Uint8Array(W * H * 3);  // image envoyée à la matrice
let liveConfirmed = false;
let tab = 'camera';

// --- Aperçu (une div par LED) ---
const previewCells = [];
(() => {
  const preview = $('preview');
  for (let i = 0; i < W * H; i++) {
    const cell = document.createElement('div');
    preview.appendChild(cell);
    previewCells.push(cell);
  }
})();

function renderCell(i) {
  const r = frame[i * 3], g = frame[i * 3 + 1], b = frame[i * 3 + 2];
  previewCells[i].style.background = `rgb(${r},${g},${b})`;
}

function renderPreview() {
  for (let i = 0; i < W * H; i++) renderCell(i);
}

// Envoi : on saute une image si la précédente n'est pas encore partie
// (évite d'accumuler du retard sur un WiFi lent).
function sendFrame() {
  if (ws && ws.readyState === WebSocket.OPEN && ws.bufferedAmount < 1024) {
    ws.send(frame);
  }
}
const sendFrameThrottled = throttle(sendFrame, 40);

// Dessine une source (vidéo, image) recadrée au format de la matrice dans
// `frame`. Réduction en deux étapes pour un résultat plus doux.
const midCanvas = Object.assign(document.createElement('canvas'), { width: W * 10, height: H * 10 });
const smallCanvas = Object.assign(document.createElement('canvas'), { width: W, height: H });
const midCtx = midCanvas.getContext('2d');
const smallCtx = smallCanvas.getContext('2d', { willReadFrequently: true });

function captureSource(source, sourceW, sourceH, mirror) {
  const ratio = W / H;
  let sw = sourceW, sh = sourceH;
  if (sw / sh > ratio) sw = sh * ratio; else sh = sw / ratio;
  const sx = (sourceW - sw) / 2, sy = (sourceH - sh) / 2;

  midCtx.save();
  if (mirror) {
    midCtx.translate(midCanvas.width, 0);
    midCtx.scale(-1, 1);
  }
  midCtx.imageSmoothingQuality = 'high';
  midCtx.drawImage(source, sx, sy, sw, sh, 0, 0, midCanvas.width, midCanvas.height);
  midCtx.restore();

  smallCtx.imageSmoothingQuality = 'high';
  smallCtx.drawImage(midCanvas, 0, 0, W, H);
  const pixels = smallCtx.getImageData(0, 0, W, H).data;
  for (let i = 0; i < W * H; i++) {
    frame[i * 3] = pixels[i * 4];
    frame[i * 3 + 1] = pixels[i * 4 + 1];
    frame[i * 3 + 2] = pixels[i * 4 + 2];
  }
  renderPreview();
}

function startLive() {
  show('live');
  liveConfirmed = false;
  send('live on');
  sendFrame();
  selectTab(tab);
}

function stopLive(notify = true) {
  stopCamera();
  $('preview').classList.remove('drawing');
  if (notify) send('live off');
}

function selectTab(name) {
  tab = name;
  for (const button of document.querySelectorAll('.tabs button')) {
    button.classList.toggle('active', button.dataset.tab === name);
  }
  for (const t of ['camera', 'draw', 'image']) $(`tab-${t}`).hidden = t !== name;
  $('preview').classList.toggle('drawing', name === 'draw');

  if (name === 'camera') startCamera();
  else stopCamera();
}

for (const button of document.querySelectorAll('.tabs button')) {
  button.addEventListener('click', () => selectTab(button.dataset.tab));
}

// --- Caméra ---
let cameraStream = null;
let cameraTimer = null;
let facingMode = 'environment';

async function startCamera() {
  stopCamera();
  const video = $('webcam');

  if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
    $('origin-hint').textContent = location.origin;
    $('camera-warning').hidden = false;
    video.parentElement.hidden = true;
    $('flip-camera').hidden = true;
    return;
  }

  try {
    cameraStream = await navigator.mediaDevices.getUserMedia({
      video: { facingMode, width: { ideal: 320 }, height: { ideal: 240 } },
      audio: false,
    });
  } catch (err) {
    console.error('Erreur caméra :', err);
    toast("Impossible d'accéder à la caméra.");
    return;
  }
  if (view !== 'live' || tab !== 'camera') {  // l'utilisateur est parti entre-temps
    stopCamera();
    return;
  }

  const mirror = facingMode === 'user';  // caméra frontale : effet miroir naturel
  video.style.transform = mirror ? 'scaleX(-1)' : '';
  video.srcObject = cameraStream;
  await video.play().catch(() => {});

  cameraTimer = setInterval(() => {
    if (video.readyState < video.HAVE_CURRENT_DATA) return;
    captureSource(video, video.videoWidth, video.videoHeight, mirror);
    sendFrame();
  }, 66);  // ~15 images/s
}

function stopCamera() {
  clearInterval(cameraTimer);
  cameraTimer = null;
  if (cameraStream) cameraStream.getTracks().forEach((track) => track.stop());
  cameraStream = null;
  $('webcam').srcObject = null;
}

$('flip-camera').addEventListener('click', () => {
  facingMode = facingMode === 'environment' ? 'user' : 'environment';
  startCamera();
});

// --- Dessin ---
let eraser = false;

function hexToRgb(hex) {
  const n = parseInt(hex.slice(1), 16);
  return [(n >> 16) & 255, (n >> 8) & 255, n & 255];
}

function currentColor() {
  return eraser ? [0, 0, 0] : hexToRgb($('draw-color').value);
}

function paintAt(e) {
  const rect = $('preview').getBoundingClientRect();
  const x = Math.floor(((e.clientX - rect.left) / rect.width) * W);
  const y = Math.floor(((e.clientY - rect.top) / rect.height) * H);
  if (x < 0 || x >= W || y < 0 || y >= H) return;

  const i = y * W + x;
  const [r, g, b] = currentColor();
  if (frame[i * 3] === r && frame[i * 3 + 1] === g && frame[i * 3 + 2] === b) return;
  frame.set([r, g, b], i * 3);
  renderCell(i);
  sendFrameThrottled();
}

(() => {
  const preview = $('preview');
  preview.addEventListener('pointerdown', (e) => {
    if (tab !== 'draw') return;
    preview.setPointerCapture(e.pointerId);
    paintAt(e);
  });
  preview.addEventListener('pointermove', (e) => {
    if (tab === 'draw' && preview.hasPointerCapture(e.pointerId)) paintAt(e);
  });
})();

$('draw-eraser').addEventListener('click', () => {
  eraser = !eraser;
  $('draw-eraser').classList.toggle('active', eraser);
});
$('draw-color').addEventListener('input', () => {
  eraser = false;
  $('draw-eraser').classList.remove('active');
});
$('draw-fill').addEventListener('click', () => {
  const [r, g, b] = currentColor();
  for (let i = 0; i < W * H; i++) frame.set([r, g, b], i * 3);
  renderPreview();
  sendFrame();
});
$('draw-clear').addEventListener('click', () => {
  frame.fill(0);
  renderPreview();
  sendFrame();
});

// --- Image ---
$('image-file').addEventListener('change', (e) => {
  const file = e.target.files[0];
  if (!file) return;
  const img = new Image();
  img.onload = () => {
    captureSource(img, img.naturalWidth, img.naturalHeight, false);
    sendFrame();
    URL.revokeObjectURL(img.src);
  };
  img.onerror = () => toast("Impossible de lire cette image.");
  img.src = URL.createObjectURL(file);
  e.target.value = '';  // permet de re-choisir la même image
});

// ======================================================================
//  Démarrage
// ======================================================================

for (const card of document.querySelectorAll('[data-category]')) {
  card.addEventListener('click', () => showList(card.dataset.category));
}
$('open-live').addEventListener('click', startLive);
for (const button of document.querySelectorAll('button.back')) {
  button.addEventListener('click', goBack);
}

// Le téléphone se met en veille / change d'onglet : on coupe la caméra
document.addEventListener('visibilitychange', () => {
  if (document.hidden) stopCamera();
  else if (view === 'live' && tab === 'camera') startCamera();
});

renderPreview();
connect();
