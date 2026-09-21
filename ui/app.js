(() => {
  const FLOORS = { cpu: 100, mem: 100, temp: 80, fan: 4000 };
  const UNITS = { cpu: "%", mem: "%", temp: "°", fan: " rpm" };
  const DIGITS = { cpu: 0, mem: 0, temp: 0, fan: 0 };

  const tabs = {
    sys: document.getElementById("tab-sys"),
    ai: document.getElementById("tab-ai"),
  };
  const pages = {
    sys: document.getElementById("page-sys"),
    ai: document.getElementById("page-ai"),
  };

  function show(name) {
    for (const key of Object.keys(pages)) {
      const on = key === name;
      tabs[key].classList.toggle("is-on", on);
      tabs[key].setAttribute("aria-selected", on ? "true" : "false");
      pages[key].classList.toggle("is-on", on);
      pages[key].hidden = !on;
    }
  }

  function showFromLocation() {
    const page = new URLSearchParams(location.search).get("p") || location.hash.replace("#", "");
    if (page === "ai" || page === "usage") show("ai");
    else show("sys");
  }

  tabs.sys.addEventListener("click", () => show("sys"));
  tabs.ai.addEventListener("click", () => show("ai"));
  window.addEventListener("keydown", (event) => {
    if (event.key === "1") show("sys");
    if (event.key === "2") show("ai");
  });
  showFromLocation();

  function formatNumber(metric, value) {
    if (value == null || Number.isNaN(value)) return "—";
    const digits = DIGITS[metric];
    const n = Number(value);
    if (metric === "fan") return `${Math.round(n)}${UNITS[metric]}`;
    if (metric === "temp") return `${Math.round(n)}${UNITS[metric]}`;
    return `${n.toFixed(digits)}${UNITS[metric]}`;
  }

  function formatTokens(n) {
    const v = Number(n) || 0;
    if (v >= 1e9) return `${(v / 1e9).toFixed(1)}B`;
    if (v >= 1e6) return `${(v / 1e6).toFixed(1)}M`;
    if (v >= 1e3) return `${(v / 1e3).toFixed(1)}K`;
    return String(Math.round(v));
  }

  function fitCanvas(canvas) {
    const rect = canvas.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    const w = Math.max(1, Math.round(rect.width * dpr));
    const h = Math.max(1, Math.round(rect.height * dpr));
    if (canvas.width !== w || canvas.height !== h) {
      canvas.width = w;
      canvas.height = h;
    }
    return { w, h, dpr };
  }

  function drawScope(canvas, samples, floor) {
    const ctx = canvas.getContext("2d");
    const { w, h, dpr } = fitCanvas(canvas);
    ctx.clearRect(0, 0, w, h);
    if (!samples.length) return;

    const ink = getComputedStyle(document.documentElement).getPropertyValue("--trace").trim() || "#1a1a1a";
    const rule = getComputedStyle(document.documentElement).getPropertyValue("--rule").trim() || "rgba(26,26,26,0.12)";
    const max = Math.max(floor, ...samples);
    const min = 0;
    const span = Math.max(1e-6, max - min);
    const padY = 4 * dpr;

    ctx.beginPath();
    ctx.strokeStyle = rule;
    ctx.lineWidth = dpr;
    const mid = padY + (h - padY * 2) * 0.5;
    ctx.moveTo(0, mid);
    ctx.lineTo(w, mid);
    ctx.stroke();

    ctx.beginPath();
    ctx.strokeStyle = ink;
    ctx.lineWidth = 1.35 * dpr;
    ctx.lineJoin = "round";
    ctx.lineCap = "round";
    const last = samples.length - 1;
    samples.forEach((value, i) => {
      const x = last <= 0 ? w : (i / last) * w;
      const y = padY + (1 - (value - min) / span) * (h - padY * 2);
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    });
    ctx.stroke();
  }

  function renderSys(state) {
    const history = state.history || {};
    const sys = state.sys || {};
    for (const metric of ["cpu", "mem", "temp", "fan"]) {
      const el = document.getElementById(`val-${metric}`);
      el.textContent = formatNumber(metric, sys[metric]);
      drawScope(
        document.getElementById(`scope-${metric}`),
        history[metric] || [],
        FLOORS[metric]
      );
    }
  }

  function limitLabel(row) {
    const pct = Math.round((Number(row.percent) || 0) * 100);
    return `${row.label} ${pct}%`;
  }

  function renderAi(state) {
    const root = document.getElementById("ai-list");
    const providers = (state.ai && state.ai.providers) || [];
    if (!providers.length) {
      root.innerHTML = `<p class="quiet">No usage records yet.</p>`;
      return;
    }
    root.innerHTML = providers
      .map((p) => {
        const primary = (p.limits && p.limits[0]) || null;
        const bar = primary
          ? Math.max(0, Math.min(100, (Number(primary.percent) || 0) * 100))
          : 0;
        const usd =
          p.todayUsd != null
            ? `$${Number(p.todayUsd).toFixed(2)} today`
            : `${p.todayPrompts || 0} prompts`;
        const week = (p.recent || []).map((d) => d.value);
        const id = `week-${p.id}`;
        return `
          <article class="ai">
            <div class="ai-top">
              <span class="ai-name">${p.label}</span>
              <span class="ai-tier">${p.tier || (p.ready ? "" : "offline")}</span>
            </div>
            <div class="ai-main">
              <span class="ai-tokens">${formatTokens(p.todayTokens)}</span>
              <span class="ai-usd">${usd}</span>
            </div>
            <canvas class="ai-week" id="${id}" width="720" height="72"></canvas>
            <div class="bar"><span style="width:${bar}%"></span></div>
            <div class="ai-meta">
              <span>${primary ? limitLabel(primary) : "Local sessions"}</span>
              <span>${p.todaySessions || 0} sessions</span>
            </div>
          </article>
        `;
      })
      .join("");
    providers.forEach((p) => {
      const canvas = document.getElementById(`week-${p.id}`);
      if (!canvas) return;
      const values = (p.recent || []).map((d) => Number(d.value) || 0);
      const floor = Math.max(1, ...values, 1);
      drawScope(canvas, values, floor);
    });
  }

  let lastAiStamp = "";

  function apply(state) {
    renderSys(state);
    const stamp = JSON.stringify(state.ai || {});
    if (stamp !== lastAiStamp) {
      lastAiStamp = stamp;
      renderAi(state);
    }
  }

  function connect() {
    const source = new EventSource("/events");
    source.onmessage = (event) => {
      try {
        apply(JSON.parse(event.data));
      } catch (err) {
        console.warn(err);
      }
    };
    source.onerror = () => {
      source.close();
      setTimeout(connect, 1500);
    };
  }

  fetch("/api/state")
    .then((r) => r.json())
    .then(apply)
    .catch(() => {})
    .finally(connect);

  window.addEventListener("resize", () => {
    fetch("/api/state")
      .then((r) => r.json())
      .then(apply)
      .catch(() => {});
  });
})();
