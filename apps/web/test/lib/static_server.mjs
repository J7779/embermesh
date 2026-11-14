// Dumb static file server for tests (any "dumb HTTP server" must be enough to run the site),
// plus optional hub emulation: a dynamic /hub.json and a minimal binary WebSocket at /ws.
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';

const TYPES = {
  '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.mjs': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8', '.json': 'application/json', '.webmanifest': 'application/manifest+json',
  '.svg': 'image/svg+xml', '.png': 'image/png', '.wasm': 'application/wasm', '.pack': 'application/octet-stream',
};

// Minimal RFC 6455 frame parser for client->server (masked) frames; returns [frames, rest].
function parseFrames(buf) {
  const out = [];
  let off = 0;
  for (;;) {
    if (buf.length - off < 2) break;
    const b0 = buf[off], b1 = buf[off + 1];
    let len = b1 & 0x7f, p = off + 2;
    if (len === 126) { if (buf.length - p < 2) break; len = buf.readUInt16BE(p); p += 2; }
    else if (len === 127) { if (buf.length - p < 8) break; len = Number(buf.readBigUInt64BE(p)); p += 8; }
    const masked = (b1 & 0x80) !== 0;
    const mask = masked ? buf.subarray(p, p + 4) : null;
    if (masked) p += 4;
    if (buf.length - p < len) break;
    const payload = Buffer.from(buf.subarray(p, p + len));
    if (mask) for (let i = 0; i < payload.length; i++) payload[i] ^= mask[i & 3];
    out.push({ fin: (b0 & 0x80) !== 0, op: b0 & 0x0f, payload });
    off = p + len;
  }
  return [out, buf.subarray(off)];
}

function frame(op, payload) {
  const n = payload.length;
  const head = n < 126 ? Buffer.from([0x80 | op, n]) : n < 65536 ? Buffer.from([0x80 | op, 126, n >> 8, n & 255]) : (() => { const h = Buffer.alloc(10); h[0] = 0x80 | op; h[1] = 127; h.writeBigUInt64BE(BigInt(n), 2); return h; })();
  return Buffer.concat([head, payload]);
}

/**
 * Static server. `hub` adds /hub.json + /ws; `relay:true` makes /ws a hub relay that forwards
 * every binary message to all other clients (what a hub does for its guests).
 */
export async function startServer(root, { hub = null, relay = false, throttle = null, base = '/', ranges = false, cutAfter = null } = {}) {
  // base: serve the site under a sub-path (GitHub Pages: /<repo>/); ranges: answer Range requests
  // with 206 (like GitHub Pages); cutAfter: {match, bytes} drop the connection after that many bytes
  const state = { root, requests: [], cutAfter };
  const wsClients = new Set();
  const received = [];
  const server = http.createServer((req, res) => {
    const u = new URL(req.url, 'http://x');
    let p = decodeURIComponent(u.pathname);
    state.requests.push(p + u.search + (req.headers.range ? ` [${req.headers.range}]` : ''));
    if (base !== '/') {
      if (p === base.replace(/\/$/, '')) { res.writeHead(301, { location: base }); res.end(); return; }
      if (!p.startsWith(base)) { res.writeHead(404, { 'content-type': 'text/plain' }); res.end('not found'); return; }
      p = '/' + p.slice(base.length);
    }
    root = state.root;
    if (hub && p === '/hub.json') {
      res.writeHead(200, { 'content-type': 'application/json', 'cache-control': 'no-store' });
      res.end(JSON.stringify({ embermesh_hub: true, name: hub.name || 'Test hub', ws: '/ws', version: '0.1.0' }));
      return;
    }
    if (p.endsWith('/')) p += 'index.html';
    const file = path.join(root, path.normalize(p).replace(/^(\.\.[/\\])+/, ''));
    if (!file.startsWith(root) || !fs.existsSync(file) || fs.statSync(file).isDirectory()) {
      res.writeHead(404, { 'content-type': 'text/plain' });
      res.end('not found');
      return;
    }
    const size = fs.statSync(file).size;
    const type = TYPES[path.extname(file)] || 'application/octet-stream';
    const range = ranges && /^bytes=(\d+)-(\d*)$/.exec(req.headers.range || '');
    if (range) {
      const a = Number(range[1]), b = Math.min(size - 1, range[2] ? Number(range[2]) : size - 1);
      if (a >= size || b < a) { res.writeHead(416, { 'content-range': `bytes */${size}` }); res.end(); return; }
      res.writeHead(206, { 'content-type': type, 'content-length': b - a + 1, 'content-range': `bytes ${a}-${b}/${size}`, 'accept-ranges': 'bytes' });
      const cut = state.cutAfter && state.cutAfter.match.test(p) ? state.cutAfter : null;
      if (cut) {
        if (cut.bytes <= 0) { res.destroy(); return; }
        const n = Math.min(cut.bytes, b - a + 1);
        cut.bytes -= n;
        const stream = fs.createReadStream(file, { start: a, end: a + n - 1 });
        stream.pipe(res, { end: false });
        stream.on('end', () => { if (n < b - a + 1) res.destroy(); else res.end(); });
        return;
      }
      fs.createReadStream(file, { start: a, end: b }).pipe(res);
      return;
    }
    res.writeHead(200, { 'content-type': type, 'content-length': size, ...(ranges ? { 'accept-ranges': 'bytes' } : {}) });
    const cut = state.cutAfter && state.cutAfter.match.test(p) ? state.cutAfter : null;
    if (cut) { // the network goes away in the middle of the download
      const n = Math.max(0, Math.min(cut.bytes, size));
      cut.bytes -= n;
      if (!n) { res.destroy(); return; }
      const stream = fs.createReadStream(file, { start: 0, end: n - 1 });
      stream.pipe(res, { end: false });
      stream.on('end', () => { if (n < size) res.destroy(); else res.end(); });
      return;
    }
    if (throttle && throttle.match.test(p)) {
      // a slow link (hub over Wi-Fi): 64 KiB chunks paced to bytesPerSec
      const fd = fs.openSync(file, 'r');
      const chunk = 65536;
      let off = 0;
      const step = () => {
        if (res.destroyed) { fs.closeSync(fd); return; }
        const n = Math.min(chunk, size - off);
        if (n <= 0) { fs.closeSync(fd); res.end(); return; }
        const b = Buffer.alloc(n);
        fs.readSync(fd, b, 0, n, off);
        off += n;
        res.write(b);
        setTimeout(step, (n / throttle.bytesPerSec) * 1000);
      };
      step();
      return;
    }
    fs.createReadStream(file).pipe(res);
  });
  server.on('upgrade', (req, socket) => {
    if (!hub || new URL(req.url, 'http://x').pathname !== '/ws') { socket.destroy(); return; }
    const key = req.headers['sec-websocket-key'];
    const accept = crypto.createHash('sha1').update(`${key}258EAFA5-E914-47DA-95CA-C5AB0DC85B11`).digest('base64');
    socket.write(`HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ${accept}\r\n\r\n`);
    wsClients.add(socket);
    let pending = Buffer.alloc(0);
    socket.on('data', (d) => {
      received.push(d.length);
      if (!relay) return;
      pending = Buffer.concat([pending, d]);
      const [frames, rest] = parseFrames(pending);
      pending = Buffer.from(rest);
      for (const f of frames) {
        if (f.op === 2 && f.fin) { for (const c of wsClients) if (c !== socket) c.write(frame(2, f.payload)); }
        else if (f.op === 9) socket.write(frame(10, f.payload));
        else if (f.op === 8) { socket.end(frame(8, Buffer.alloc(0))); }
      }
    });
    socket.on('close', () => wsClients.delete(socket));
    socket.on('error', () => {});
  });
  await new Promise((r) => server.listen(0, '127.0.0.1', r));
  const { port } = server.address();
  return {
    url: `http://127.0.0.1:${port}${base}`,
    state,
    setRoot(dir) { state.root = dir; },
    port,
    wsClients,
    received,
    close: () => new Promise((r) => { for (const s of wsClients) s.destroy(); server.closeAllConnections?.(); server.close(() => r()); }),
  };
}
