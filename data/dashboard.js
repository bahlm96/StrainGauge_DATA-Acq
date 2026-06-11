'use strict';
 
const MAX_POINTS     = 300;
const FETCH_INTERVAL = 100;   // ms
 
const GAUGE_CONFIG = [
  { id: 'j1', label: 'Jauge 1', color: '#1A6BBF', container: 'chart-j1', current: 'ch-j1', metric: 'm-j1' },
  { id: 'j2', label: 'Jauge 2', color: '#E05C2A', container: 'chart-j2', current: 'ch-j2', metric: 'm-j2' },
  { id: 'j3', label: 'Jauge 3', color: '#1E9E4E', container: 'chart-j3', current: 'ch-j3', metric: 'm-j3' },
  { id: 'j4', label: 'Jauge 4', color: '#8B3FBF', container: 'chart-j4', current: 'ch-j4', metric: 'm-j4' },
];
 
/* ── État global ─────────────────────────────────────────────────────── */
let state         = 'stopped';
let fetchTimer    = null;
let visibleGauges = [true, true, true, true];
let uPlots        = [null, null, null, null];
let chartData     = [];
let ejectState    = 'WRITING';   // 'WRITING' ou 'SAFE'
let _sensorMask   = 0x0F;        // masque courant (mis à jour par wzApplyAndStart)
 
/* ── Largeur graphes ────────────────────────────────────────────────── */
function chartWidth() {
  const mainW = Math.min(window.innerWidth - 40, 1060);
  return Math.max(200, Math.floor((mainW - 14) / 2));
}
 
/* ── Options uPlot ──────────────────────────────────────────────────── */
function buildOpts(cfg) {
  return {
    width:  chartWidth(),
    height: 220,
    cursor: { show: true, drag: { x: false, y: false } },
    legend: { show: false },
    axes: [
      {
        stroke: '#AAAAAA', grid: { stroke: '#F0F0F0', width: 1 },
        ticks: { stroke: '#E8E8E8' }, label: 'Temps (s)', labelSize: 16,
        font: '11px Barlow, sans-serif', labelFont: '11px Barlow, sans-serif',
      },
      {
        stroke: '#AAAAAA', grid: { stroke: '#F0F0F0', width: 1 },
        ticks: { stroke: '#E8E8E8' }, label: 'V', labelSize: 20, size: 68,
        font: '11px Barlow, sans-serif', labelFont: '11px Barlow, sans-serif',
      },
    ],
    scales: { x: { time: false }, y: { auto: true } },
    series: [
      {},
      { show: true, label: cfg.label, stroke: cfg.color, width: 2, spanGaps: true, points: { show: false } },
    ],
  };
}
 
/* ── Init graphes ───────────────────────────────────────────────────── */
function initCharts() {
  chartData = GAUGE_CONFIG.map(() => [[], []]);
  GAUGE_CONFIG.forEach((cfg, i) => {
    const el = document.getElementById(cfg.container);
    if (!el) return;
    el.innerHTML = '';
    if (typeof uPlot === 'undefined') {
      el.innerHTML = '<p style="padding:16px;color:#999;font-size:13px;">uPlot non chargé.</p>';
      return;
    }
    try { uPlots[i] = new uPlot(buildOpts(cfg), [[], []], el); }
    catch (e) { el.innerHTML = '<p style="padding:16px;color:#c00;">Erreur uPlot : ' + e.message + '</p>'; }
  });
}
 
function resizeCharts() {
  const w = chartWidth();
  uPlots.forEach(u => { if (u) u.setSize({ width: w, height: 220 }); });
}
window.addEventListener('resize', resizeCharts);
 
/* ── Visibilité jauges (contrôlée par le wizard) ────────────────────── */
function applyVisibility() {
  GAUGE_CONFIG.forEach((cfg, i) => {
    const card = document.getElementById('card-' + cfg.id);
    if (card) card.classList.toggle('hidden', !visibleGauges[i]);
  });
}

/* ══════════════════════════════════════════════════════════════════════
   WIZARD — sélection capteurs
   ══════════════════════════════════════════════════════════════════════

   wzState : { step: 0|1|2|3, quota: 1-4|'all', selected: Set<0..3> }
   step 0 = idle / step 1 = choix quantité / step 2 = choix canaux / step 3 = running
*/
const wzState = { step: 0, quota: null, selected: new Set() };
const CH_COLORS = ['#1A6BBF', '#E05C2A', '#1E9E4E', '#8B3FBF'];

function wzShowStep(n) {
  [0,1,2,3].forEach(i => {
    const el = document.getElementById('wz-step' + i);
    if (el) el.hidden = (i !== n);
  });
  wzState.step = n;
}

function wzBuildMask() {
  let m = 0;
  wzState.selected.forEach(i => { m |= (1 << i); });
  return m;
}

/* ── Étape 1 : boutons de quantité ─────────────────────────────────── */
document.querySelectorAll('.wz-count-btn').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.wz-count-btn').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');
    const val = btn.dataset.count;
    wzState.quota = (val === 'all') ? 'all' : parseInt(val);

    if (wzState.quota === 'all') {
      /* ALL → sélectionne tout et démarre directement */
      wzState.selected = new Set([0,1,2,3]);
      wzApplyAndStart();
    } else {
      /* Passe à l'étape 2 */
      wzState.selected.clear();
      wzRefreshChBtns();
      document.getElementById('wz-quota-label').textContent = wzState.quota;
      wzUpdateHint();
      wzShowStep(2);
    }
  });
});

/* ── Étape 2 : boutons de canaux ────────────────────────────────────── */
function wzRefreshChBtns() {
  document.querySelectorAll('.wz-ch-btn').forEach(btn => {
    const ch = parseInt(btn.dataset.ch);
    btn.classList.toggle('active', wzState.selected.has(ch));
  });
  wzUpdateStartBtn();
}

function wzUpdateHint() {
  const hint = document.getElementById('wz-ch-hint');
  const sel  = wzState.selected.size;
  const q    = wzState.quota;
  if (sel < q)       hint.textContent = `Sélectionnez encore ${q - sel} capteur(s)`;
  else if (sel === q) hint.textContent = `✓ ${q} capteur(s) sélectionné(s)`;
}

function wzUpdateStartBtn() {
  const btn = document.getElementById('btn-start');
  if (btn) btn.disabled = (wzState.selected.size !== wzState.quota);
}

document.querySelectorAll('.wz-ch-btn').forEach(btn => {
  btn.addEventListener('click', () => {
    const ch = parseInt(btn.dataset.ch);
    if (wzState.selected.has(ch)) {
      wzState.selected.delete(ch);
    } else {
      if (wzState.quota !== 'all' && wzState.selected.size >= wzState.quota) return;
      wzState.selected.add(ch);
    }
    wzRefreshChBtns();
    wzUpdateHint();
  });
});

/* ── Lancer la visualisation ────────────────────────────────────────── */
function wzApplyAndStart() {
  /* Met à jour visibleGauges selon la sélection */
  visibleGauges = [false, false, false, false];
  wzState.selected.forEach(i => { visibleGauges[i] = true; });
  applyVisibility();

  /* Publie le masque vers l'ESP32 */
  const mask = wzBuildMask();
  _sensorMask = mask;   // mémorise pour le toggle enregistrement
  const fd   = new FormData();
  fd.append('mask',   mask);
  fd.append('record', 'true');
  fetch('/setSDConfig', { method: 'POST', body: fd }).catch(() => {});

  /* Met à jour les chips de l'étape 3 */
  const chipsEl = document.getElementById('wz-running-chips');
  chipsEl.innerHTML = '';
  GAUGE_CONFIG.forEach((cfg, i) => {
    if (!visibleGauges[i]) return;
    const chip = document.createElement('span');
    chip.className = 'wz-chip';
    chip.style.background = cfg.color;
    chip.innerHTML = `<span class="wz-chip-dot"></span>${cfg.label}`;
    chipsEl.appendChild(chip);
  });

  wzShowStep(3);
  setUIState('running');
}

/* ── Bouton Visualiser (étape 2) ────────────────────────────────────── */
const btnStart = document.getElementById('btn-start');
if (btnStart) btnStart.addEventListener('click', () => {
  if (wzState.selected.size === wzState.quota) wzApplyAndStart();
});

/* ── Bouton Pause ───────────────────────────────────────────────────── */
const btnPause = document.getElementById('btn-pause');
if (btnPause) btnPause.addEventListener('click', () => {
  if (state === 'running')     setUIState('paused');
  else if (state === 'paused') setUIState('running');
});

/* ── Bouton Arrêter ─────────────────────────────────────────────────── */
document.getElementById('btn-stop').addEventListener('click', () => {
  fetch('/stopRecord', { method: 'POST' }).catch(() => {});
  setUIState('stopped');
  wzState.selected.clear();
  wzState.quota = null;
  document.querySelectorAll('.wz-count-btn').forEach(b => b.classList.remove('active'));
  wzShowStep(0);
});

/* ── Navigation retour ──────────────────────────────────────────────── */
document.getElementById('btn-wz-config').addEventListener('click', () => wzShowStep(1));
document.getElementById('wz1-back').addEventListener('click', () => wzShowStep(0));
document.getElementById('wz2-back').addEventListener('click', () => {
  wzState.selected.clear();
  wzRefreshChBtns();
  wzShowStep(1);
});
 
/* ── Fetch données temps réel ───────────────────────────────────────── */
async function fetchAndUpdate() {
  try {
    const res  = await fetch('/data');
    if (!res.ok) return;
    const json = await res.json();
    const vals = [
      json.j1 !== undefined ? json.j1 : (json.v || 0),
      json.j2 !== undefined ? json.j2 : 0,
      json.j3 !== undefined ? json.j3 : 0,
      json.j4 !== undefined ? json.j4 : 0,
    ];
    const t = json.t !== undefined ? json.t : (performance.now() / 1000);
    GAUGE_CONFIG.forEach((cfg, i) => {
      chartData[i][0].push(t);
      chartData[i][1].push(vals[i]);
      if (chartData[i][0].length > MAX_POINTS) { chartData[i][0].shift(); chartData[i][1].shift(); }
      if (visibleGauges[i] && uPlots[i]) uPlots[i].setData(chartData[i]);
      const curEl = document.getElementById(cfg.current);
      const metEl = document.getElementById(cfg.metric);
      if (curEl) curEl.textContent = vals[i].toFixed(4) + ' V';
      if (metEl) metEl.textContent = vals[i].toFixed(6);
    });
  } catch (_) {}
}
 
/* ── SPS ────────────────────────────────────────────────────────────── */
async function refreshSPS() {
  try {
    const r = await fetch('/getSPS');
    const v = await r.text();
    const el = document.getElementById('m-sps');
    if (el) el.textContent = v.trim() || '—';
  } catch (_) {}
}
 
/* ── État UI start/pause/stop ───────────────────────────────────────── */
function resetDisplayValues() {
  GAUGE_CONFIG.forEach(cfg => {
    const c = document.getElementById(cfg.current);
    const m = document.getElementById(cfg.metric);
    if (c) c.textContent = '—';
    if (m) m.textContent = '—';
  });
}
 
function setUIState(newState) {
  state = newState;
  const dot      = document.getElementById('status-dot');
  const label    = document.getElementById('status-label');
  const btnPause = document.getElementById('btn-pause');
  dot.className  = 'status-dot';

  if (newState === 'running') {
    dot.classList.add('running');
    label.textContent = 'En cours';
    if (btnPause) btnPause.disabled = false;
    if (!fetchTimer) fetchTimer = setInterval(fetchAndUpdate, FETCH_INTERVAL);
  } else if (newState === 'paused') {
    dot.classList.add('paused');
    label.textContent = 'Pause';
    if (btnPause) btnPause.disabled = true;
    clearInterval(fetchTimer); fetchTimer = null;
  } else {
    dot.classList.add('stopped');
    label.textContent = 'Arrêté';
    if (btnPause) btnPause.disabled = true;
    clearInterval(fetchTimer); fetchTimer = null;
    chartData = GAUGE_CONFIG.map(() => [[], []]);
    uPlots.forEach(u => { if (u) u.setData([[], []]); });
    resetDisplayValues();
  }
}
 
/* btn-start et btn-pause sont gérés par le wizard ci-dessus */
 
/* ── Menu hamburger ─────────────────────────────────────────────────── */
const btnMenu  = document.getElementById('btn-menu');
const dropdown = document.getElementById('dropdown');
 
btnMenu.addEventListener('click', e => {
  e.stopPropagation();
  const open = !dropdown.hidden;
  dropdown.hidden = open;
  btnMenu.classList.toggle('open', !open);
});
document.addEventListener('click', () => {
  dropdown.hidden = true;
  btnMenu.classList.remove('open');
});
dropdown.addEventListener('click', e => e.stopPropagation());
 
/* ── SD STATE ────────────────────────────────────────────────────────── */
const SD_TOTAL_FALLBACK = 4 * 1024 * 1024 * 1024; // 4 Go (fallback si l'ESP ne renvoie pas total)
 
function fmtBytes(b) {
  if (b < 1024)         return b + ' o';
  if (b < 1024 * 1024)  return (b / 1024).toFixed(1) + ' Ko';
  if (b < 1024 ** 3)    return (b / 1024 ** 2).toFixed(1) + ' Mo';
  return (b / 1024 ** 3).toFixed(2) + ' Go';
}
 
function setSDStateUI(used, total) {
  const free  = Math.max(0, total - used);
  const pct   = Math.min(100, Math.round(used / total * 100));
  const fill  = document.getElementById('sd-fill');
  const pctEl = document.getElementById('sd-pct');
  const usedEl= document.getElementById('sd-used');
  const freeEl= document.getElementById('sd-free');
  const totEl = document.getElementById('sd-total');
  const detEl = document.getElementById('sdstate-detail');
 
  if (fill) {
    fill.style.width = pct + '%';
    fill.className = 'sdstate-fill' + (pct >= 90 ? ' crit' : pct >= 70 ? ' warn' : '');
  }
  if (pctEl)  pctEl.textContent  = pct + ' %';
  if (freeEl) freeEl.textContent = fmtBytes(free);
  if (usedEl) usedEl.textContent = fmtBytes(used);
  if (totEl)  totEl.textContent  = fmtBytes(total);
  if (detEl)  detEl.style.color  = '';
}
 
function setSDStateError(msg) {
  const pctEl = document.getElementById('sd-pct');
  const detEl = document.getElementById('sdstate-detail');
  const fill  = document.getElementById('sd-fill');
  if (pctEl)  pctEl.textContent  = '—';
  if (fill)   fill.style.width   = '0%';
  if (detEl) { detEl.textContent = msg; detEl.style.color = 'var(--red)'; }
}
 
async function refreshSDState() {
  const btn = document.getElementById('btn-sd-refresh');
  if (btn) { btn.disabled = true; btn.querySelector('span').textContent = '⏳'; }
 
  try {
    const r = await fetch('/sdState');
    if (!r.ok) { setSDStateError('Erreur ' + r.status); return; }
    const data = await r.json();
    if (data.error) { setSDStateError('Timeout ESP32'); return; }
    // L'ESP32 renvoie les valeurs en Ko (unit:"Ko") pour éviter l'overflow JSON 32 bits
    const mult  = (data.unit === 'Ko') ? 1024 : 1;
    const used  = (Number(data.used)  || 0) * mult;
    const total = (Number(data.total) || SD_TOTAL_FALLBACK / mult) * mult;
    setSDStateUI(used, total);
  } catch (_) {
    setSDStateError('ESP32 injoignable');
  } finally {
    if (btn) { btn.disabled = false; btn.querySelector('span').textContent = '↻'; }
  }
}
 
document.getElementById('btn-sd-refresh').addEventListener('click', refreshSDState);
 
/* ── CONFIGURATION ───────────────────────────────────────────────────── */
const cfgRecordToggle = document.getElementById('cfg-record-toggle');
const cfgRecordLabel  = document.getElementById('cfg-record-label');
 
cfgRecordToggle.addEventListener('change', async () => {
  const rec = cfgRecordToggle.checked;
  cfgRecordLabel.textContent = rec ? 'Actif' : 'Inactif';
  // Synchronise avec l'ESP32 en utilisant le masque courant du wizard
  try {
    const fd = new FormData();
    const mask = wzBuildMask();
    fd.append('mask',   mask || _sensorMask || 15);
    fd.append('record', rec ? 'true' : 'false');
    await fetch('/setSDConfig', { method: 'POST', body: fd });
    if (!rec && state === 'running') {
      // Stoppe aussi le fetch visuel si l'utilisateur coupe l'enregistrement
      fetch('/stopRecord', { method: 'POST' }).catch(() => {});
    }
  } catch (_) {}
});
 
/* ── SAFE EJECT ─────────────────────────────────────────────────────── */
const btnEject        = document.getElementById('btn-eject');
const btnEjectReset   = document.getElementById('btn-eject-reset');
const btnBannerReset  = document.getElementById('btn-banner-reset');
const ejectBanner     = document.getElementById('eject-banner');
const ejectLabel      = document.getElementById('eject-label');
 
function setEjectState(safe) {
  ejectState = safe ? 'SAFE' : 'WRITING';
  ejectBanner.style.display  = safe ? '' : 'none';
  btnEject.style.display      = safe ? 'none' : '';
  btnEjectReset.style.display = safe ? '' : 'none';
  if (ejectLabel) ejectLabel.textContent = safe
    ? '✓ SD éjectée'
    : 'Éjecter en toute sécurité';
}
 
btnEject.addEventListener('click', async () => {
  btnEject.disabled = true;
  btnEject.textContent = '⏳ Éjection…';
  try {
    const r = await fetch('/ejectSD', { method: 'POST' });
    if (r.ok) {
      const txt = await r.text();
      setEjectState(txt.trim() === 'SAFE');
    } else {
      alert('Erreur : réponse inattendue (' + r.status + ')');
    }
  } catch (_) {
    alert('Impossible de contacter l\'ESP32.');
  }
  btnEject.disabled = false;
  if (ejectState !== 'SAFE') btnEject.textContent = '⏏ Éjecter en toute sécurité';
  // Ferme le menu
  dropdown.hidden = true;
  btnMenu.classList.remove('open');
});
 
async function doEjectReset() {
  try {
    await fetch('/ejectReset', { method: 'POST' });
    setEjectState(false);
  } catch (_) {
    alert('Impossible de contacter l\'ESP32.');
  }
}
 
btnEjectReset.addEventListener('click', doEjectReset);
btnBannerReset.addEventListener('click', doEjectReset);
 
/* Vérifie l'état éjection au chargement */
async function checkEjectStatus() {
  try {
    const r = await fetch('/ejectStatus');
    const v = await r.text();
    setEjectState(v.trim() === 'SAFE');
  } catch (_) {}
}
 
/* ── MODAL RENOMMAGE ─────────────────────────────────────────────────── */
(function injectModal() {
  const overlay = document.createElement('div');
  overlay.id = 'dl-modal-overlay';
  overlay.innerHTML = `
    <div class="dl-modal" id="dl-modal" role="dialog" aria-modal="true">
      <div class="dl-modal-header">
        <span class="dl-modal-title">⬇ Télécharger le fichier</span>
        <button class="dl-modal-close" id="dl-modal-close" aria-label="Fermer">✕</button>
      </div>
      <div class="dl-modal-body">
        <label class="dl-modal-label">Fichier source sur la SD</label>
        <div class="dl-modal-source" id="dl-modal-source">—</div>
        <label class="dl-modal-label" for="dl-modal-input">Nom du fichier à enregistrer</label>
        <input class="dl-modal-input" id="dl-modal-input" type="text" placeholder="mon_essai.csv" spellcheck="false">
        <div class="dl-modal-hint" id="dl-modal-hint"></div>
      </div>
      <div class="dl-modal-footer">
        <button class="dl-modal-cancel" id="dl-modal-cancel">Annuler</button>
        <button class="dl-modal-confirm" id="dl-modal-confirm">⬇ Télécharger</button>
      </div>
    </div>`;
  document.body.appendChild(overlay);
 
  const closeModal = () => { overlay.style.display = 'none'; };
  document.getElementById('dl-modal-close').addEventListener('click',  closeModal);
  document.getElementById('dl-modal-cancel').addEventListener('click', closeModal);
  overlay.addEventListener('click', e => { if (e.target === overlay) closeModal(); });
})();
 
function openDownloadModal(sdName, sizeFmt) {
  const overlay  = document.getElementById('dl-modal-overlay');
  const sourceEl = document.getElementById('dl-modal-source');
  const input    = document.getElementById('dl-modal-input');
  const hint     = document.getElementById('dl-modal-hint');
  const confirm  = document.getElementById('dl-modal-confirm');
 
  // Pré-remplit avec le nom de base sans extension, propre
  const baseName = sdName.replace(/\.csv$/i, '');
  sourceEl.textContent  = sdName + (sizeFmt ? '  (' + sizeFmt + ')' : '');
  input.value           = baseName;
  hint.textContent      = '';
  confirm.disabled      = false;
  confirm.textContent   = '⬇ Télécharger';
 
  overlay.style.display = 'flex';
  input.focus();
  input.select();
 
  // Retire l'ancien listener pour éviter les doublons
  const newConfirm = confirm.cloneNode(true);
  confirm.parentNode.replaceChild(newConfirm, confirm);
 
  newConfirm.addEventListener('click', async () => {
    let saveName = input.value.trim();
    if (!saveName) { hint.textContent = 'Veuillez saisir un nom.'; return; }
    if (!saveName.endsWith('.csv')) saveName += '.csv';
 
    newConfirm.disabled    = true;
    newConfirm.textContent = '⏳ Téléchargement…';
    hint.textContent       = '';
 
    try {
      const url = '/downloadFile?name=' + encodeURIComponent(sdName) +
                  '&saveas=' + encodeURIComponent(saveName);
      const res = await fetch(url);
      if (!res.ok) {
        hint.style.color   = '#c00';
        hint.textContent   = '❌ Erreur ' + res.status + ' — ' + (await res.text());
        newConfirm.disabled    = false;
        newConfirm.textContent = '⬇ Télécharger';
        return;
      }
      const blob = await res.blob();
      if (blob.size === 0) {
        hint.style.color = '#c00';
        hint.textContent = '❌ Fichier vide reçu.';
        newConfirm.disabled    = false;
        newConfirm.textContent = '⬇ Télécharger';
        return;
      }
      // Déclenche le téléchargement navigateur
      const a = document.createElement('a');
      a.href     = URL.createObjectURL(blob);
      a.download = saveName;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(a.href);
 
      hint.style.color = 'var(--green)';
      hint.textContent = '✓ Téléchargement lancé : ' + saveName;
      newConfirm.textContent = '✓ Fait';
      setTimeout(() => {
        document.getElementById('dl-modal-overlay').style.display = 'none';
      }, 1200);
    } catch (err) {
      hint.style.color   = '#c00';
      hint.textContent   = '❌ ' + err.message;
      newConfirm.disabled    = false;
      newConfirm.textContent = '⬇ Télécharger';
    }
  });
 
  // Valide avec Entrée
  input.onkeydown = e => { if (e.key === 'Enter') newConfirm.click(); };
}
 
/* ── Téléchargement (step 0 + step 3) ───────────────────────────────── */
function toggleFileSection() {
  const sec = document.getElementById('file-section');
  sec.style.display = sec.style.display === 'none' ? '' : 'none';
  if (sec.style.display !== 'none') loadFileList();
}
document.getElementById('btn-dl').addEventListener('click', toggleFileSection);
document.getElementById('btn-dl2').addEventListener('click', toggleFileSection);
 
document.getElementById('btn-refresh-files').addEventListener('click', loadFileList);
 
async function loadFileList() {
  const wrap = document.getElementById('file-list-wrap');
  wrap.innerHTML = '<p class="file-empty">⏳ Chargement de la liste...</p>';
  try {
    const res = await fetch('/listFiles');
    if (!res.ok) {
      wrap.innerHTML = `<p class="file-empty" style="color:#c00">❌ Erreur serveur HTTP ${res.status}</p>`;
      return;
    }
    const rawText = await res.text();
    let files;
    try { files = JSON.parse(rawText); }
    catch (e) {
      wrap.innerHTML = `<p class="file-empty" style="color:#c00">❌ Réponse invalide : ${rawText.substring(0,80)}</p>`;
      return;
    }
    if (!Array.isArray(files) || files.length === 0) {
      wrap.innerHTML = '<p class="file-empty">Aucun fichier sur la carte SD.</p>';
      return;
    }
    const ul = document.createElement('ul');
    ul.className = 'file-list';
    files.forEach(f => {
      const li = document.createElement('li');
      li.className = 'file-item';
      const sizeFmt = fmtSize(f.size);
      li.innerHTML = `
        <span class="file-name">${f.name}</span>
        <span class="file-size">${sizeFmt}</span>
        <button class="btn-file-dl" data-name="${f.name}" data-size="${sizeFmt}">⬇ Télécharger</button>
        <button class="btn-file-del" data-name="${f.name}">✕</button>`;
      li.querySelector('.btn-file-dl').addEventListener('click', e => {
        openDownloadModal(e.currentTarget.dataset.name, e.currentTarget.dataset.size);
      });
      li.querySelector('.btn-file-del').addEventListener('click', () => deleteFile(f.name));
      ul.appendChild(li);
    });
    wrap.innerHTML = '';
    wrap.appendChild(ul);
  } catch (err) {
    wrap.innerHTML = `<p class="file-empty" style="color:#c00">❌ ${err.message}</p>`;
  }
}
 
async function deleteFile(name) {
  if (!confirm('Supprimer ' + name + ' ?')) return;
  const fd = new FormData();
  fd.append('name', name);
  try {
    const r = await fetch('/deleteFile', { method: 'POST', body: fd });
    if (r.ok) loadFileList();
    else alert('Erreur lors de la suppression.');
  } catch (_) { alert('Impossible de contacter la carte SD.'); }
}
 
function fmtSize(b) {
  if (b < 1024)         return b + ' o';
  if (b < 1024 * 1024)  return (b / 1024).toFixed(1) + ' Ko';
  return (b / (1024 * 1024)).toFixed(2) + ' Mo';
}
 
 
/* ── INIT ────────────────────────────────────────────────────────────── */
function waitForUplot(cb, tries) {
  tries = tries || 0;
  if (typeof uPlot !== 'undefined') { cb(); return; }
  if (tries > 50) { console.warn('uPlot non disponible après 5 s.'); return; }
  setTimeout(() => waitForUplot(cb, tries + 1), 100);
}
 
document.addEventListener('DOMContentLoaded', () => {
  waitForUplot(() => {
    initCharts();
    applyVisibility();
    setUIState('stopped');
    wzShowStep(0);           // wizard commence à l'étape 0
    checkEjectStatus();
    refreshSDState();
    setInterval(refreshSDState, 30000);
  });
});