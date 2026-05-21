/* =====================================================================
   dashboard.js — Logique de l'interface LIEBHERR Acquisition 2026
   Auteur  : BALA Halim
   Version : 0.2.0  (ajout tare/zérotage)
   ===================================================================== */

'use strict';

let isRecording = true;
let isSafe      = false;
let isTareOn    = false;


/* ══════════════════════════════════════════════════════════════════════
   MÉTRIQUES  (polling 1 s)
══════════════════════════════════════════════════════════════════════ */
function refreshMetrics() {
  fetch('/getSPS')
    .then(r => r.text())
    .then(v => { document.getElementById('sps-val').textContent = v || '—'; })
    .catch(() => {});

  fetch('/getVoltage')
    .then(r => r.text())
    .then(v => { document.getElementById('volt-val').textContent = parseFloat(v).toFixed(4) || '—'; })
    .catch(() => {});
}


/* ══════════════════════════════════════════════════════════════════════
   TARE — ZÉROTAGE
══════════════════════════════════════════════════════════════════════ */

/**
 * Envoie POST /tare pour déclencher la capture des offsets côté firmware.
 * Le bouton est désactivé le temps de la requête pour éviter les doubles clics.
 * Un court délai (150 ms) laisse le firmware capturer l'offset avant de
 * lire /tareStatus.
 */
function requestTare() {
  const btn   = document.getElementById('btn-tare');
  const label = document.getElementById('tare-btn-label');
  btn.disabled      = true;
  label.textContent = 'Capture en cours…';

  fetch('/tare', { method: 'POST' })
    .then(r => r.json())
    .then(() => {
      setTimeout(() => {
        btn.disabled = false;
        syncTareStatus();
      }, 150);
    })
    .catch(() => {
      btn.disabled      = false;
      label.textContent = 'Capturer le zéro (Tare)';
      alert('Erreur serveur — tare non appliquée.');
    });
}

/**
 * Envoie POST /resetTare pour remettre tous les offsets à zéro.
 */
function resetTare() {
  fetch('/resetTare', { method: 'POST' })
    .then(r => r.json())
    .then(() => syncTareStatus())
    .catch(() => alert('Erreur serveur — réinitialisation échouée.'));
}

/**
 * Lit GET /tareStatus et applique l'état reçu dans le DOM.
 * Appelé au démarrage et toutes les 2 s.
 */
function syncTareStatus() {
  fetch('/tareStatus')
    .then(r => r.json())
    .then(data => applyTareState(data.active, data.offsets))
    .catch(() => {});
}

/**
 * Met à jour le DOM complet de la section Tare selon l'état reçu du firmware.
 * @param {boolean} active   - true si la soustraction d'offset est active
 * @param {number[]} offsets - tableau de 4 offsets en Volts
 */
function applyTareState(active, offsets) {
  isTareOn = active;

  const btn        = document.getElementById('btn-tare');
  const label      = document.getElementById('tare-btn-label');
  const statusRow  = document.getElementById('tare-status-row');
  const dot        = document.getElementById('tare-dot');
  const statusText = document.getElementById('tare-status-text');

  if (active) {
    btn.classList.add('active');
    label.textContent   = 'Mettre à jour le zéro';
    btn.style.animation = 'none';

    statusRow.classList.remove('inactive');
    dot.classList.add('on');
    statusText.textContent = 'Tare active — soustraction des offsets en cours';

    offsets.forEach((val, i) => {
      const cell   = document.getElementById('off-' + i);
      const valEl  = document.getElementById('off-val-' + i);
      const isZero = Math.abs(val) < 1e-9;
      valEl.textContent = val.toFixed(6);
      valEl.className   = 'offset-value' + (isZero ? ' zeroed' : '');
      cell.className    = 'tare-offset-cell' + (isZero ? ' zeroed' : '');
    });

  } else {
    btn.classList.remove('active');
    label.textContent = 'Capturer le zéro (Tare)';

    statusRow.classList.add('inactive');
    dot.classList.remove('on');
    statusText.textContent = 'Aucun zéro capturé — valeurs brutes affichées';

    for (let i = 0; i < 4; i++) {
      document.getElementById('off-val-' + i).textContent = '—';
      document.getElementById('off-val-' + i).className   = 'offset-value';
      document.getElementById('off-' + i).className       = 'tare-offset-cell';
    }
  }
}


/* ══════════════════════════════════════════════════════════════════════
   TOGGLE ENREGISTREMENT SD
══════════════════════════════════════════════════════════════════════ */
function toggleSD() {
  fetch('/toggleSD', { method: 'POST' })
    .then(r => r.text())
    .then(s => {
      isRecording = (s === 'RECORDING');
      const btn   = document.getElementById('btn-toggle');
      const label = document.getElementById('toggle-label');
      if (isRecording) {
        btn.classList.remove('stopped');
        label.textContent = "Arrêter l'enregistrement SD";
      } else {
        btn.classList.add('stopped');
        label.textContent = "Reprendre l'enregistrement SD";
      }
    })
    .catch(() => alert('Erreur serveur.'));
}


/* ══════════════════════════════════════════════════════════════════════
   ÉJECTION SÉCURISÉE SD
══════════════════════════════════════════════════════════════════════ */
function syncEjectStatus() {
  fetch('/ejectStatus')
    .then(r => r.text())
    .then(s => applyEjectState(s === 'SAFE'))
    .catch(() => {});
}

function ejectSD() {
  fetch('/ejectSD', { method: 'POST' })
    .then(r => r.text())
    .then(s => applyEjectState(s === 'SAFE'))
    .catch(() => alert('Erreur serveur.'));
}

function applyEjectState(safe) {
  isSafe = safe;
  const btnEject  = document.getElementById('btn-eject');
  const ejectLbl  = document.getElementById('eject-label');
  const led       = document.getElementById('led');
  const ledLabel  = document.getElementById('led-label');
  const statusBar = document.getElementById('status-bar');

  if (safe) {
    btnEject.classList.add('safe');
    ejectLbl.textContent  = 'Carte safe — cliquez pour reprendre';
    led.classList.add('on');
    ledLabel.innerHTML    = '<strong>LED GPIO 26</strong> — ALLUMÉE : carte safe, retirez-la';
    statusBar.textContent = 'ÉJECTION SÉCURISÉE ACTIVE — écriture SD bloquée';
    statusBar.style.color = '#1E9E4E';
  } else {
    btnEject.classList.remove('safe');
    ejectLbl.textContent  = 'Éjecter la carte SD';
    led.classList.remove('on');
    ledLabel.innerHTML    = '<strong>LED GPIO 26</strong> — éteinte : écriture en cours';
    statusBar.textContent = 'Acquisition active — 192.168.4.1';
    statusBar.style.color = '';
  }
}


/* ══════════════════════════════════════════════════════════════════════
   FILE BROWSER — LISTE / TÉLÉCHARGEMENT / SUPPRESSION
══════════════════════════════════════════════════════════════════════ */
function formatSize(bytes) {
  if (bytes < 1024)    return bytes + ' o';
  if (bytes < 1048576) return (bytes / 1024).toFixed(1) + ' Ko';
  return (bytes / 1048576).toFixed(2) + ' Mo';
}

function loadFiles() {
  const btn       = document.getElementById('btn-refresh');
  const container = document.getElementById('file-list-container');
  btn.classList.add('spinning');

  fetch('/listFiles')
    .then(r => r.json())
    .then(files => {
      btn.classList.remove('spinning');

      if (files.length === 0) {
        container.innerHTML = `
          <div class="file-empty">
            <svg width="40" height="40" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
              <path d="M13 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V9z"/>
              <polyline points="13 2 13 9 20 9"/>
            </svg>
            Aucun fichier .csv sur la carte SD
          </div>`;
        return;
      }

      let html = '<ul class="file-list">';
      files.forEach(f => {
        html += `
          <li class="file-item" id="row-${CSS.escape(f.name)}">
            <div class="file-icon">
              <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">
                <path d="M13 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V9z"/>
                <polyline points="13 2 13 9 20 9"/>
                <line x1="8" y1="13" x2="16" y2="13"/>
                <line x1="8" y1="17" x2="12" y2="17"/>
              </svg>
            </div>
            <div class="file-info">
              <div class="file-name">${f.name}</div>
              <div class="file-size">${formatSize(f.size)}</div>
            </div>
            <div class="file-actions">
              <a class="btn-dl"
                 href="/downloadFile?name=${encodeURIComponent(f.name)}"
                 download="${f.name}">
                <svg class="icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
                  <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/>
                  <polyline points="7 10 12 15 17 10"/>
                  <line x1="12" y1="15" x2="12" y2="3"/>
                </svg>
                DL
              </a>
              <button class="btn-del" onclick="deleteFile('${f.name}')">
                <svg class="icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
                  <polyline points="3 6 5 6 21 6"/>
                  <path d="M19 6l-1 14a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2L5 6"/>
                  <path d="M10 11v6"/><path d="M14 11v6"/>
                  <path d="M9 6V4h6v2"/>
                </svg>
                Suppr.
              </button>
            </div>
          </li>`;
      });
      html += '</ul>';
      container.innerHTML = html;
    })
    .catch(() => {
      btn.classList.remove('spinning');
      container.innerHTML = `
        <div class="file-empty">
          <svg width="40" height="40" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
            <circle cx="12" cy="12" r="10"/>
            <line x1="12" y1="8" x2="12" y2="12"/>
            <line x1="12" y1="16" x2="12.01" y2="16"/>
          </svg>
          Erreur — carte SD inaccessible
        </div>`;
    });
}

function deleteFile(name) {
  const formData = new FormData();
  formData.append('name', name);

  fetch('/deleteFile', { method: 'POST', body: formData })
    .then(r => {
      if (r.status === 403) {
        alert('Éjection sécurisée active — désactivez-la avant de supprimer.');
        return;
      }
      if (!r.ok) { alert('Erreur lors de la suppression.'); return; }

      const row = document.getElementById('row-' + CSS.escape(name));
      if (row) {
        row.style.transition = 'opacity .25s, max-height .3s';
        row.style.opacity    = '0';
        row.style.maxHeight  = '0';
        row.style.overflow   = 'hidden';
        setTimeout(() => {
          row.remove();
          const ul = document.querySelector('.file-list');
          if (ul && ul.children.length === 0) loadFiles();
        }, 320);
      }
    })
    .catch(() => alert('Erreur serveur.'));
}


/* ══════════════════════════════════════════════════════════════════════
   INIT — exécuté au chargement de la page
══════════════════════════════════════════════════════════════════════ */
syncEjectStatus();
syncTareStatus();
refreshMetrics();
loadFiles();

setInterval(refreshMetrics, 1000);   /* métriques toutes les 1 s  */
setInterval(syncTareStatus,  2000);  /* état tare toutes les 2 s  */
