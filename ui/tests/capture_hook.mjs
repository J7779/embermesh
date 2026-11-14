// Shared by ui/tests/smoke_chrome.mjs and apps/web/test/e2e_morse.test.mjs (importing the smoke
// script itself would run it).
/**
 * Test hook for ui/js/lib/capture_media.js: a canvas.captureStream() "camera" showing a light keyed
 * in Morse, and a Web Audio oscillator keyed in Morse as the "microphone". Keying starts only when
 * the walk calls window.__emberCaptureHook.go() — after the reader reports live samples
 * (.morse-transcript[data-live]) — so a slow start never eats the first character.
 */
// What the test light/tone keys: a "VVV" tune-up first (as operators do), so the decoder's tone and
// speed tracking have settled before the message; the walk checks the message that follows.
export const MORSE_SENT = 'VVV CQ TEST';
export const MORSE_OK = /(^|\s)CQ TEST$/;
export const CAPTURE_HOOK = (text, wpm) => `(async () => {
  const { toMorse, morsePattern } = await import(new URL('js/lib/morse.js', location.href).href);
  let pat = morsePattern(toMorse(${JSON.stringify(text)}), ${wpm});
  const on = t => { let a = 0; for (let i = 0; i < pat.length; i++) { if (t < a + pat[i]) return i % 2 === 0; a += pat[i]; } return false; };
  let t0 = Infinity, mics = [], draws = [];
  window.__emberCaptureHook = {
    // go(wpm?): start keying (optionally at another speed); returns the keying time in ms
    go: (w) => { if (w) pat = morsePattern(toMorse(${JSON.stringify(text)}), w); t0 = performance.now() + 300; for (const m of mics) m(); mics = []; return pat.reduce((x, y) => x + y, 0) + 300; },
    // camera frames per second actually produced over the last 2 s (this page's paint rate)
    fps: () => { const now = performance.now(); draws = draws.filter(d => now - d < 2000); return draws.length / 2; },
    camera: () => {
      const c = document.createElement('canvas'); c.width = 320; c.height = 240;
      const g = c.getContext('2d');
      // The light's state is a function of the clock, so each frame shows what a real lamp would show
      // at that moment, however few frames a busy machine manages to paint.
      const draw = () => { const n = performance.now(); draws.push(n); if (draws.length > 400) draws.splice(0, 200); const t = n - t0; g.fillStyle = '#202020'; g.fillRect(0, 0, 320, 240); g.fillStyle = t > 0 && on(t) ? '#ffffff' : '#383838'; g.fillRect(220, 40, 50, 50); requestAnimationFrame(draw); };
      draw(); return c.captureStream(30);
    },
    mic: () => {
      const ac = new AudioContext(); const o = ac.createOscillator(); o.frequency.value = 650; const gn = ac.createGain(); gn.gain.value = 0;
      const dst = ac.createMediaStreamDestination(); o.connect(gn).connect(dst); o.start();
      mics.push(() => {
        const s0 = ac.currentTime + 0.3; let t = 0;
        for (let i = 0; i < pat.length; i++) { gn.gain.setValueAtTime(i % 2 === 0 ? 0.5 : 0, s0 + t / 1000); t += pat[i]; }
        gn.gain.setValueAtTime(0, s0 + t / 1000);
      });
      return dst.stream;
    },
  };
  return true;
})()`;
/**
 * Keying speed a camera can follow: at least 3 frames per dot (the decoder's documented need: 3–25
 * WPM for light, limited by the frame rate). A busy machine paints the test light at a few frames per
 * second; a real camera at 30 fps reads 12 WPM.
 */
export const cameraWpm = fps => Math.max(3, Math.min(12, Math.floor(1200 / (3.2 * 1000 / Math.max(1, fps)))));

