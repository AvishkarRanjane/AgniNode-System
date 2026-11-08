/* AgniNode Interactive Demo Dashboard Engine - 60 FPS Oscilloscope & Fault Simulator */
(() => {
  const $ = (s) => document.querySelector(s);

  let currentMode = 'NORMAL';
  let manualAlarm = false;
  let isAudioMuted = false;
  let isSirenPlaying = false;
  let audioCtx = null;
  let sirenOsc = null;
  let sirenGain = null;

  // Real-time telemetry state
  const state = {
    v_rms: 230.5,
    smoke_analog: 0.82,
    smoke_dig: 0,
    temp: 28.4,
    flame: 1, // 1 = clear, 0 = flame detected
    fire: false,
    arc_fault: false,
    shock: false,
    ssr_tripped: false,
    hf_noise: 1.8,
  };

  /* ---------- Audio Siren Generator ---------- */
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
      sirenGain.gain.setValueAtTime(0.12, audioCtx.currentTime);

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

  /* ---------- Mode State Update ---------- */
  function updatePhysics() {
    // Add micro jitter to readings
    const jitterV = (Math.random() - 0.5) * 0.8;
    const jitterS = (Math.random() - 0.5) * 0.02;
    const jitterT = (Math.random() - 0.5) * 0.05;

    if (currentMode === 'NORMAL') {
      state.v_rms = 230.0 + jitterV;
      state.smoke_analog = Math.max(0.5, 0.82 + jitterS);
      state.smoke_dig = 0;
      state.temp = 28.4 + jitterT;
      state.flame = 1;
      state.fire = false;
      state.arc_fault = false;
      state.shock = false;
      state.ssr_tripped = false;
      state.hf_noise = 1.6 + Math.random() * 0.4;
    } else if (currentMode === 'ARC') {
      state.v_rms = 224.0 + (Math.random() - 0.5) * 12.0;
      state.smoke_analog = 0.95 + jitterS;
      state.smoke_dig = 0;
      state.temp = 31.0 + jitterT;
      state.flame = 1;
      state.fire = false;
      state.arc_fault = true;
      state.shock = false;
      state.ssr_tripped = true;
      state.hf_noise = 24.5 + Math.random() * 18.0;
    } else if (currentMode === 'FIRE') {
      state.v_rms = 229.0 + jitterV;
      state.smoke_analog = 3.42 + jitterS;
      state.smoke_dig = 1;
      state.temp = 63.8 + jitterT;
      state.flame = 0; // Flame active!
      state.fire = true;
      state.arc_fault = false;
      state.shock = false;
      state.ssr_tripped = true;
      state.hf_noise = 2.1 + Math.random() * 0.5;
    } else if (currentMode === 'SHOCK') {
      state.v_rms = 162.0 + (Math.random() - 0.5) * 4.0;
      state.smoke_analog = 0.81 + jitterS;
      state.smoke_dig = 0;
      state.temp = 28.5 + jitterT;
      state.flame = 1;
      state.fire = false;
      state.arc_fault = false;
      state.shock = true;
      state.ssr_tripped = true;
      state.hf_noise = 3.2 + Math.random() * 1.0;
    } else if (currentMode === 'SURGE') {
      state.v_rms = 276.5 + (Math.random() - 0.5) * 3.0;
      state.smoke_analog = 0.85 + jitterS;
      state.smoke_dig = 0;
      state.temp = 29.0 + jitterT;
      state.flame = 1;
      state.fire = false;
      state.arc_fault = false;
      state.shock = true;
      state.ssr_tripped = true;
      state.hf_noise = 4.0 + Math.random() * 1.5;
    }

    if (manualAlarm) {
      state.ssr_tripped = true;
    }

    // Audio alarm management
    const hasAlarm =
      state.fire || state.arc_fault || state.shock || state.ssr_tripped || manualAlarm;
    if (hasAlarm && !isAudioMuted) {
      startSiren();
    } else {
      stopSiren();
    }

    renderUI();
  }

  /* ---------- UI Renderer ---------- */
  function renderUI() {
    const banner = $('#banner');
    const bS = $('#bS');
    const bSub = $('#bSub');

    if (state.fire) {
      banner.dataset.s = 'ALARM';
      bS.textContent = 'CRITICAL ALARM — FIRE HAZARD DETECTED!';
      bSub.textContent =
        'Flame detected on PB14 • Smoke analog 3.42V • Temp 63.8 °C. SSR cut off load immediately.';
    } else if (state.arc_fault) {
      banner.dataset.s = 'ALARM';
      bS.textContent = 'CRITICAL ALARM — ARC FAULT DETECTED (PA0)!';
      bSub.textContent =
        'Rapid high-frequency voltage waveform distortion on ZMPT101B. Load isolated via PB13 Open-Drain SSR.';
    } else if (state.shock) {
      banner.dataset.s = 'WARNING';
      bS.textContent = 'ELECTRICAL ALARM — VOLTAGE / SHOCK ANOMALY!';
      bSub.textContent = `AC Mains Voltage anomaly (${state.v_rms.toFixed(1)} V). Exceeds safe operating window (180V - 260V).`;
    } else if (manualAlarm) {
      banner.dataset.s = 'ALARM';
      bS.textContent = 'MANUAL TEST ALARM TRIGGERED';
      bSub.textContent = 'Operator simulated emergency panel trip. SSR tripped and siren active.';
    } else {
      banner.dataset.s = 'NORMAL';
      bS.textContent = 'Normal — All Systems Monitored & Safe';
      bSub.textContent =
        'STM32 64MHz decision engine active. Waveform sampling 12.8 kHz, 3.2x divider scaling applied.';
    }

    // Update KPI Card 1: AC Voltage
    $('#kV').textContent = `${state.v_rms.toFixed(1)} V`;
    const vPct = Math.min(100, Math.max(0, ((state.v_rms - 150) / 150) * 100));
    $('#bV').style.width = `${vPct}%`;
    $('#bV').style.background = state.v_rms < 180 || state.v_rms > 260 ? '#e50914' : '#ffffff';
    $('#cV').textContent =
      state.v_rms < 180
        ? 'Voltage Sag Fault (<180V)'
        : state.v_rms > 260
          ? 'Overvoltage Surge Fault (>260V)'
          : 'Nominal Grid RMS • Safe Range (180V - 260V)';

    // Update KPI Card 2: Smoke Sensor
    $('#kS').textContent = `${state.smoke_analog.toFixed(2)} V`;
    const sPct = Math.min(100, (state.smoke_analog / 5.0) * 100);
    $('#bSBar').style.width = `${sPct}%`;
    $('#bSBar').style.background = state.smoke_analog > 1.8 ? '#e50914' : '#cbd5e1';
    $('#cS').textContent =
      `Analog: ${state.smoke_analog.toFixed(2)}V • DO: ${state.smoke_dig} (${state.smoke_dig ? 'SMOKE ACTIVE' : 'Normal / Safe'})`;

    // Update KPI Card 3: Temperature Sensor
    $('#kT').textContent = `${state.temp.toFixed(1)} °C`;
    const tPct = Math.min(100, (state.temp / 80.0) * 100);
    $('#bT').style.width = `${tPct}%`;
    $('#bT').style.background = state.temp > 55.0 ? '#e50914' : '#cbd5e1';
    $('#cT').textContent =
      `Thermal Level • Threshold: > 55.0 °C (${state.temp > 55.0 ? 'EXCEEDED' : 'Normal'})`;

    // Update KPI Card 4: Flame Optical Sensor
    if (state.flame === 0) {
      $('#kF').textContent = 'FLAME DETECTED (0)';
      $('#kF').style.color = '#e50914';
      $('#bF').style.background = '#e50914';
      $('#cF').textContent = 'Optical IR radiation confirmed! Immediate hazard.';
    } else {
      $('#kF').textContent = 'CLEAR (1)';
      $('#kF').style.color = '#10b981';
      $('#bF').style.background = '#10b981';
      $('#cF').textContent = 'Active LOW logic • No flame radiation detected';
    }

    // Update SSR Breaker Status
    if (state.ssr_tripped) {
      $('#kSsr').textContent = 'TRIPPED / ISOLATED';
      $('#kSsr').style.color = '#e50914';
      $('#bSsr').style.background = '#e50914';
      $('#cSsr').textContent = 'Open-Drain sunk to GND. AC load isolated for physical safety.';
    } else {
      $('#kSsr').textContent = 'ENERGIZED / CLOSED';
      $('#kSsr').style.color = '#10b981';
      $('#bSsr').style.background = '#10b981';
      $('#cSsr').textContent = 'Mains load active. In Open-Drain High-Z safe conduction.';
    }

    // Update Buzzer Status
    const buzOn = state.fire || state.arc_fault || state.shock || manualAlarm;
    if (buzOn) {
      $('#kBuzzer').textContent = 'SOUNDING (1)';
      $('#kBuzzer').style.color = '#e50914';
      $('#bBuzzer').style.width = '100%';
      $('#bBuzzer').style.background = '#e50914';
      $('#cBuzzer').textContent = 'Active high audible siren output engaged on PB12.';
    } else {
      $('#kBuzzer').textContent = 'SILENT (0)';
      $('#kBuzzer').style.color = '#64748b';
      $('#bBuzzer').style.width = '0%';
      $('#cBuzzer').textContent = 'No active emergency audible alert.';
    }

    // Update Autonomous Safety Verdict
    if (state.fire || state.arc_fault || state.shock) {
      $('#kSafety').textContent = 'PROTECTION TRIPPED';
      $('#kSafety').style.color = '#e50914';
      $('#bSafety').style.background = '#e50914';
      $('#cSafety').textContent = 'Hazard interlock engaged autonomously by STM32 firmware.';
    } else {
      $('#kSafety').textContent = 'ALL CLEAR';
      $('#kSafety').style.color = '#10b981';
      $('#bSafety').style.background = '#10b981';
      $('#cSafety').textContent = 'Zero faults detected. Instant autonomous response armed.';
    }

    // Update Oscilloscope Overlay Text
    $('#scopeVrms').textContent = `V_RMS: ${state.v_rms.toFixed(1)} V`;
    $('#scopeHf').textContent = `HF Noise: ${state.hf_noise.toFixed(1)} counts`;
    $('#scopeStatus').textContent = state.arc_fault
      ? 'Waveform: ARC SPUTTER DISTORTION'
      : state.shock
        ? state.v_rms < 180
          ? 'Waveform: SEVERE SAG'
          : 'Waveform: OVERVOLTAGE SURGE'
        : 'Waveform: PURE SINE';

    // Update JSON Preview
    const telemetryObj = {
      v_rms: Number(state.v_rms.toFixed(1)),
      smoke_analog: Number(state.smoke_analog.toFixed(2)),
      smoke_dig: state.smoke_dig,
      temp: Number(state.temp.toFixed(1)),
      flame: state.flame,
      fire: state.fire,
      arc_fault: state.arc_fault,
      shock: state.shock,
      ssr_tripped: state.ssr_tripped,
    };
    $('#jsonPreview').textContent = JSON.stringify(telemetryObj, null, 2);
  }

  /* ---------- 60 FPS Real-time AC Waveform Canvas Oscilloscope ---------- */
  const canvas = $('#scopeCanvas');
  const ctx = canvas.getContext('2d');
  let phase = 0;

  function drawScope() {
    const w = canvas.width;
    const h = canvas.height;
    ctx.clearRect(0, 0, w, h);

    // Draw Grid Lines
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

    // Zero Volt Center Reference
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.15)';
    ctx.setLineDash([4, 4]);
    ctx.beginPath();
    ctx.moveTo(0, h / 2);
    ctx.lineTo(w, h / 2);
    ctx.stroke();
    ctx.setLineDash([]);

    // Draw Real-time AC Sine Wave
    const midY = h / 2;
    const ampScale = (state.v_rms / 230.0) * 80.0;
    phase += 0.08;

    ctx.lineWidth = 2.5;
    if (state.arc_fault) {
      ctx.strokeStyle = '#ff1e2d';
      ctx.shadowColor = 'rgba(255, 30, 45, 0.8)';
      ctx.shadowBlur = 12;
    } else if (state.shock) {
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
      let y = midY - Math.sin(angle) * ampScale;

      // Inject arc sputtering noise if arc fault is active
      if (state.arc_fault) {
        if (Math.abs(Math.sin(angle)) > 0.4) {
          y += (Math.random() - 0.5) * 28.0;
        }
      }

      if (x === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();
    ctx.shadowBlur = 0;

    requestAnimationFrame(drawScope);
  }

  /* ---------- Event Handlers ---------- */
  function setupEvents() {
    const buttons = document.querySelectorAll('#simActions .sim-btn:not(.sim-reset)');
    buttons.forEach((btn) => {
      btn.onclick = () => {
        buttons.forEach((b) => b.classList.remove('active'));
        btn.classList.add('active');
        currentMode = btn.dataset.mode;
        manualAlarm = false;
        updatePhysics();
      };
    });

    $('#btnSimReset').onclick = () => {
      currentMode = 'NORMAL';
      manualAlarm = false;
      buttons.forEach((b) => b.classList.remove('active'));
      $('#btnSimNormal').classList.add('active');
      updatePhysics();
    };

    $('#alarmToggle').onclick = () => {
      manualAlarm = !manualAlarm;
      updatePhysics();
    };

    $('#btnAudioMute').onclick = () => {
      isAudioMuted = !isAudioMuted;
      $('#txtAudioMute').textContent = isAudioMuted ? 'Sound Muted' : 'Sound On';
      if (isAudioMuted) stopSiren();
      else updatePhysics();
    };

    document.addEventListener('click', initAudio, { once: true });
  }

  // Initialize
  setupEvents();
  updatePhysics();
  setInterval(updatePhysics, 400); // 400ms periodic physics refresh
  requestAnimationFrame(drawScope);
})();
