#!/usr/bin/env python3
"""Rysuje ikonę i ekrany startowe Music Impactor (Resources/*.png). Wymaga Pillow: pip install pillow."""
import math, os
from PIL import Image, ImageDraw, ImageFilter, ImageFont, ImageChops

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Resources")
FONT_DIR = "/usr/share/fonts/opentype/inter/"
NAME = "Music Impactor"
FOOT = "nazarOvsky 2024-2026"
ACCENT_T, ACCENT_B = (255, 92, 118), (160, 18, 62)       # czerwień-malina

def font(name, size):
    for p in (FONT_DIR + name, "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"):
        try:
            return ImageFont.truetype(p, size)
        except OSError:
            pass
    return ImageFont.load_default()

def vgrad(size, top, bot):
    w, h = size
    g = Image.new("RGB", (1, h))
    for y in range(h):
        t = y / max(1, h - 1)
        g.putpixel((0, y), tuple(int(top[i] + (bot[i] - top[i]) * t) for i in range(3)))
    return g.resize((w, h))

def rounded_mask(size, r):
    m = Image.new("L", size, 0)
    ImageDraw.Draw(m).rounded_rectangle([0, 0, size[0] - 1, size[1] - 1], r, fill=255)
    return m

def draw_note(layer, S):
    """podwójna nuta (ósemki połączone belką) + strzałka w dół do tacki"""
    d = ImageDraw.Draw(layer)
    w = (255, 255, 255, 255)
    # główki nut
    def head(cx, cy):
        r = Image.new("L", layer.size, 0)
        rd = ImageDraw.Draw(r)
        rd.ellipse([cx - S * 0.075, cy - S * 0.052, cx + S * 0.075, cy + S * 0.052], fill=255)
        r = r.rotate(18, center=(cx, cy), resample=Image.BICUBIC)
        layer.paste(w, (0, 0), r)
    lx, rx = S * 0.36, S * 0.64
    hy_l, hy_r = S * 0.50, S * 0.45
    head(lx, hy_l)
    head(rx, hy_r)
    sw = S * 0.030
    d.rectangle([lx + S * 0.052, S * 0.18, lx + S * 0.052 + sw, hy_l], fill=w)
    d.rectangle([rx + S * 0.052, S * 0.13, rx + S * 0.052 + sw, hy_r], fill=w)
    d.polygon([(lx + S * 0.052, S * 0.18), (rx + S * 0.052 + sw, S * 0.13), (rx + S * 0.052 + sw, S * 0.215), (lx + S * 0.052, S * 0.265)], fill=w)
    # tacka i strzałka
    ty = S * 0.70
    tw = S * 0.040
    d.rounded_rectangle([S * 0.26, ty, S * 0.26 + tw, S * 0.84], S * 0.012, fill=w)
    d.rounded_rectangle([S * 0.74 - tw, ty, S * 0.74, S * 0.84], S * 0.012, fill=w)
    d.rounded_rectangle([S * 0.26, S * 0.84 - tw, S * 0.74, S * 0.84], S * 0.012, fill=w)
    ax = S * 0.50
    d.rectangle([ax - S * 0.018, S * 0.60, ax + S * 0.018, S * 0.74], fill=w)
    d.polygon([(ax - S * 0.075, S * 0.72), (ax + S * 0.075, S * 0.72), (ax, S * 0.80)], fill=w)

def make_icon(S=1024):
    img = vgrad((S, S), ACCENT_T, ACCENT_B)
    # ciemniejszy dół i delikatne plamy światła
    shade = vgrad((S, S), (0, 0, 0), (0, 0, 0))
    vign = Image.new("L", (S, S), 0)
    ImageDraw.Draw(vign).ellipse([-S * 0.2, -S * 0.1, S * 1.2, S * 1.1], fill=255)
    vign = vign.filter(ImageFilter.GaussianBlur(S * 0.18))
    img = Image.composite(img, shade, vign.point(lambda v: int(150 + v * 105 / 255)))
    # cień nut
    layer = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    draw_note(layer, S)
    sh = Image.new("L", (S, S), 0)
    sh.paste(layer.split()[3].point(lambda v: int(v * 0.55)), (0, int(S * 0.012)))
    sh = sh.filter(ImageFilter.GaussianBlur(S * 0.014))
    img.paste((60, 0, 20), (0, 0), sh)
    img.paste(layer, (0, 0), layer.split()[3])
    # połysk (jak w ikonach iOS 6)
    gloss = Image.new("L", (S, S), 0)
    ImageDraw.Draw(gloss).ellipse([-S * 0.35, -S * 1.05, S * 1.35, S * 0.52], fill=255)
    gloss = gloss.filter(ImageFilter.GaussianBlur(S * 0.004))
    img.paste((255, 255, 255), (0, 0), gloss.point(lambda v: int(v * 0.24)))
    return img

def save_resized(img, size, name):
    img.resize((size, size), Image.LANCZOS).convert("RGB").save(os.path.join(OUT, name))

def make_splash(icon, W, H):
    cx, cy = W / 2, H * 0.40
    img = Image.new("RGB", (W, H), (14, 12, 14))
    d = ImageDraw.Draw(img)
    n = 24
    R = math.hypot(W, H)
    for i in range(n):
        a0 = 2 * math.pi * i / n
        a1 = 2 * math.pi * (i + 1) / n
        if i % 2 == 0:
            d.polygon([(cx, cy), (cx + R * math.cos(a0), cy + R * math.sin(a0)), (cx + R * math.cos(a1), cy + R * math.sin(a1))], fill=(74, 40, 48))
    vig = Image.new("L", (W, H), 0)
    ImageDraw.Draw(vig).ellipse([-W * 0.4, -H * 0.2, W * 1.4, H * 1.2], fill=255)
    vig = vig.filter(ImageFilter.GaussianBlur(W * 0.15))
    dark = Image.new("RGB", (W, H), (0, 0, 0))
    img = Image.composite(img, dark, vig.point(lambda v: int(120 + v * 135 / 255)))
    sz = int(W * 0.37)
    ic = icon.resize((sz, sz), Image.LANCZOS)
    m = rounded_mask((sz, sz), int(sz * 0.22))
    x0, y0 = int(cx - sz / 2), int(cy - sz / 2)
    refl = ic.transpose(Image.FLIP_TOP_BOTTOM)
    fade = vgrad((sz, sz), (110, 110, 110), (0, 0, 0)).convert("L")
    rm = ImageChops.multiply(m.transpose(Image.FLIP_TOP_BOTTOM), fade)
    refl_layer = Image.new("RGB", (sz, sz))
    refl_layer.paste(refl, (0, 0))
    img.paste(refl_layer.crop((0, 0, sz, int(sz * 0.45))), (x0, y0 + sz - int(sz * 0.12)), rm.crop((0, 0, sz, int(sz * 0.45))).point(lambda v: int(v * 0.5)))
    img.paste(ic, (x0, y0), m)
    d = ImageDraw.Draw(img)
    tf = font("Inter-BlackItalic.otf", int(W * 0.105))
    bb = d.textbbox((0, 0), NAME, font=tf)
    ty = int(cy + sz / 2 + H * 0.05)
    d.text((cx - (bb[2] - bb[0]) / 2 - bb[0] + 2, ty + 3 - bb[1]), NAME, font=tf, fill=(0, 0, 0))
    d.text((cx - (bb[2] - bb[0]) / 2 - bb[0], ty - bb[1]), NAME, font=tf, fill=(255, 255, 255))
    ff = font("Inter-Medium.otf", int(W * 0.033))
    track = int(W * 0.006)
    widths = [d.textlength(ch, font=ff) + track for ch in FOOT]
    total = sum(widths) - track
    x = cx - total / 2
    y = H - int(H * 0.045) - int(W * 0.033)
    for ch, w in zip(FOOT, widths):
        d.text((x, y), ch, font=ff, fill=(235, 235, 235))
        x += w
    return img

if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    icon = make_icon(1024)
    save_resized(icon, 114, "Icon@2x.png")
    save_resized(icon, 57, "Icon.png")
    save_resized(icon, 58, "Icon-Small@2x.png")
    save_resized(icon, 29, "Icon-Small.png")
    s = make_splash(icon, 640, 960)
    s.save(os.path.join(OUT, "Default@2x.png"))
    s.resize((320, 480), Image.LANCZOS).save(os.path.join(OUT, "Default.png"))
    make_splash(icon, 640, 1136).save(os.path.join(OUT, "Default-568h@2x.png"))
    icon.resize((512, 512), Image.LANCZOS).convert("RGB").save(os.path.join(OUT, "..", "icon-512.png"))
    print("gotowe")
