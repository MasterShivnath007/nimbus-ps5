#!/usr/bin/env python3
"""Compose Nimbus' 4K PS5 shell backgrounds (pic0 / pic1).

The shell renders the app name as text over the left side, so the artwork keeps
that area dark and puts the planet on the right.
"""
from pathlib import Path

import math
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
OUT = ROOT / "assets" / "shell"
SHELL = ROOT / "sce_sys"

W, H = 3840, 2160


def stars(rng, count, dim):
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer)
    for _ in range(count):
        x = int(rng.random() * W)
        y = int(rng.random() * H * 0.78)
        b = int(40 + rng.random() ** 2.6 * 215)
        r = 1 if rng.random() < 0.88 else 2
        draw.ellipse((x - r, y - r, x + r, y + r), fill=(b, b, min(255, b + 12), dim))
    return layer.filter(ImageFilter.GaussianBlur(0.35))


def compose(variant: int) -> Image.Image:
    rng = np.random.default_rng(7 + variant)
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    x = xx / W
    y = yy / H
    top = np.array([0.010, 0.019, 0.030], dtype=np.float32)
    bottom = np.array([0.022, 0.060, 0.072], dtype=np.float32)
    base = top[None, None, :] + (bottom - top)[None, None, :] * (y ** 1.35)[..., None]
    aurora = np.exp(-(((x - 0.12) ** 2 / 0.12) + ((y - 0.86) ** 2 / 0.20)))
    base += aurora[..., None] * np.array([0.015, 0.090, 0.076], dtype=np.float32)
    image = Image.fromarray((np.clip(base, 0, 1) * 255).astype(np.uint8), "RGB").convert("RGBA")
    image = Image.alpha_composite(image, stars(rng, 520, 190))

    # Planet limb on the right: a crop of the day map, shaded like a sphere.
    size = 2800 if variant == 0 else 3100
    day = Image.open(ASSETS / "earth_day.jpg").resize((2048, 1024), Image.LANCZOS)
    source = day.crop((1280, 160, 1730, 610)).resize((size, size), Image.LANCZOS)
    globe = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, size - 1, size - 1), fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(2.0))
    globe.paste(source, (0, 0), mask)

    gx, gy = np.mgrid[0:size, 0:size].astype(np.float32) / size
    light = np.clip(1.35 - 2.1 * np.sqrt((gx - 0.72) ** 2 + (gy - 0.24) ** 2), 0.08, 1.0)
    arr = np.asarray(globe, dtype=np.float32)
    arr[..., 0] *= light * 0.92
    arr[..., 1] *= light * 0.98
    arr[..., 2] *= light * 1.06
    globe = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "RGBA")

    cx = 3550 if variant == 0 else 3710
    cy = 1210 if variant == 0 else 980
    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    draw = ImageDraw.Draw(glow)
    radius = size // 2 + 26
    draw.ellipse((cx - radius, cy - radius, cx + radius, cy + radius),
                 outline=(70, 193, 255, 150), width=9)
    draw.ellipse((cx - radius - 20, cy - radius - 20, cx + radius + 20, cy + radius + 20),
                 outline=(70, 193, 255, 55), width=4)
    glow = glow.filter(ImageFilter.GaussianBlur(28))
    image = Image.alpha_composite(image, glow)
    image.alpha_composite(globe, (cx - size // 2, cy - size // 2))

    # Orbital arcs and a satellite glint.
    orbit = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    draw = ImageDraw.Draw(orbit)
    box = (cx - 1850, cy - 1300, cx + 1850, cy + 1300)
    draw.arc(box, start=186, end=342, fill=(224, 248, 255, 120), width=5)
    draw.arc((box[0] + 120, box[1] + 80, box[2] - 120, box[3] - 80), start=190, end=338,
             fill=(224, 248, 255, 40), width=2)
    dot_x = cx + int(1800 * math.cos(math.radians(342)))
    dot_y = cy + int(1300 * math.sin(math.radians(342)))
    draw.ellipse((dot_x - 17, dot_y - 17, dot_x + 17, dot_y + 17), fill=(92, 237, 212, 255))
    image = Image.alpha_composite(image, orbit.filter(ImageFilter.GaussianBlur(18)))
    image = Image.alpha_composite(image, orbit)

    # A soft vignette keeps the title area calm.
    vignette = Image.new("L", (W, H), 0)
    ImageDraw.Draw(vignette).ellipse((-500, -500, W + 500, H + 500), fill=255)
    vignette = vignette.filter(ImageFilter.GaussianBlur(280))
    black = Image.new("RGBA", (W, H), (0, 0, 0, 70))
    image = Image.composite(image, Image.alpha_composite(image, black), vignette)
    return image.convert("RGB")


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    (ROOT / "docs" / "screenshots").mkdir(parents=True, exist_ok=True)
    for i in range(2):
        image = compose(i)
        png = OUT / f"pic{i}.png"
        (SHELL / f"pic{i}.png").unlink(missing_ok=True)
        image.save(png, "PNG", optimize=True)
        image.resize((1920, 1080), Image.LANCZOS).save(
            ROOT / "docs" / "screenshots" / f"shell-pic{i}.jpg", "JPEG", quality=90)
        print(f"wrote {png} ({png.stat().st_size/1024/1024:.1f} MiB)")


if __name__ == "__main__":
    main()
