// Start-up scene: the logo as a small 3D world on a 2D canvas (perspective projection, depth-sorted
// draw list, additive glow sprites). World units = logo.svg units, origin at its centre (64,64),
// y down, z away from the viewer: the settled frame IS the logo. Importable without a DOM.
import { ease, clamp01 } from './timeline.js';

export const NODES = [[0, -50], [43, -25], [43, 25], [0, 50], [-43, 25], [-43, -25]];
export const SPOKES = [[0, -30], [24, -14], [24, 14], [0, 34], [-24, 14], [-24, -14]];
export const FLAME = 'M64 30 C73 44 88 55 88 75 C88 90 77 100 64 100 C51 100 40 90 40 75 C40 63 47 55 52 46 C54 55 58 60 63 62 C60 52 60 40 64 30 Z';
export const CORE = 'M64 62 C70 70 76 76 76 84 C76 92 70 97 64 97 C58 97 52 92 52 84 C52 78 56 74 59 70 C60 75 62 77 64 78 C63 72 62 67 64 62 Z';
export const DIST = 420; // camera distance
const WORD = 'EmberMesh', WS = 0.256, WY = 95, LIFT = 17, BASE = 32, EM = [0, 20, 0];
const FONT = '700 100px system-ui,-apple-system,"Segoe UI",Roboto,Helvetica,Arial,sans-serif';
// per node when scattered: extra radius, depth, sideways drift
const OFF = [[34, 78, 18], [54, -64, -22], [40, 48, 26], [60, -86, -14], [36, 66, 22], [50, -42, -28]];
// bg, link, node, flame×3, core×2, glow, hot, spark, ember (rgb), word×2, additive?, glow gain
const PAL = {
  dark: ['#17110f', 'rgba(255,138,61,.55)', '#ffb35c', '#e8471c', '#ff8a3d', '#ffc861', '#ffd27a', '#fff4d6', '255,138,61', '255,214,140', '255,244,214', '255,170,80', '#fff4e8', '#ff8a3d', 1, 1],
  light: ['#f4f2ee', 'rgba(196,84,22,.6)', '#e8702a', '#e8471c', '#ff8a3d', '#ffc861', '#ffd27a', '#fff4d6', '255,150,70', '255,170,90', '232,71,28', '232,90,30', '#2a1610', '#b34700', 0, 0.5],
  night: ['#000000', 'rgba(255,59,48,.5)', '#e02a20', '#5a0000', '#a01810', '#e02a20', '#ff3b30', '#ff4a3a', '200,0,0', '255,24,8', '255,40,20', '230,20,0', '#ff3b30', '#b3221a', 1, 0.6],
};

/** Rotation yaw (about y) → pitch (about x) → roll (about z). */
export function rotator(yaw, pitch, roll) {
  const cy = Math.cos(yaw), sy = Math.sin(yaw), cp = Math.cos(pitch), sp = Math.sin(pitch), cr = Math.cos(roll), sr = Math.sin(roll);
  return (x, y, z) => {
    const x1 = x * cy + z * sy, z1 = z * cy - x * sy, y1 = y * cp - z1 * sp;
    return [x1 * cr - y1 * sr, x1 * sr + y1 * cr, y * sp + z1 * cp];
  };
}

/** Perspective; cam = {cx, cy, F (focal px), dist}. → [x, y, px per unit, depth] */
export function project(p, cam) {
  const z = p[2] + cam.dist, k = cam.F / Math.max(z, 1e-3);
  return [cam.cx + p[0] * k, cam.cy + p[1] * k, k, z];
}

/** Node i scattered by sc (0 = in the hexagon): pushed out, swung round, moved in depth. */
export function nodePos(i, sc) {
  const [x, y] = NODES[i], o = OFF[i];
  const a = Math.atan2(y, x) + sc * 0.9, r = Math.hypot(x, y) + sc * o[0], t = o[2] * sc;
  return [Math.cos(a) * r - Math.sin(a) * t, Math.sin(a) * r + Math.cos(a) * t, o[1] * sc];
}

const hash = i => { const x = Math.sin(i * 127.1 + 311.7) * 43758.5453; return x - Math.floor(x); };
const lerp = (a, b, e) => a.map((v, i) => v + (b[i] - v) * e);

export function createRenderer(ctx, theme) {
  const P = PAL[theme] || PAL.dark, ADD = P[14] ? 'lighter' : 'source-over', gk = P[15];
  const flame = new Path2D(FLAME), core = new Path2D(CORE);
  const grad = (y0, y1, st) => { const g = ctx.createLinearGradient(0, y0, 0, y1); st.forEach((c, i) => g.addColorStop(st.length > 2 ? [0, 0.6, 1][i] : i, c)); return g; };
  const gF = grad(100, 30, [P[3], P[4], P[5]]), gC = grad(97, 62, [P[6], P[7]]);
  const sprite = c => {
    const s = document.createElement('canvas'), g = s.getContext('2d'), r = g.createRadialGradient(32, 32, 0, 32, 32, 32);
    s.width = s.height = 64;
    [[0, 1], [0.2, 0.6], [0.45, 0.22], [0.7, 0.06], [1, 0]].forEach(([o, a]) => r.addColorStop(o, `rgba(${c},${a})`));
    g.fillStyle = r; g.fillRect(0, 0, 64, 64);
    return s;
  };
  const [glow, hot, spk, emb] = [8, 9, 10, 11].map(i => sprite(P[i]));
  ctx.font = FONT;
  const total = ctx.measureText(WORD).width;
  const letters = [...WORD].map((c, j) => [c, ctx.measureText(WORD.slice(0, j)).width - total / 2]);

  /** v = {f (channels), W, H, dpr, S (px/unit), cx, cy, clock, ignite, out (null|0..1), q (0..2), target} */
  function draw(v) {
    const { f, dpr, S, q } = v, p = v.out;
    let iris = 0, ga = 1, cx = v.cx, cy = v.cy - LIFT * S * f.lift, Sx = S, spin = 0;
    if (p != null) {
      // hand-over: the logo turns once in depth as it flies onto the header's logo (or, with none
      // visible, folds into its own centre) while a hexagon opens onto the page behind it
      const mk = (v.target && v.target.mark) || { x: cx, y: cy, size: 0 }, e = ease.io(clamp01(p / 0.62));
      Sx = S * (Math.max(mk.size, 1) / 128 / S) ** e; cx += (mk.x - cx) * e; cy += (mk.y - cy) * e; spin = 6.2832 * e;
      iris = ease.i(clamp01((p - 0.04) / 0.5)); ga = 1 - ease.o(clamp01(p / 0.3));
    }
    const fa = p == null || (v.target && v.target.mark) ? 1 : 1 - clamp01(p / 0.6), la = fa;
    const cam = { cx, cy, F: Sx * DIST, dist: DIST / f.zoom }, R = rotator(f.yaw + spin, f.pitch, f.roll);
    const pr = (x, y, z) => project(R(x, y, z), cam);
    const scr = () => ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    const A = (a, op) => { ctx.globalAlpha = a; ctx.globalCompositeOperation = op || 'source-over'; };
    // planar element: affine approximation of the projection around (x,y,z), exact when flat
    const plane = (x, y, z, s) => {
      const c = pr(x, y, z), a = pr(x + 1, y, z), b = pr(x, y + 1, z), k = dpr * s;
      ctx.setTransform(k * (a[0] - c[0]), k * (a[1] - c[1]), k * (b[0] - c[0]), k * (b[1] - c[1]), dpr * c[0], dpr * c[1]);
    };
    const spr = (img, x, y, r, a) => { if (a > 0.003 && r > 0) { A(Math.min(1, a * gk), ADD); ctx.drawImage(img, x - r, y - r, 2 * r, 2 * r); A(1); } };
    const seg = (a, b) => { ctx.beginPath(); ctx.moveTo(a[0], a[1]); ctx.lineTo(b[0], b[1]); ctx.stroke(); };
    const poly = (pts, k) => { pts.forEach((h, i) => ctx[i ? 'lineTo' : 'moveTo'](h[0] * k, h[1] * k)); ctx.closePath(); };

    ctx.setTransform(1, 0, 0, 1, 0, 0); A(1);
    const cw = ctx.canvas.width, ch = ctx.canvas.height, nodes = NODES.map((_, i) => nodePos(i, f.scatter));
    ctx.clearRect(0, 0, cw, ch);
    ctx.fillStyle = P[0];
    ctx.beginPath(); ctx.rect(0, 0, cw, ch);
    let hole = null;
    if (iris > 0) {
      const big = Math.hypot(v.W, v.H) * 1.2 * iris / 50;
      hole = NODES.map(n => [v.cx + n[0] * big, v.cy + n[1] * big]);
      poly(hole, dpr);
    }
    ctx.fill('evenodd');
    scr();
    const fs = f.flame, sp = f.sep * (0.35 + 0.65 * fs);
    const E = pr(EM[0], EM[1], 24 * sp);
    spr(glow, E[0], E[1], E[2] * (70 + 50 * fs), f.glow * 0.42 * ga); // warm light behind it all

    const items = [], put = (z, o, fn) => items.push([z, o, fn]);
    // background sparks: a drifting volume that shows the camera's motion in depth
    if (f.dust > 0.01) {
      for (let i = 0; i < [42, 18, 0][q]; i++) {
        const y = ((hash(i + 99) * 640 - v.clock * (0.008 + 0.012 * hash(i + 7))) % 640 + 640) % 640 - 320;
        const D = pr((hash(i) - 0.5) * 560, y, (hash(i + 50) - 0.3) * 520);
        if (D[3] < 20) continue;
        const a = f.dust * (0.25 + 0.6 * hash(i + 3)) * Math.min(1, 300 / D[3]) * (p == null ? 1 : 1 - p);
        put(D[3], -2, () => spr(spk, D[0], D[1], D[2] * (3 + 4 * hash(i + 5)), a));
      }
    }
    const line = (a, b, e) => {
      if (e <= 0.001) return;
      const P0 = pr(...a), P1 = pr(...lerp(a, b, e));
      put((P0[3] + P1[3]) / 2, 0, () => { scr(); A(la); ctx.lineCap = 'round'; ctx.strokeStyle = P[1]; ctx.lineWidth = 1.25 * (P0[2] + P1[2]); seg(P0, P1); A(1); });
    };
    for (let i = 0; i < 6; i++) {
      line(nodes[i], nodes[(i + 1) % 6], clamp01(f.ring * 1.6 - i * 0.12));
      line(nodes[i], [...SPOKES[i], 0], f.spoke);
    }
    // nodes: sparks while scattered, a pulse as each sends its spark to the ember
    const run = f.spark > 0 && f.spark < 1;
    const halo = Math.max(clamp01(Math.abs(f.scatter)) * 0.9, run ? 0.7 * Math.sin(Math.PI * Math.min(1, f.spark * 2)) : 0);
    for (const n of nodes) {
      const N = pr(...n);
      put(N[3] - 30, 1, () => { scr(); spr(glow, N[0], N[1], N[2] * 24, halo * la); A(la); ctx.fillStyle = P[2]; ctx.beginPath(); ctx.arc(N[0], N[1], 5.5 * N[2], 0, 6.2832); ctx.fill(); A(1); });
    }
    // flame: three layers apart in depth (parallax) that close into the flat logo
    const fl = (z, s, path, fill, o, a) => put(pr(0, BASE, z)[3], o, () => { plane(0, BASE, z, s); ctx.translate(-64, -96); A(a); ctx.fillStyle = fill; ctx.fill(path); A(1); });
    if (fa > 0) {
      if (sp > 0.01) fl(30 * sp, fs * 1.25, flame, gF, -1, 0.32 * f.sep * ga);
      fl(8 * sp, fs, flame, gF, 2, fa);
      fl(-12 * sp, fs, core, gC, 3, fa);
      const C = pr(0, BASE - (BASE - EM[1]) * fs, -12 * sp);
      put(C[3] - 1, 4, () => { scr(); spr(hot, C[0], C[1], C[2] * 15 * (1 + fs), f.glow * 0.38 * ga); });
    }
    // sparks run from every node down its spoke into the ember
    if (run) {
      for (let i = 0; i < 6; i++) {
        const a = nodes[i], b = [...SPOKES[i], 0], l1 = Math.hypot(b[0] - a[0], b[1] - a[1], b[2] - a[2]), l2 = Math.hypot(b[0] - EM[0], b[1] - EM[1]);
        const at = s => { const d = clamp01(s) * (l1 + l2); return d < l1 ? lerp(a, b, d / l1) : lerp(b, EM, (d - l1) / l2); };
        const H = pr(...at(f.spark)), T = pr(...at(f.spark - 0.16));
        put(H[3] - 2, 5, () => {
          scr();
          const g = ctx.createLinearGradient(T[0], T[1], H[0], H[1]);
          g.addColorStop(0, `rgba(${P[10]},0)`); g.addColorStop(1, `rgba(${P[10]},.9)`);
          A(1, ADD); ctx.strokeStyle = g; ctx.lineCap = 'round'; ctx.lineWidth = 2.2 * H[2]; seg(T, H); A(1);
          spr(spk, H[0], H[1], H[2] * 11, 0.95);
        });
      }
    }
    // embers rising from the flame after it ignites (a pure function of time, so stills are exact)
    const te = v.clock - v.ignite;
    if (v.ignite > 0 && te > 0 && ga > 0) {
      for (let i = 0; i < [14, 8, 4][q]; i++) {
        const per = 1100 + 700 * hash(i + 20), dl = per * hash(i + 30) * 0.8;
        if (te < dl) continue;
        const a = ((te - dl) % per) / per;
        const D = pr((hash(i + 40) - 0.5) * 22 + Math.sin(a * 5 + i) * 7 * a, -6 - a * 78 * (0.7 + 0.5 * hash(i + 70)), (hash(i + 60) - 0.5) * 30);
        put(D[3], 5, () => spr(emb, D[0], D[1], D[2] * 2.4 * (2 - a), Math.sin(Math.PI * a) * (1 - a * 0.4) * 0.85 * ga));
      }
    }
    // wordmark: letters come forward out of depth one after another
    if (f.word > 0) {
      const wa = p == null ? 1 : 1 - clamp01(p / 0.25);
      letters.forEach(([c, lx], j) => {
        const e = ease.o(clamp01(f.word * 1.56 - j * 0.07)), x = lx * WS, z = -110 * (1 - e);
        if (e * wa > 0.003) put(pr(x, WY, z)[3], 6, () => { plane(x, WY + 4 * (1 - e), z, WS); A(e * wa); ctx.font = FONT; ctx.fillStyle = P[j < 5 ? 12 : 13]; ctx.fillText(c, 0, 0); A(1); });
      });
    }
    items.sort((a, b) => b[0] - a[0] || a[1] - b[1]);
    for (const it of items) it[2]();
    scr();
    if (hole && iris < 1) { A(1 - iris); ctx.strokeStyle = P[1]; ctx.lineJoin = 'round'; ctx.lineWidth = 3; ctx.beginPath(); poly(hole, 1); ctx.stroke(); A(1); }
  }
  return { draw };
}
