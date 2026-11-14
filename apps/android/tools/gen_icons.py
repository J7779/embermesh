#!/usr/bin/env python3
"""Generates the Android launch screen, launcher and notification icons from docs/assets/logo.svg.

    python3 apps/android/tools/gen_icons.py        (needs Pillow for the legacy PNG icons)

Outputs (all under app/src/main/res, committed):
  drawable/ic_logo.xml, drawable-v24/ic_logo.xml     the logo as a vector (solid flame before API 24,
                                                     gradients from 24 — gradients need API 24)
  drawable-v23/splash_window.xml                     pre-Android-12 window background: logo on #17110f
  drawable-v31/splash_icon.xml                       Android 12+ splash icon (logo inside the 288 dp icon box)
  drawable/ic_launcher_foreground.xml, ic_launcher_monochrome.xml   adaptive-icon layers (108 dp)
  drawable/ic_stat_ember.xml                         notification small icon (white silhouette)
  mipmap-{m,h,xh,xxh,xxxh}dpi/ic_launcher.png        legacy launcher icons (no metadata chunks)

Only the shapes this logo uses are supported: circle, and path with M/L/C/Z (absolute), fill or
stroke, linearGradient (vertical, objectBoundingBox). Sizes: the splash logo is SPLASH_FRACTION of
the shorter side of a typical 411 dp phone, which is how the web boot animation sizes its first frame.
"""
import os
import re
import sys
import xml.etree.ElementTree as ET

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
RES = os.path.join(HERE, "..", "app", "src", "main", "res")
SVG = os.path.join(ROOT, "docs", "assets", "logo.svg")
NS = {"s": "http://www.w3.org/2000/svg"}
BG = "#17110f"
SPLASH_FRACTION = 0.30          # logo width / shorter screen side (launch-screen contract)
TYPICAL_SHORT_SIDE_DP = 411     # typical phone; the Android 12+ splash icon has a fixed dp size
LEGACY = {"mdpi": 48, "hdpi": 72, "xhdpi": 96, "xxhdpi": 144, "xxxhdpi": 192}


def load():
    t = ET.parse(SVG).getroot()
    grads = {}
    for g in t.iter("{%s}linearGradient" % NS["s"]):
        stops = [(float(s.get("offset")), s.get("stop-color")) for s in g.findall("s:stop", NS)]
        grads[g.get("id")] = stops
    shapes = []  # dicts: kind, d|cx.., fill, stroke, stroke_width, opacity

    def walk(el, inherited):
        attrs = dict(inherited)
        for k in ("fill", "stroke", "stroke-width", "stroke-opacity", "stroke-linecap"):
            if el.get(k) is not None:
                attrs[k] = el.get(k)
        tag = el.tag.split("}")[1]
        if tag == "circle":
            shapes.append(dict(kind="circle", cx=float(el.get("cx")), cy=float(el.get("cy")), r=float(el.get("r")), **attrs))
        elif tag == "path":
            shapes.append(dict(kind="path", d=el.get("d"), **attrs))
        for c in el:
            if c.tag.split("}")[1] in ("defs", "title", "linearGradient"):
                continue
            walk(c, attrs)
    walk(t, {})
    return shapes, grads


def circle_path(cx, cy, r):
    return "M%g,%g a%g,%g 0 1,0 %g,0 a%g,%g 0 1,0 %g,0 Z" % (cx - r, cy, r, r, 2 * r, r, r, -2 * r)


def hexcol(c, alpha=1.0):
    c = c.lstrip("#")
    return "#%02X%s" % (round(alpha * 255), c.upper())


# ---- vector drawables ----------------------------------------------------------------------------

def vector_paths(shapes, grads, mode, ind="        "):
    """mode: 'color' (gradients), 'solid' (gradient -> its middle stop), 'mono' (white silhouette, no backdrop)."""
    out = []
    for s in shapes:
        d = s["d"] if s["kind"] == "path" else circle_path(s["cx"], s["cy"], s["r"])
        fill = s.get("fill")
        if s["kind"] == "circle" and fill and fill.lower() == BG and mode in ("mono", "fg"):
            continue  # the dark backdrop disc is the icon background layer, not foreground
        if fill and fill != "none":
            if fill.startswith("url("):
                gid = fill[5:-1]
                stops = grads[gid]
                if mode == "color":
                    items = "".join('%s            <item android:offset="%g" android:color="%s"/>\n' % (ind, o, hexcol(c)) for o, c in stops)
                    # objectBoundingBox x1=0 y1=1 x2=0 y2=0 -> bottom-to-top of the path's bounds
                    ys = [float(v) for v in re.findall(r"-?\d+\.?\d*", d)[1::2]]
                    out.append('%s<path android:pathData="%s">\n%s    <aapt:attr name="android:fillColor">\n%s        <gradient android:type="linear" android:startX="0" android:startY="%g" android:endX="0" android:endY="%g">\n%s%s        </gradient>\n%s    </aapt:attr>\n%s</path>\n'
                               % (ind, d, ind, ind, max(ys), min(ys), items, ind, ind, ind))
                    continue
                color = "#FFFFFF" if mode == "mono" else stops[len(stops) // 2][1]
                out.append('%s<path android:fillColor="%s" android:pathData="%s"/>\n' % (ind, hexcol(color), d))
            else:
                color = "#FFFFFF" if mode == "mono" else fill
                out.append('%s<path android:fillColor="%s" android:pathData="%s"/>\n' % (ind, hexcol(color), d))
        if s.get("stroke") and s.get("stroke") != "none":
            a = 1.0 if mode == "mono" else float(s.get("stroke-opacity", "1"))
            color = "#FFFFFF" if mode == "mono" else s["stroke"]
            out.append('%s<path android:strokeColor="%s" android:strokeWidth="%s" android:strokeLineCap="%s" android:strokeLineJoin="round" android:pathData="%s"/>\n'
                       % (ind, hexcol(color, a), s.get("stroke-width", "1"), s.get("stroke-linecap", "butt"), d))
    return "".join(out)


def vector(size_dp, viewport, body, scale=1.0, tx=0.0, ty=0.0, aapt=False, tint=None):
    ns = ' xmlns:aapt="http://schemas.android.com/aapt"' if aapt else ""
    t = ' android:tint="%s"' % tint if tint else ""
    return ('<?xml version="1.0" encoding="utf-8"?>\n<!-- Generated by apps/android/tools/gen_icons.py from docs/assets/logo.svg. Do not edit. -->\n'
            '<vector xmlns:android="http://schemas.android.com/apk/res/android"%s\n    android:width="%gdp" android:height="%gdp"\n'
            '    android:viewportWidth="%g" android:viewportHeight="%g"%s>\n    <group android:scaleX="%g" android:scaleY="%g" android:translateX="%g" android:translateY="%g">\n%s    </group>\n</vector>\n'
            % (ns, size_dp, size_dp, viewport, viewport, t, scale, scale, tx, ty, body))


def write(rel, text):
    p = os.path.join(RES, rel)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "w", encoding="utf-8") as f:
        f.write(text)
    print("wrote", os.path.relpath(p, ROOT))


# ---- legacy PNGs (supersampled rasteriser for the primitives above) -------------------------------

def flatten(d, steps=24):
    toks = re.findall(r"[MLCZmlcz]|-?\d*\.?\d+", d)
    i, cur, start, polys, pts = 0, (0.0, 0.0), (0.0, 0.0), [], []
    cmd = None
    while i < len(toks):
        if re.match(r"[MLCZmlcz]", toks[i]):
            cmd = toks[i]; i += 1
            if cmd in "Zz":
                if pts:
                    polys.append((pts, True)); pts = []
                cur = start
                continue
        nums = lambda n: [float(x) for x in toks[i:i + n]]
        if cmd == "M":
            if pts:
                polys.append((pts, False))
            x, y = nums(2); i += 2; cur = start = (x, y); pts = [cur]; cmd = "L"
        elif cmd == "L":
            x, y = nums(2); i += 2; cur = (x, y); pts.append(cur)
        elif cmd == "C":
            x1, y1, x2, y2, x, y = nums(6); i += 6
            x0, y0 = cur
            for k in range(1, steps + 1):
                t = k / steps; u = 1 - t
                pts.append((u**3 * x0 + 3 * u * u * t * x1 + 3 * u * t * t * x2 + t**3 * x,
                            u**3 * y0 + 3 * u * u * t * y1 + 3 * u * t * t * y2 + t**3 * y))
            cur = (x, y)
        else:
            raise SystemExit("unsupported path command %s" % cmd)
    if pts:
        polys.append((pts, False))
    return polys


def rgba(c, a=1.0):
    c = c.lstrip("#")
    return (int(c[0:2], 16), int(c[2:4], 16), int(c[4:6], 16), round(a * 255))


def render_png(shapes, grads, px, path):
    from PIL import Image, ImageDraw
    ss = 8
    S = px * ss
    k = S / 128.0
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    for s in shapes:
        layer = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        dr = ImageDraw.Draw(layer)
        fill = s.get("fill")
        if s["kind"] == "circle":
            cx, cy, r = s["cx"] * k, s["cy"] * k, s["r"] * k
            if fill and fill != "none":
                dr.ellipse([cx - r, cy - r, cx + r, cy + r], fill=rgba(fill))
            img = Image.alpha_composite(img, layer)
            continue
        polys = flatten(s["d"])
        if fill and fill != "none":
            mask = Image.new("L", (S, S), 0)
            md = ImageDraw.Draw(mask)
            for pts, _ in polys:
                md.polygon([(x * k, y * k) for x, y in pts], fill=255)
            if fill.startswith("url("):
                stops = grads[fill[5:-1]]
                ys = [y for pts, _ in polys for _, y in pts]
                y0, y1 = min(ys) * k, max(ys) * k
                grad = Image.new("RGBA", (S, S))
                gp = grad.load()
                for yy in range(S):
                    t = 0 if y1 == y0 else min(1, max(0, (y1 - yy) / (y1 - y0)))  # 0 at bottom, 1 at top
                    for j in range(len(stops) - 1):
                        if stops[j][0] <= t <= stops[j + 1][0] or j == len(stops) - 2:
                            a, b = stops[j], stops[j + 1]
                            f = 0 if b[0] == a[0] else min(1, max(0, (t - a[0]) / (b[0] - a[0])))
                            ca, cb = rgba(a[1]), rgba(b[1])
                            col = tuple(round(ca[c] + (cb[c] - ca[c]) * f) for c in range(4))
                            break
                    for xx in range(S):
                        gp[xx, yy] = col
                layer.paste(grad, (0, 0), mask)
            else:
                layer.paste(Image.new("RGBA", (S, S), rgba(fill)), (0, 0), mask)
        if s.get("stroke") and s["stroke"] != "none":
            w = float(s.get("stroke-width", "1")) * k
            col = rgba(s["stroke"], float(s.get("stroke-opacity", "1")))
            m = Image.new("L", (S, S), 0)
            md = ImageDraw.Draw(m)
            for pts, closed in polys:
                seq = [(x * k, y * k) for x, y in pts] + ([(pts[0][0] * k, pts[0][1] * k)] if closed else [])
                md.line(seq, fill=255, width=max(1, round(w)), joint="curve")
                for x, y in seq:  # round caps / joins
                    md.ellipse([x - w / 2, y - w / 2, x + w / 2, y + w / 2], fill=255)
            layer = Image.alpha_composite(layer, Image.composite(Image.new("RGBA", (S, S), col), Image.new("RGBA", (S, S), (0, 0, 0, 0)), m))
        img = Image.alpha_composite(img, layer)
    out = img.resize((px, px), Image.LANCZOS)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    out.save(path, format="PNG", optimize=True)  # Pillow writes no text/EXIF chunks unless asked
    print("wrote", os.path.relpath(path, ROOT), "(%dx%d)" % (px, px))


def main():
    shapes, grads = load()
    # The logo itself (128 viewport), 128 dp intrinsic size.
    write("drawable-v24/ic_logo.xml", vector(128, 128, vector_paths(shapes, grads, "color", "        "), aapt=True))
    write("drawable/ic_logo.xml", vector(128, 128, vector_paths(shapes, grads, "solid")))

    logo_dp = round(SPLASH_FRACTION * TYPICAL_SHORT_SIDE_DP)
    # Android 12+: the splash icon drawable is a 288-unit canvas drawn at 288 dp (an icon without a
    # background must keep its content inside the central 192 dp circle; the logo is far smaller).
    # Measured on the emulator once the launch animation has settled.
    box = 288.0
    on_canvas = float(logo_dp)
    sc = on_canvas / 128.0
    write("drawable-v31/splash_icon.xml", vector(box, box, vector_paths(shapes, grads, "color"), sc, (box - on_canvas) / 2, (box - on_canvas) / 2, aapt=True))
    # Before Android 12: window background = colour + centred logo (layer-list item size/gravity need API 23).
    write("drawable-v23/splash_window.xml",
          '<?xml version="1.0" encoding="utf-8"?>\n<!-- Generated by apps/android/tools/gen_icons.py. Do not edit. -->\n'
          '<layer-list xmlns:android="http://schemas.android.com/apk/res/android">\n'
          '    <item android:drawable="@color/ember_bg"/>\n'
          '    <item android:drawable="@drawable/ic_logo" android:width="%ddp" android:height="%ddp" android:gravity="center"/>\n'
          '</layer-list>\n' % (logo_dp, logo_dp))
    # Adaptive icon layers (108 dp; the visible safe zone is the central 66 dp circle). The hexagon
    # with its corner dots has a radius of ~55.5 logo units -> fit 2*55.5 units into 64 dp.
    s = 64.0 / 111.0
    t = 54 - 64 * s
    write("drawable/ic_launcher_foreground.xml", vector(108, 108, vector_paths(shapes, grads, "fg"), s, t, t))
    write("drawable/ic_launcher_monochrome.xml", vector(108, 108, vector_paths(shapes, grads, "mono"), s, t, t))
    # Notification small icon: white silhouette, 24 dp (the logo shrunk to fill it).
    s2 = 24.0 / 111.0
    t2 = 12 - 64 * s2
    write("drawable/ic_stat_ember.xml", vector(24, 24, vector_paths(shapes, grads, "mono"), s2, t2, t2))
    try:
        import PIL  # noqa: F401
    except ImportError:
        print("Pillow not installed: legacy PNG launcher icons not regenerated", file=sys.stderr)
        return
    for dens, px in LEGACY.items():
        render_png(shapes, grads, px, os.path.join(RES, "mipmap-%s" % dens, "ic_launcher.png"))


if __name__ == "__main__":
    main()
