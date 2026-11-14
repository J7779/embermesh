#!/usr/bin/env python3
"""Generates the iOS logo assets from docs/assets/logo.svg (no third-party tools).

  apps/ios/tools/make-logo-assets.py

Writes
  EmberMesh/Assets.xcassets/LaunchLogo.imageset/LaunchLogo.pdf   vector logo (launch screen + the
                                                                 native placeholder over the web view)
  EmberMesh/Assets.xcassets/AppIcon.appiconset/AppIcon-1024*.png light, dark and tinted app icons

The SVG's own shapes (circle, M/L/C/Z paths, two linear gradients, stroke opacity) are translated
into a hand-written PDF content stream, so the PDF carries no producer/creation-date metadata and
renders identically everywhere. Icons are rendered from icon PDFs with macOS `sips` and re-encoded
as plain RGB PNGs (IHDR + IDAT + IEND only: no text, time, ICC or EXIF chunks).
"""
import os
import re
import struct
import subprocess
import sys
import tempfile
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
IOS = os.path.dirname(HERE)
REPO = os.path.dirname(os.path.dirname(IOS))
SVG = os.path.join(REPO, "docs", "assets", "logo.svg")
ASSETS = os.path.join(IOS, "EmberMesh", "Assets.xcassets")
BG = "#17110f"


def rgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) / 255 for i in (0, 2, 4))


def gray(c):
    y = 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]
    return (y, y, y)


def fmt(v):
    s = ("%.4f" % v).rstrip("0").rstrip(".")
    return s if s not in ("", "-0") else "0"


def parse_svg(text):
    """Returns the drawing as a list of ops understood by content()."""
    grads = {}
    for m in re.finditer(r'<linearGradient id="(\w+)"[^>]*>(.*?)</linearGradient>', text, re.S):
        stops = [(float(o), c) for o, c in re.findall(r'offset="([\d.]+)" stop-color="(#[0-9a-fA-F]{6})"', m.group(2))]
        grads[m.group(1)] = stops
    ops = []
    bg = re.search(r'<circle cx="([\d.]+)" cy="([\d.]+)" r="([\d.]+)" fill="(#[0-9a-fA-F]{6})"/>', text)
    ops.append(("circle", [float(bg.group(1)), float(bg.group(2)), float(bg.group(3))], bg.group(4)))
    g = re.search(r'<g stroke="(#\w+)" stroke-opacity="([\d.]+)" stroke-width="([\d.]+)"[^>]*>(.*?)</g>', text, re.S)
    ops.append(("stroke", [p for p in re.findall(r'd="([^"]+)"', g.group(4))], g.group(1), float(g.group(2)), float(g.group(3))))
    nodes = re.search(r'<g fill="(#\w+)">(.*?)</g>', text, re.S)
    for cx, cy, r in re.findall(r'<circle cx="([\d.]+)" cy="([\d.]+)" r="([\d.]+)"/>', nodes.group(2)):
        ops.append(("circle", [float(cx), float(cy), float(r)], nodes.group(1)))
    for gid, d in re.findall(r'<path fill="url\(#(\w+)\)" d="([^"]+)"/>', text):
        ops.append(("gradient", d, grads[gid]))
    return ops


def path_ops(d):
    out = []
    toks = re.findall(r"[MLCZmlcz]|-?[\d.]+", d)
    i, cmd = 0, None
    pts = []
    while i < len(toks):
        t = toks[i]
        if t.isalpha():
            cmd = t.upper(); i += 1
            if cmd == "Z":
                out.append("h")
            continue
        n = {"M": 2, "L": 2, "C": 6}[cmd]
        v = [float(x) for x in toks[i:i + n]]; i += n
        pts += list(zip(v[0::2], v[1::2]))
        out.append(" ".join(fmt(x) for x in v) + {"M": " m", "L": " l", "C": " c"}[cmd])
        if cmd == "M":
            cmd = "L"
    return " ".join(out), pts


def circle_ops(cx, cy, r):
    k = 0.5522847498 * r
    return (f"{fmt(cx + r)} {fmt(cy)} m "
            f"{fmt(cx + r)} {fmt(cy + k)} {fmt(cx + k)} {fmt(cy + r)} {fmt(cx)} {fmt(cy + r)} c "
            f"{fmt(cx - k)} {fmt(cy + r)} {fmt(cx - r)} {fmt(cy + k)} {fmt(cx - r)} {fmt(cy)} c "
            f"{fmt(cx - r)} {fmt(cy - k)} {fmt(cx - k)} {fmt(cy - r)} {fmt(cx)} {fmt(cy - r)} c "
            f"{fmt(cx + k)} {fmt(cy - r)} {fmt(cx + r)} {fmt(cy - k)} {fmt(cx + r)} {fmt(cy)} c h")


def make_pdf(ops, size, square_bg=None, tint=False, scale_logo=1.0):
    """One-page PDF, `size` pt square. The 128-unit SVG box is mapped onto the page (y flipped)."""
    col = (lambda c: gray(rgb(c))) if tint else (lambda c: rgb(c))
    objs = {}
    shadings = []
    body = []
    if square_bg:
        c = col(square_bg)
        body.append(f"{fmt(c[0])} {fmt(c[1])} {fmt(c[2])} rg 0 0 {size} {size} re f")
    s = size / 128 * scale_logo
    off = (size - 128 * s) / 2
    body.append(f"q {fmt(s)} 0 0 {fmt(-s)} {fmt(off)} {fmt(size - off)} cm")
    for op in ops:
        if op[0] == "circle":
            c = col(op[2])
            body.append(f"{fmt(c[0])} {fmt(c[1])} {fmt(c[2])} rg {circle_ops(*op[1])} f")
        elif op[0] == "stroke":
            c = col(op[2])
            paths = " ".join(path_ops(d)[0] for d in op[1])
            body.append(f"q /GS1 gs {fmt(c[0])} {fmt(c[1])} {fmt(c[2])} RG {fmt(op[4])} w 1 J 1 j {paths} S Q")
            objs["gs_alpha"] = op[3]
        elif op[0] == "gradient":
            p, pts = path_ops(op[1])
            ys = [y for _, y in pts]
            y_bottom, y_top = max(ys), min(ys)   # gradient runs from the bbox bottom (offset 0) to its top
            name = f"Sh{len(shadings) + 1}"
            shadings.append((name, y_bottom, y_top, [(o, col(c)) for o, c in op[2]]))
            body.append(f"q {p} W n /{name} sh Q")
    body.append("Q")
    stream = "\n".join(body).encode()

    # Assemble objects.
    out = []
    def add(b):
        out.append(b if isinstance(b, bytes) else b.encode())
        return len(out)
    def fn(stops):
        if len(stops) == 2:
            return add(f"<< /FunctionType 2 /Domain [0 1] /C0 [{' '.join(fmt(x) for x in stops[0][1])}] "
                       f"/C1 [{' '.join(fmt(x) for x in stops[1][1])}] /N 1 >>")
        subs = [fn([stops[i], stops[i + 1]]) for i in range(len(stops) - 1)]
        bounds = " ".join(fmt(o) for o, _ in stops[1:-1])
        enc = " ".join("0 1" for _ in subs)
        return add(f"<< /FunctionType 3 /Domain [0 1] /Functions [{' '.join(f'{i} 0 R' for i in subs)}] "
                   f"/Bounds [{bounds}] /Encode [{enc}] >>")
    sh_refs = []
    for name, yb, yt, stops in shadings:
        f = fn(stops)
        sh_refs.append((name, add(f"<< /ShadingType 2 /ColorSpace /DeviceRGB /Coords [0 {fmt(yb)} 0 {fmt(yt)}] "
                                  f"/Function {f} 0 R /Extend [true true] >>")))
    gs = add(f"<< /Type /ExtGState /CA {fmt(objs.get('gs_alpha', 1))} /ca {fmt(objs.get('gs_alpha', 1))} >>")
    content = add(b"<< /Length " + str(len(stream)).encode() + b" >>\nstream\n" + stream + b"\nendstream")
    res = (f"<< /ExtGState << /GS1 {gs} 0 R >> /Shading << "
           + " ".join(f"/{n} {r} 0 R" for n, r in sh_refs) + " >> >>")
    page_idx = len(out) + 1
    pages_idx = page_idx + 1
    add(f"<< /Type /Page /Parent {pages_idx} 0 R /MediaBox [0 0 {size} {size}] /Resources {res} /Contents {content} 0 R >>")
    add(f"<< /Type /Pages /Kids [{page_idx} 0 R] /Count 1 >>")
    cat = add(f"<< /Type /Catalog /Pages {pages_idx} 0 R >>")
    pdf = bytearray(b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n")
    offsets = []
    for i, b in enumerate(out, 1):
        offsets.append(len(pdf))
        pdf += f"{i} 0 obj\n".encode() + b + b"\nendobj\n"
    xref = len(pdf)
    pdf += f"xref\n0 {len(out) + 1}\n0000000000 65535 f \n".encode()
    for o in offsets:
        pdf += f"{o:010d} 00000 n \n".encode()
    pdf += f"trailer\n<< /Size {len(out) + 1} /Root {cat} 0 R >>\nstartxref\n{xref}\n%%EOF\n".encode()
    return bytes(pdf)


def png_chunks(data):
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    i = 8
    while i < len(data):
        n = struct.unpack(">I", data[i:i + 4])[0]
        yield data[i + 4:i + 8], data[i + 8:i + 8 + n]
        i += 12 + n


def png_to_clean_rgb(src):
    """Decodes an 8-bit RGB/RGBA PNG and writes it back as RGB with only IHDR/IDAT/IEND."""
    data = open(src, "rb").read()
    ihdr = idat = None
    raw = b""
    for t, c in png_chunks(data):
        if t == b"IHDR":
            ihdr = c
        elif t == b"IDAT":
            raw += c
    w, h, depth, ctype = struct.unpack(">IIBB", ihdr[:10])
    assert depth == 8 and ctype in (2, 6), (depth, ctype)
    bpp = 4 if ctype == 6 else 3
    dec = zlib.decompress(raw)
    stride = w * bpp
    prev = bytearray(stride)
    rows = []
    p = 0
    for _ in range(h):
        f = dec[p]; line = bytearray(dec[p + 1:p + 1 + stride]); p += 1 + stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if f == 1: line[x] = (line[x] + a) & 255
            elif f == 2: line[x] = (line[x] + b) & 255
            elif f == 3: line[x] = (line[x] + ((a + b) >> 1)) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else (b if pb <= pc else c))) & 255
        rows.append(bytes(line)); prev = line
    out = bytearray()
    for r in rows:
        out.append(0)
        if bpp == 4:
            for x in range(w):  # flatten onto black (icons are opaque anyway)
                al = r[4 * x + 3]
                out += bytes((r[4 * x] * al // 255, r[4 * x + 1] * al // 255, r[4 * x + 2] * al // 255))
        else:
            out += r
    def chunk(t, c):
        return struct.pack(">I", len(c)) + t + c + struct.pack(">I", zlib.crc32(t + c) & 0xFFFFFFFF)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(out), 9)) + chunk(b"IEND", b""))


def render_icon(pdf_bytes, dest, px=1024):
    with tempfile.TemporaryDirectory() as td:
        pdf = os.path.join(td, "icon.pdf"); png = os.path.join(td, "icon.png")
        open(pdf, "wb").write(pdf_bytes)
        subprocess.run(["sips", "-s", "format", "png", "-z", str(px), str(px), pdf, "--out", png],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        open(dest, "wb").write(png_to_clean_rgb(png))


def main():
    ops = parse_svg(open(SVG, encoding="utf-8").read())
    launch_dir = os.path.join(ASSETS, "LaunchLogo.imageset")
    os.makedirs(launch_dir, exist_ok=True)
    open(os.path.join(launch_dir, "LaunchLogo.pdf"), "wb").write(make_pdf(ops, 128))
    open(os.path.join(launch_dir, "Contents.json"), "w").write("""{
  "images" : [ { "filename" : "LaunchLogo.pdf", "idiom" : "universal" } ],
  "info" : { "author" : "xcode", "version" : 1 },
  "properties" : { "preserves-vector-representation" : true, "template-rendering-intent" : "original" }
}
""")
    icon_dir = os.path.join(ASSETS, "AppIcon.appiconset")
    os.makedirs(icon_dir, exist_ok=True)
    # The node circles span 0.06–0.94 of the logo box; scale 0.92 keeps them inside the iOS
    # rounded-corner mask (nodes span ~72 % of the icon, centred).
    render_icon(make_pdf(ops, 1024, square_bg=BG, scale_logo=0.92), os.path.join(icon_dir, "AppIcon-1024.png"))
    # Dark and tinted variants sit on black (iOS darkens / tints them); the logo's own background
    # disc (the first op) would show as a grey circle there, so it is left out.
    art = ops[1:]
    render_icon(make_pdf(art, 1024, square_bg="#000000", scale_logo=0.92), os.path.join(icon_dir, "AppIcon-1024-dark.png"))
    render_icon(make_pdf(art, 1024, square_bg="#000000", tint=True, scale_logo=0.92), os.path.join(icon_dir, "AppIcon-1024-tinted.png"))
    open(os.path.join(icon_dir, "Contents.json"), "w").write("""{
  "images" : [
    { "filename" : "AppIcon-1024.png", "idiom" : "universal", "platform" : "ios", "size" : "1024x1024" },
    { "appearances" : [ { "appearance" : "luminosity", "value" : "dark" } ],
      "filename" : "AppIcon-1024-dark.png", "idiom" : "universal", "platform" : "ios", "size" : "1024x1024" },
    { "appearances" : [ { "appearance" : "luminosity", "value" : "tinted" } ],
      "filename" : "AppIcon-1024-tinted.png", "idiom" : "universal", "platform" : "ios", "size" : "1024x1024" }
  ],
  "info" : { "author" : "xcode", "version" : 1 }
}
""")
    print("wrote LaunchLogo.pdf and AppIcon-1024{,-dark,-tinted}.png")


if __name__ == "__main__":
    sys.exit(main())
