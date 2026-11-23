#!/usr/bin/env python3
"""Compose the PS5 shell backgrounds and launcher icon from the operator art.

Source images (provided by the project operator):
  assets/shell/nimbus_background.png  - Earth limb from orbit
  assets/shell/nimbus_icon_source.png - night-side India / Asia

The script writes the 4K PNGs used by the DDS encoder and the 512x512 icon.
Run tools/encode_shell_dds.sh afterwards to rebuild sce_sys/pic0.dds and
sce_sys/pic1.dds.
"""
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
SHELL = ASSETS / "shell"
DOCS = ROOT / "docs" / "screenshots"
W, H = 3840, 2160


def cover(image: Image.Image, width: int, height: int) -> Image.Image:
    image = image.convert("RGB")
    scale = max(width / image.width, height / image.height)
    resized = image.resize(
        (int(image.width * scale + 0.5), int(image.height * scale + 0.5)),
        Image.LANCZOS,
    )
    left = (resized.width - width) // 2
    top = (resized.height - height) // 2
    return resized.crop((left, top, left + width, top + height))


def left_scrim(image: Image.Image, strength: float) -> Image.Image:
    width, height = image.size
    x = np.linspace(0.0, 1.0, width, dtype=np.float32)
    alpha = (np.clip(1.0 - x * 2.2, 0.0, 1.0) ** 1.7 * 255.0 * strength).astype(np.uint8)
    overlay = np.zeros((height, width, 4), dtype=np.uint8)
    overlay[..., 0:3] = 2
    overlay[..., 3] = alpha[None, :]
    return Image.alpha_composite(image.convert("RGBA"), Image.fromarray(overlay, "RGBA"))


def make_icon(source: Image.Image) -> Image.Image:
    side = min(source.width, source.height)
    left = (source.width - side) // 2
    crop = source.crop((left, 0, left + side, side)).resize((512, 512), Image.LANCZOS)
    pixels = np.asarray(crop, dtype=np.float32) / 255.0
    pixels = np.clip((pixels - 0.5) * 1.06 + 0.5, 0.0, 1.0)
    return Image.fromarray((pixels * 255.0).astype(np.uint8), "RGB")


def main() -> None:
    background_path = SHELL / "nimbus_background.png"
    icon_path = SHELL / "nimbus_icon_source.png"
    if not background_path.exists() or not icon_path.exists():
        raise SystemExit("missing assets/shell/nimbus_background.png or nimbus_icon_source.png")

    background = cover(Image.open(background_path), W, H)
    pic0 = left_scrim(background, 0.50).convert("RGB")
    # The second shell image mirrors the limb, giving the launcher a different
    # crop while keeping the dark side under the system's title text.
    pic1 = left_scrim(background.transpose(Image.FLIP_LEFT_RIGHT), 0.60).convert("RGB")
    pic0.save(SHELL / "pic0.png", "PNG", optimize=True)
    pic1.save(SHELL / "pic1.png", "PNG", optimize=True)

    icon = make_icon(Image.open(icon_path).convert("RGB"))
    icon.save(ASSETS / "icon.png", "PNG", optimize=True)
    (ROOT / "sce_sys").mkdir(exist_ok=True)
    icon.save(ROOT / "sce_sys" / "icon0.png", "PNG", optimize=True)

    DOCS.mkdir(parents=True, exist_ok=True)
    pic0.resize((1920, 1080), Image.LANCZOS).save(DOCS / "shell-pic0.jpg", "JPEG", quality=92)
    pic1.resize((1920, 1080), Image.LANCZOS).save(DOCS / "shell-pic1.jpg", "JPEG", quality=92)
    icon.save(DOCS / "icon.png", "PNG", optimize=True)
    for path in (SHELL / "pic0.png", SHELL / "pic1.png", ASSETS / "icon.png"):
        print(f"wrote {path.relative_to(ROOT)} ({path.stat().st_size / 1024 / 1024:.1f} MiB)")


if __name__ == "__main__":
    main()
