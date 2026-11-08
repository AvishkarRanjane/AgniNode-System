/* ==========================================================================
   AgniNode IoT Safety Console - Live Hardware Client (Dual-MCU Architecture)
   Subscribes in real time to Firebase Firestore telemetry:
   Keys: v_rms, smoke_analog, smoke_dig, temp, flame, fire, arc_fault, shock, ssr_tripped
   ========================================================================== */

(() => {
  const $ = (s) => document.querySelector(s);

  let currentData = {
    v_rms: 230.0,
    smoke_analog: 0.8,
    smoke_dig: 0,
    temp: 28.0,
    flame: 1,
    fire: false,
    arc_fault: false,
    shock: false,
    ssr_tripped: false,
    gatewayOnline: false,
    stm32Online: false,
    updatedAt: 0,
  };

  let audioCtx = null;
  let sirenOsc = null;
  let sirenGain = null;
  let isAudioMuted = false;
  let isSirenPlaying = false;
  let authUser = null;
  let db = null;
  let auth = null;
  let firestoreFns = null;

  /* ---------- Web Audio Siren ---------- */
  function initAudio() {
    try {
      if (!audioCtx) {
        const AudioClass = window.AudioContext || window.webkitAudioContext;
        if (AudioClass) audioCtx = new AudioClass();
      }
      if (audioCtx && audioCtx.state === 'suspended') {
        audioCtx.resume();
      }
    } catch (e) {}
  }

  function startSiren() {
    if (isSirenPlaying || isAudioMuted) return;
    try {
      initAudio();
      if (!audioCtx) return;

      sirenOsc = audioCtx.createOscillator();
      sirenOsc.type = 'sawtooth';
      sirenOsc.frequency.setValueAtTime(880, audioCtx.currentTime);

      const mod = audioCtx.createOscillator();
      mod.type = 'square';
      mod.frequency.setValueAtTime(3.5, audioCtx.currentTime);

      const modGain = audioCtx.createGain();
      modGain.gain.setValueAtTime(120, audioCtx.currentTime);
      mod.connect(modGain);
      modGain.connect(sirenOsc.frequency);
      mod.start();

      sirenGain = audioCtx.createGain();
      sirenGain.gain.setValueAtTime(0.15, audioCtx.currentTime);

      sirenOsc.connect(sirenGain);
      sirenGain.connect(audioCtx.destination);
      sirenOsc.start();
      isSirenPlaying = true;

      const muteBtn = $('#btnAudioMute');
      if (muteBtn) muteBtn.classList.add('is-sounding');
    } catch (e) {}
  }

  function stopSiren() {
    if (!isSirenPlaying) return;
    try {
      if (sirenGain && audioCtx) {
        sirenGain.gain.linearRampToValueAtTime(0.001, audioCtx.currentTime + 0.05);
      }
      setTimeout(() => {
        try {
          if (sirenOsc) {
            sirenOsc.stop();
            sirenOsc.disconnect();
            sirenOsc = null;
          }
        } catch (e) {}
        isSirenPlaying = false;
        const muteBtn = $('#btnAudioMute');
        if (muteBtn) muteBtn.classList.remove('is-sounding');
      }, 60);
    } catch (e) {
      isSirenPlaying = false;
    }
  }

  /* ---------- Update UI with Live Hardware Telemetry ---------- */
  function renderLive() {
    const d = currentData;
    const banner = $('#banner');
    const bS = $('#bS');
    const bSub = $('#bSub');
    const conn = $('#conn');

    // Connection indicator
    const isOnline = d.gatewayOnline !== false;
    if (isOnline) {
      conn.textContent = 'Hardware Online (ESP32 + STM32)';
      conn.className = 'pill online';
    } else {
      conn.textContent = 'Waiting for Gateway...';
      conn.className = 'pill';
    }

    // Determine fault status
    const hasAlarm = d.fire || d.arc_fault || d.shock || d.ssr_tripped;
    if (d.fire) {
      banner.dataset.s = 'ALARM';
      conn.className = 'pill alarm';
      bS.textContent = 'CRITICAL ALARM — FIRE HAZARD DETECTED!';
      bSub.textContent = `Flame: ${d.flame === 0 ? 'ACTIVE (PB14)' : 'Clear'} • Smoke Analog: ${d.smoke_analog.toFixed(2)}V (PA1) • Temp: ${d.temp.toFixed(1)}°C (PA8). SSR Breaker tripped to cut load.`;
    } else if (d.arc_fault) {
      banner.dataset.s = 'ALARM';
      conn.className = 'pill alarm';
      bS.textContent = 'CRITICAL ALARM — ARC FAULT DETECTED (PA0)!';
      bSub.textContent =
        'High-frequency AC waveform distortion detected on ZMPT101B sensor. Open-Drain SSR isolated load.';
    } else if (d.shock) {
      banner.dataset.s = 'WARNING';
      conn.className = 'pill alarm';
      bS.textContent = 'ELECTRICAL ALARM — VOLTAGE / SHOCK ANOMALY!';
      bSub.textContent = `Mains Voltage anomaly (${d.v_rms.toFixed(1)} V). Exceeds safe operating limit (180V - 260V).`;
    } else if (d.ssr_tripped) {
      banner.dataset.s = 'WARNING';
      bS.textContent = 'CIRCUIT BREAKER IS TRIPPED';
      bSub.textContent =
        'Solid State Relay is open / isolated. Re-arm breaker once circuit conditions are confirmed safe.';
    } else {
      banner.dataset.s = 'NORMAL';
      bS.textContent = 'Normal — Hardware Online & Monitored';
      bSub.textContent = `Autonomous safety engine active on STM32 MCU. Grid Voltage: ${d.v_rms.toFixed(1)}V • System healthy.`;
    }

    // Audio alarm
    if (hasAlarm && !isAudioMuted) {
      startSiren();
    } else {
      stopSiren();
    }

    // Card 1: AC Line Voltage (PA0 via 220/100 ohm divider)
    $('#kV').textContent = `${d.v_rms.toFixed(1)} V`;
    const vPct = Math.min(100, Math.max(0, ((d.v_rms - 150) / 150) * 100));
    $('#bV').style.width = `${vPct}%`;
    $('#bV').style.background = d.v_rms < 180 || d.v_rms > 260 ? '#e50914' : '#ffffff';
    $('#cV').textContent =
      d.v_rms < 180
        ? 'Severe Voltage Sag (< 180V)'
        : d.v_rms > 260
          ? 'Overvoltage Surge (> 260V)'
          : 'Safe Operating Window (180V - 260V)';

    // Card 2: Smoke Level (PA1 & PB0)
    $('#kS').textContent = `${d.smoke_analog.toFixed(2)} V`;
    const sPct = Math.min(100, (d.smoke_analog / 5.0) * 100);
    $('#bSBar').style.width = `${sPct}%`;
    $('#bSBar').style.background = d.smoke_analog > 1.8 ? '#e50914' : '#cbd5e1';
    $('#cS').textContent =
      `Analog: ${d.smoke_analog.toFixed(2)}V (3.2x Scaled) • DO: ${d.smoke_dig} (${d.smoke_dig ? 'THRESHOLD EXCEEDED' : 'Normal'})`;

    // Card 3: Temperature (PA8)
    $('#kT').textContent = `${d.temp.toFixed(1)} °C`;
    const tPct = Math.min(100, (d.temp / 80.0) * 100);
    $('#bT').style.width = `${tPct}%`;
    $('#bT').style.background = d.temp > 55.0 ? '#e50914' : '#cbd5e1';
    $('#cT').textContent =
      `Safe Limit: 55.0 °C • Status: ${d.temp > 55.0 ? 'OVER TEMPERATURE' : 'Normal'}`;

    // Card 4: Flame IR (PB14)
    if (d.flame === 0) {
      $('#kF').textContent = 'FLAME DETECTED (0)';
      $('#kF').style.color = '#e50914';
      $('#bF').style.width = '100%';
      $('#bF').style.background = '#e50914';
      $('#cF').textContent = 'Optical IR radiation confirmed! Active LOW input on PB14.';
    } else {
      $('#kF').textContent = 'CLEAR (1)';
      $('#kF').style.color = '#10b981';
      $('#bF').style.width = '100%';
      $('#bF').style.background = '#10b981';
      $('#cF').textContent = 'Normal logic state (PB14 = HIGH) • No IR flame source.';
    }

    // SSR Breaker Status Card
    if (d.ssr_tripped) {
      $('#kSsr').textContent = 'TRIPPED / ISOLATED';
      $('#kSsr').style.color = '#e50914';
      $('#bSsr').style.width = '100%';
      $('#bSsr').style.background = '#e50914';
      $('#cSsr').textContent = 'Open-Drain sunk to GND. Mains circuit isolated.';
    } else {
      $('#kSsr').textContent = 'ENERGIZED / CLOSED';
      $('#kSsr').style.color = '#10b981';
      $('#bSsr').style.width = '100%';
      $('#bSsr').style.background = '#10b981';
      $('#cSsr').textContent = 'High-Z safe conduction. AC load energized.';
    }

    // Buzzer Status Card
    const buzOn = d.fire || d.arc_fault || d.shock;
    if (buzOn) {
      $('#kBuzzer').textContent = 'ACTIVE SIREN (1)';
      $('#kBuzzer').style.color = '#e50914';
      $('#bBuzzer').style.width = '100%';
      $('#bBuzzer').style.background = '#e50914';
      $('#cBuzzer').textContent = 'Buzzer pin PB12 driven HIGH by safety firmware.';
    } else {
      $('#kBuzzer').textContent = 'SILENT (0)';
      $('#kBuzzer').style.color = '#64748b';
      $('#bBuzzer').style.width = '0%';
      $('#cBuzzer').textContent = 'Buzzer pin PB12 quiescent.';
    }

    // Safety Verdict Summary Card
    if (d.fire) {
      $('#kSafety').textContent = 'FIRE TRIP';
      $('#kSafety').style.color = '#e50914';
      $('#bSafety').style.background = '#e50914';
      $('#cSafety').textContent = 'Fire hazard detected and latched.';
    } else if (d.arc_fault) {
      $('#kSafety').textContent = 'ARC TRIP';
      $('#kSafety').style.color = '#e50914';
      $('#bSafety').style.background = '#e50914';
      $('#cSafety').textContent = 'Arc fault signature detected on AC voltage.';
    } else if (d.shock) {
      $('#kSafety').textContent = 'VOLTAGE FAULT';
      $('#kSafety').style.color = '#e50914';
      $('#bSafety').style.background = '#e50914';
      $('#cSafety').textContent = 'Shock anomaly / severe voltage sag.';
    } else {
      $('#kSafety').textContent = 'HEALTHY';
      $('#kSafety').style.color = '#10b981';
      $('#bSafety').style.background = '#10b981';
      $('#cSafety').textContent = 'Fire: No • Arc: No • Shock: No • SSR: Closed';
    }

    // Oscilloscope Text
    $('#scopeVrms').textContent = `V_RMS: ${d.v_rms.toFixed(1)} V`;
    $('#scopeStatus').textContent = d.arc_fault
      ? 'Waveform: ARC SPUTTER DISTORTION'
      : d.shock
        ? d.v_rms < 180
          ? 'Waveform: SEVERE SAG'
          : 'Waveform: SURGE'
        : 'Waveform: SYNCHRONIZED SINE';

    // JSON Raw Document Inspector
    $('#jsonPreview').textContent = JSON.stringify(d, null, 2);
    if (d.updatedAt) {
      const sec = Math.round((Date.now() - d.updatedAt) / 1000);
      $('#lastUpdatedTag').textContent = `Last sync: ${sec < 2 ? 'just now' : sec + 's ago'}`;
    }
  }

  /* ---------- Real-time AC Oscilloscope Canvas ---------- */
  const canvas = $('#scopeCanvas');
  const ctx = canvas ? canvas.getContext('2d') : null;
  let phase = 0;

  function drawScope() {
    if (!ctx) return;
    const w = canvas.width;
    const h = canvas.height;
    ctx.clearRect(0, 0, w, h);

    // Grid Lines
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.05)';
    ctx.lineWidth = 1;
    for (let x = 0; x < w; x += 40) {
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, h);
      ctx.stroke();
    }
    for (let y = 0; y < h; y += 30) {
      ctx.beginPath();
      ctx.moveTo(0, y);
      ctx.lineTo(w, y);
      ctx.stroke();
    }

    // Center Baseline
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.15)';
    ctx.setLineDash([4, 4]);
    ctx.beginPath();
    ctx.moveTo(0, h / 2);
    ctx.lineTo(w, h / 2);
    ctx.stroke();
    ctx.setLineDash([]);

    // AC Waveform trace
    const midY = h / 2;
    const amp = ((currentData.v_rms || 230.0) / 230.0) * 80.0;
    phase += 0.08;

    ctx.lineWidth = 2.5;
    if (currentData.arc_fault) {
      ctx.strokeStyle = '#ff1e2d';
      ctx.shadowColor = 'rgba(255, 30, 45, 0.8)';
      ctx.shadowBlur = 12;
    } else if (currentData.shock) {
      ctx.strokeStyle = '#e50914';
      ctx.shadowColor = 'rgba(229, 9, 20, 0.8)';
      ctx.shadowBlur = 10;
    } else {
      ctx.strokeStyle = '#ffffff';
      ctx.shadowColor = 'rgba(255, 255, 255, 0.6)';
      ctx.shadowBlur = 8;
    }

    ctx.beginPath();
    for (let x = 0; x < w; x++) {
      const angle = (x / w) * Math.PI * 8 + phase;
      let y = midY - Math.sin(angle) * amp;

      if (currentData.arc_fault) {
        if (Math.abs(Math.sin(angle)) > 0.4) {
          y += (Math.random() - 0.5) * 26.0;
        }
      }

      if (x === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();
    ctx.shadowBlur = 0;

    requestAnimationFrame(drawScope);
  }

  /* ---------- Remote Command Dispatch to Hardware via Firestore ---------- */
  async function sendHardwareCommand(action, params = {}) {
    if (!db || !firestoreFns) {
      alert('Firestore not connected yet.');
      return;
    }
    try {
      const devId = $('#devSel').value || 'agninode-001';
      const colRef = firestoreFns.collection(db, 'devices', devId, 'commands');
      await firestoreFns.addDoc(colRef, {
        action,
        status: 'pending',
        createdAt: firestoreFns.serverTimestamp(),
        ...params,
      });
      console.log(`[Command Sent] ${action} dispatched to ${devId}`);
    } catch (e) {
      console.error('Error sending command:', e);
      alert('Could not dispatch command: ' + e.message);
    }
  }

  /* ---------- Firebase Subscription ---------- */
  async function initFirebase() {
    try {
      const B = 'https://www.gstatic.com/firebasejs/10.12.2/';
      const [{ initializeApp }, A, F] = await Promise.all([
        import(B + 'firebase-app.js'),
        import(B + 'firebase-auth.js'),
        import(B + 'firebase-firestore.js'),
      ]);

      const app = initializeApp(CFG);
      auth = A.getAuth(app);
      db = F.getFirestore(app);
      firestoreFns = F;

      A.onAuthStateChanged(auth, (u) => {
        if (!u) {
          // If not logged in, redirect to login page
          const demoRole = sessionStorage.getItem('demoRole');
          if (!demoRole) {
            location.replace('login.html');
            return;
          }
          $('#who').textContent = `Demo: ${demoRole}`;
        } else {
          authUser = u;
          $('#who').textContent = u.email || 'Operator';
        }
      });

      // Subscribe to live status
      const devId = 'agninode-001';
      const docRef = F.doc(db, 'devices', devId);
      const liveRef = F.doc(db, 'devices', devId, 'status', 'live');

      const handleSnap = (snap) => {
        if (!snap.exists()) return;
        const data = snap.data();

        // Parse exact hardware fields
        if (data.v_rms != null) currentData.v_rms = Number(data.v_rms);
        else if (data.voltageV != null) currentData.v_rms = Number(data.voltageV);

        if (data.smoke_analog != null) currentData.smoke_analog = Number(data.smoke_analog);
        else if (data.smokeAdc != null) currentData.smoke_analog = Number(data.smokeAdc) / 1000.0;

        if (data.smoke_dig != null) currentData.smoke_dig = Number(data.smoke_dig);

        if (data.temp != null) currentData.temp = Number(data.temp);
        else if (data.temperatureC != null) currentData.temp = Number(data.temperatureC);

        if (data.flame != null) currentData.flame = Number(data.flame);
        else if (data.flameDetected != null) currentData.flame = data.flameDetected ? 0 : 1;

        if (data.fire != null) currentData.fire = Boolean(data.fire);
        else if (data.fireConfirmed != null) currentData.fire = Boolean(data.fireConfirmed);

        if (data.arc_fault != null) currentData.arc_fault = Boolean(data.arc_fault);
        else if (data.afddTrip != null) currentData.arc_fault = Boolean(data.afddTrip);

        if (data.shock != null) currentData.shock = Boolean(data.shock);
        else if (data.rccbTrip != null) currentData.shock = Boolean(data.rccbTrip);

        if (data.ssr_tripped != null) currentData.ssr_tripped = Boolean(data.ssr_tripped);
        else if (data.mainsClosed != null) currentData.ssr_tripped = !Boolean(data.mainsClosed);

        if (data.gatewayOnline != null) currentData.gatewayOnline = Boolean(data.gatewayOnline);
        if (data.stm32Online != null) currentData.stm32Online = Boolean(data.stm32Online);

        currentData.updatedAt = Date.now();
        renderLive();
      };

      F.onSnapshot(docRef, handleSnap, (err) => console.warn('Root snap notice:', err));
      F.onSnapshot(liveRef, handleSnap, (err) => console.warn('Live snap notice:', err));
    } catch (e) {
      console.warn('Firebase initialization notice:', e);
      // Fallback display if offline
      renderLive();
    }
  }

  /* ---------- Event Handlers ---------- */
  function setupEvents() {
    $('#btnResetBreaker').onclick = () => {
      sendHardwareCommand('TRIP_RESET');
      currentData.fire = false;
      currentData.arc_fault = false;
      currentData.shock = false;
      currentData.ssr_tripped = false;
      renderLive();
    };

    $('#alarmToggle').onclick = () => {
      sendHardwareCommand('MAINS_OPEN');
      currentData.ssr_tripped = true;
      renderLive();
    };

    $('#btnAudioMute').onclick = () => {
      isAudioMuted = !isAudioMuted;
      $('#txtAudioMute').textContent = isAudioMuted ? 'Sound Muted' : 'Sound On';
      if (isAudioMuted) stopSiren();
      else renderLive();
    };

    $('#out').onclick = async () => {
      sessionStorage.removeItem('demoRole');
      if (auth) {
        try {
          const { signOut } =
            await import('https://www.gstatic.com/firebasejs/10.12.2/firebase-auth.js');
          await signOut(auth);
        } catch (e) {}
      }
      location.replace('login.html');
    };

    document.addEventListener('click', initAudio, { once: true });
  }

  // Startup
  setupEvents();
  renderLive();
  initFirebase();
  if (canvas) requestAnimationFrame(drawScope);
})();
