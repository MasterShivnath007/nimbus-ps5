#!/usr/bin/env python3
"""Fetch and prepare Nimbus' public-domain / openly licensed assets.

Everything this script downloads is credited in assets/ATTRIBUTION.md and in
the app's About screen.  Run it from the repository root:

    .venv/bin/python tools/fetch_assets.py --force

The prepared files are committed to the repository so that builds are
reproducible and the app works before its first network request.
"""

from __future__ import annotations

import argparse
import csv
import io
import math
import os
import shutil
import struct
import sys
import time
import urllib.request
import zipfile
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
CACHE = ROOT / ".cache" / "assets"
FONTS = ASSETS / "fonts"
UA = "NimbusPS5/1.0 (+https://github.com/MasterShivnath007/nimbus-ps5)"


def log(message: str) -> None:
    print(f"[assets] {message}", flush=True)


def download(url: str, destination: Path, *, retries: int = 4) -> Path:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists() and destination.stat().st_size > 0:
        return destination
    last_error: Exception | None = None
    for attempt in range(retries):
        try:
            request = urllib.request.Request(url, headers={"User-Agent": UA})
            with urllib.request.urlopen(request, timeout=90) as response:
                data = response.read()
            if not data:
                raise RuntimeError("empty response")
            destination.write_bytes(data)
            log(f"{destination.name}: {len(data)/1024:.0f} KiB")
            return destination
        except Exception as error:  # noqa: BLE001 - retry any transport failure
            last_error = error
            time.sleep(1.5 * (attempt + 1))
    raise RuntimeError(f"could not download {url}: {last_error}")


def open_image(path: Path) -> Image.Image:
    image = Image.open(path)
    if image.mode != "RGB":
        image = image.convert("RGB")
    return image


def resize_cover(image: Image.Image, width: int, height: int) -> Image.Image:
    """Resize an equirectangular image without changing its aspect ratio."""
    return image.resize((width, height), Image.LANCZOS)


# ---------------------------------------------------------------------------
# Earth textures
# ---------------------------------------------------------------------------

def prepare_earth(force: bool) -> None:
    sources = {
        "day": ("https://www.solarsystemscope.com/textures/download/8k_earth_daymap.jpg", 4096, 2048, 91),
        "night": ("https://www.solarsystemscope.com/textures/download/8k_earth_nightmap.jpg", 4096, 2048, 91),
        "clouds": ("https://www.solarsystemscope.com/textures/download/8k_earth_clouds.jpg", 2048, 1024, 88),
        "moon": ("https://www.solarsystemscope.com/textures/download/2k_moon.jpg", 1024, 512, 90),
    }
    for name, (url, width, height, quality) in sources.items():
        target = ASSETS / f"earth_{name}.jpg"
        if target.exists() and not force:
            continue
        raw = download(url, CACHE / f"earth_{name}_8k.jpg")
        image = resize_cover(open_image(raw), width, height)
        if name == "clouds":
            # The source has a faint bluish cast; make it a neutral luminance
            # map so the cloud shader can tint it for day and night.
            image = image.convert("L").convert("RGB")
        image.save(target, "JPEG", quality=quality, optimize=True, progressive=True)
        log(f"wrote assets/{target.name} ({target.stat().st_size/1024/1024:.1f} MiB)")

    # The sky shader draws the Moon as a textured disc.  Pre-project the
    # equirectangular lunar map onto the Earth-facing hemisphere.
    disc_path = ASSETS / "moon_disc.png"
    if force or not disc_path.exists():
        moon = open_image(ASSETS / "earth_moon.jpg").resize((1024, 512), Image.LANCZOS)
        source = np.asarray(moon, dtype=np.uint8)
        size = 512
        disc = np.zeros((size, size, 4), dtype=np.uint8)
        for py in range(size):
            y = 1.0 - 2.0 * (py + 0.5) / size
            for px in range(size):
                x = 2.0 * (px + 0.5) / size - 1.0
                radius_sq = x * x + y * y
                if radius_sq > 1.0:
                    continue
                z = math.sqrt(max(0.0, 1.0 - radius_sq))
                lon = math.atan2(x, z)
                lat = math.asin(max(-1.0, min(1.0, y)))
                u = int((lon / (2 * math.pi) + 0.5) * (source.shape[1] - 1))
                v = int((0.5 - lat / math.pi) * (source.shape[0] - 1))
                disc[py, px, :3] = source[v, u]
                edge = min(1.0, (1.0 - math.sqrt(radius_sq)) * 80.0)
                disc[py, px, 3] = int(255 * edge)
        Image.fromarray(disc, "RGBA").save(disc_path, optimize=True)
        log(f"wrote assets/{disc_path.name}")

    # An ocean mask makes the specular highlight believable.  It is derived
    # from the day texture: open water is blue and relatively dark, land is
    # warmer and brighter.  A little blur removes JPEG speckle.
    mask_path = ASSETS / "earth_ocean.png"
    if force or not mask_path.exists():
        day = open_image(ASSETS / "earth_day.jpg").resize((2048, 1024), Image.LANCZOS)
        rgb = np.asarray(day, dtype=np.float32) / 255.0
        red, green, blue = rgb[..., 0], rgb[..., 1], rgb[..., 2]
        water = np.clip((blue - 0.55 * red - 0.25 * green - 0.02) * 3.2, 0.0, 1.0)
        water = np.asarray(Image.fromarray((water * 255).astype(np.uint8), "L").filter(
            ImageFilter.GaussianBlur(2.5)), dtype=np.float32) / 255.0
        Image.fromarray((water * 255).astype(np.uint8), "L").save(mask_path, optimize=True)
        log(f"wrote assets/{mask_path.name}")


# ---------------------------------------------------------------------------
# Natural Earth
# ---------------------------------------------------------------------------

def fetch_ne(force: bool, path: str, name: str) -> Path:
    target = CACHE / f"{name}.zip"
    if force or not target.exists():
        download(f"https://naciscdn.org/naturalearth/{path}", target)
    return target


def find_shapefile(archive_path: Path, workdir: Path) -> Path:
    workdir.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive_path) as archive:
        archive.extractall(workdir)
    files = list(workdir.glob("*.shp"))
    if not files:
        raise RuntimeError(f"no shapefile in {archive_path}")
    return files[0]


def clean_text(value: object) -> str:
    if value is None:
        return ""
    text = str(value).replace("\t", " ").replace("\r", " ").replace("\n", " ")
    return " ".join(text.split())


def prepare_cities(force: bool) -> None:
    target = ASSETS / "cities.tsv"
    if target.exists() and not force:
        return
    try:
        import shapefile
    except ImportError:
        log("pyshp is missing; install requirements-dev.txt")
        raise

    archive = fetch_ne(force, "10m/cultural/ne_10m_populated_places_simple.zip", "ne_cities")
    shape_path = find_shapefile(archive, CACHE / "ne_cities")
    reader = shapefile.Reader(str(shape_path))
    fields = [field[0] for field in reader.fields[1:]]
    log(f"cities fields: {', '.join(fields[:18])}")

    def field(record, *names: str) -> object:
        for name in names:
            if name in fields:
                return record[fields.index(name)]
        return None

    rows = []
    for record in reader.records():
        name = clean_text(field(record, "name", "NAME", "NAMEASCII"))
        country = clean_text(field(record, "adm0name", "ADM0NAME", "SOV0NAME"))
        admin = clean_text(field(record, "adm1name", "ADM1NAME"))
        latitude = field(record, "latitude", "LATITUDE", "LAT")
        longitude = field(record, "longitude", "LONGITUDE", "LON")
        population = field(record, "pop_max", "POP_MAX", "POP")
        feature = clean_text(field(record, "featurecla", "FEATURECLA"))
        iso = clean_text(field(record, "iso_a2", "ISO_A2", "iso_3166_1"))
        if not name or latitude is None or longitude is None:
            continue
        try:
            latitude, longitude = float(latitude), float(longitude)
            population = int(float(population or 0))
        except (TypeError, ValueError):
            continue
        if not (-90 <= latitude <= 90 and -180 <= longitude <= 180):
            continue
        capital = 1 if "capital" in feature.lower() else 0
        if population < 5000 and not capital:
            continue
        rows.append((name, country, admin, iso, latitude, longitude, population, capital))

    rows.sort(key=lambda row: (-row[6], row[0]))
    with target.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle, delimiter="\t", lineterminator="\n")
        writer.writerow(["name", "country", "admin", "iso", "lat", "lon", "population", "capital"])
        writer.writerows(rows)
    log(f"wrote assets/cities.tsv ({len(rows)} places)")


def prepare_countries(force: bool) -> None:
    target = ASSETS / "countries.tsv"
    if target.exists() and not force:
        return
    import shapefile

    archive = fetch_ne(force, "10m/cultural/ne_10m_admin_0_countries.zip", "ne_countries")
    shape_path = find_shapefile(archive, CACHE / "ne_countries")
    reader = shapefile.Reader(str(shape_path))
    fields = [field[0] for field in reader.fields[1:]]
    rows = []
    for record in reader.records():
        def value(name: str) -> object:
            return record[fields.index(name)] if name in fields else None

        name = clean_text(value("NAME") or value("ADMIN"))
        iso = clean_text(value("ISO_A2") or value("WB_A2"))
        continent = clean_text(value("CONTINENT") or value("REGION_UN"))
        population = value("POP_EST")
        label_x = value("LABEL_X")
        label_y = value("LABEL_Y")
        if not name:
            continue
        try:
            population = int(float(population or 0))
        except (TypeError, ValueError):
            population = 0
        try:
            label_x = float(label_x or 0)
            label_y = float(label_y or 0)
        except (TypeError, ValueError):
            label_x = label_y = 0.0
        rows.append((name, iso, continent, population, label_x, label_y))
    rows.sort(key=lambda row: row[0])
    with target.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle, delimiter="\t", lineterminator="\n")
        writer.writerow(["name", "iso", "continent", "population", "label_lat", "label_lon"])
        for name, iso, continent, population, label_y, label_x in rows:
            writer.writerow([name, iso, continent, population, label_y, label_x])
    log(f"wrote assets/countries.tsv ({len(rows)} countries)")


def rdp(points: list[tuple[float, float]], epsilon: float) -> list[tuple[float, float]]:
    """Ramer-Douglas-Peucker, iterative to survive long coastlines."""
    if len(points) < 3:
        return points
    keep = [False] * len(points)
    keep[0] = keep[-1] = True
    stack = [(0, len(points) - 1)]
    while stack:
        first, last = stack.pop()
        if last <= first + 1:
            continue
        ax, ay = points[first]
        bx, by = points[last]
        dx, dy = bx - ax, by - ay
        norm = math.hypot(dx, dy)
        best_distance, best_index = -1.0, -1
        for index in range(first + 1, last):
            px, py = points[index]
            if norm < 1e-12:
                distance = math.hypot(px - ax, py - ay)
            else:
                distance = abs(dx * (ay - py) - (ax - px) * dy) / norm
            if distance > best_distance:
                best_distance, best_index = distance, index
        if best_distance > epsilon:
            keep[best_index] = True
            stack.append((first, best_index))
            stack.append((best_index, last))
    return [point for point, retained in zip(points, keep) if retained]


def prepare_linework(force: bool) -> None:
    layers = {
        "borders": ("10m/cultural/ne_10m_admin_0_boundary_lines_land.zip", 0.035),
        "coastlines": ("50m/physical/ne_50m_coastline.zip", 0.045),
    }
    import shapefile

    for name, (path, epsilon) in layers.items():
        target = ASSETS / f"{name}.bin"
        if target.exists() and not force:
            continue
        archive = fetch_ne(force, path, f"ne_{name}")
        shape_path = find_shapefile(archive, CACHE / f"ne_{name}")
        reader = shapefile.Reader(str(shape_path))
        polylines: list[list[tuple[float, float]]] = []
        for shape in reader.shapes():
            points = [(float(x), float(y)) for x, y in shape.points]
            if len(points) < 2:
                continue
            start = 0
            for end in list(shape.parts[1:]) + [len(points)]:
                part = points[start:end]
                start = end
                if len(part) < 2:
                    continue
                simplified = rdp(part, epsilon)
                if len(simplified) >= 2:
                    polylines.append(simplified)
        with target.open("wb") as handle:
            handle.write(b"NIML")
            handle.write(struct.pack("<II", 1, len(polylines)))
            total = 0
            for line in polylines:
                handle.write(struct.pack("<I", len(line)))
                for lon, lat in line:
                    handle.write(struct.pack("<ff", lon, lat))
                total += len(line)
        log(f"wrote assets/{target.name} ({len(polylines)} lines, {total} points)")


# ---------------------------------------------------------------------------
# Satellites
# ---------------------------------------------------------------------------

GROUPS = ["stations", "weather", "gps-ops", "glo-ops", "galileo", "beidou", "science"]


def fetch_text(url: str, destination: Path, retries: int = 4) -> str:
    if destination.exists() and destination.stat().st_size > 0:
        return destination.read_text(encoding="utf-8", errors="replace")
    last_error: Exception | None = None
    for attempt in range(retries):
        try:
            request = urllib.request.Request(url, headers={"User-Agent": UA})
            with urllib.request.urlopen(request, timeout=60) as response:
                text = response.read().decode("utf-8", errors="replace")
            if not text.strip():
                raise RuntimeError("empty response")
            destination.write_text(text, encoding="utf-8")
            return text
        except Exception as error:  # noqa: BLE001
            last_error = error
            time.sleep(2 + attempt * 2)
    raise RuntimeError(f"could not fetch {url}: {last_error}")


def prepare_satellites(force: bool) -> None:
    tle_target = ASSETS / "satellites.tle"
    satcat_target = ASSETS / "satcat.tsv"
    if tle_target.exists() and satcat_target.exists() and not force:
        return

    canonical: dict[int, tuple[str, str, str]] = {}
    for group in GROUPS:
        url = f"https://celestrak.org/NORAD/elements/gp.php?GROUP={group}&FORMAT=tle"
        text = fetch_text(url, CACHE / f"tle_{group}.txt")
        lines = [line.rstrip() for line in text.splitlines() if line.strip()]
        if group != "stations":
            time.sleep(2)
        for index in range(0, len(lines) - 2, 3):
            name, line1, line2 = lines[index:index + 3]
            if not line1.startswith("1 ") or not line2.startswith("2 "):
                continue
            try:
                norad = int(line1[2:7])
            except ValueError:
                continue
            canonical[norad] = (name.strip(), line1, line2)

    if not canonical:
        raise RuntimeError("CelesTrak returned no usable TLEs")

    with tle_target.open("w", encoding="utf-8", newline="\n") as handle:
        for norad in sorted(canonical):
            name, line1, line2 = canonical[norad]
            handle.write(f"{name}\n{line1}\n{line2}\n")
    log(f"wrote assets/satellites.tle ({len(canonical)} objects)")

    csv_text = fetch_text("https://celestrak.org/pub/satcat.csv", CACHE / "satcat.csv")
    if csv_text.lstrip().startswith("<"):
        # Cloudflare or an HTML error page: keep the previous snapshot if any.
        if satcat_target.exists():
            log("SATCAT download was HTML; keeping the previous snapshot")
            return
        raise RuntimeError("SATCAT returned HTML")
    reader = csv.DictReader(io.StringIO(csv_text))
    wanted = set(canonical)
    rows = []
    for row in reader:
        try:
            norad = int((row.get("NORAD_CAT_ID") or "").strip())
        except ValueError:
            continue
        if norad not in wanted:
            continue
        rows.append((
            norad,
            clean_text(row.get("OBJECT_NAME")),
            clean_text(row.get("OWNER")),
            clean_text(row.get("LAUNCH_DATE")),
            clean_text(row.get("DECAY_DATE")),
            clean_text(row.get("OPS_STATUS_CODE")),
            clean_text(row.get("PERIOD")),
            clean_text(row.get("INCLINATION")),
            clean_text(row.get("APOGEE")),
            clean_text(row.get("PERIGEE")),
            clean_text(row.get("OBJECT_TYPE")),
        ))
    with satcat_target.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle, delimiter="\t", lineterminator="\n")
        writer.writerow(["norad", "name", "owner", "launch", "decay", "status",
                         "period", "inclination", "apogee", "perigee", "type"])
        writer.writerows(rows)
    log(f"wrote assets/satcat.tsv ({len(rows)} records)")


# ---------------------------------------------------------------------------
# Fonts and icon
# ---------------------------------------------------------------------------

def instantiate_font(source: Path, destination: Path, weight: int) -> None:
    from fontTools import ttLib
    from fontTools.varLib.instancer import instantiateVariableFont

    font = ttLib.TTFont(source)
    axes = {axis.axisTag: weight for axis in font["fvar"].axes if axis.axisTag == "wght"}
    if axes:
        font = instantiateVariableFont(font, axes, inplace=True)
    destination.parent.mkdir(parents=True, exist_ok=True)
    font.save(destination)
    log(f"wrote assets/fonts/{destination.name}")


def prepare_fonts(force: bool) -> None:
    FONTS.mkdir(parents=True, exist_ok=True)
    sources = {
        "Manrope": ("https://raw.githubusercontent.com/google/fonts/main/ofl/manrope/Manrope%5Bwght%5D.ttf",
                    [400, 500, 600, 700, 800]),
        "SpaceGrotesk": ("https://raw.githubusercontent.com/google/fonts/main/ofl/spacegrotesk/SpaceGrotesk%5Bwght%5D.ttf",
                         [400, 500, 600, 700]),
    }
    for family, (url, weights) in sources.items():
        variable = download(url, CACHE / f"{family}-variable.ttf")
        for weight in weights:
            target = FONTS / f"{family}-{weight}.ttf"
            if force or not target.exists():
                instantiate_font(variable, target, weight)

    for family, license_url in {
        "Manrope": "https://raw.githubusercontent.com/google/fonts/main/ofl/manrope/OFL.txt",
        "SpaceGrotesk": "https://raw.githubusercontent.com/google/fonts/main/ofl/spacegrotesk/OFL.txt",
    }.items():
        target = FONTS / f"{family}-OFL.txt"
        if force or not target.exists():
            text = urllib.request.urlopen(
                urllib.request.Request(license_url, headers={"User-Agent": UA}), timeout=30
            ).read()
            target.write_bytes(text)

    lucide_ttf = ROOT / ".cache" / "lucide" / "lucide.ttf"
    if force or not lucide_ttf.exists():
        archive = download("https://registry.npmjs.org/lucide-static/-/lucide-static-0.468.0.tgz",
                           CACHE / "lucide-static.tgz")
        import tarfile
        with tarfile.open(archive, "r:gz") as tf:
            for member in ("package/font/lucide.ttf", "package/font/info.json"):
                tf.extract(member, ROOT / ".cache" / "lucide")
        shutil.move(str(ROOT / ".cache" / "lucide" / "package" / "font" / "lucide.ttf"),
                    str(lucide_ttf))
    if force or not (FONTS / "lucide.ttf").exists():
        shutil.copy2(lucide_ttf, FONTS / "lucide.ttf")
        log("wrote assets/fonts/lucide.ttf")
    license_target = FONTS / "lucide-LICENSE.txt"
    if force or not license_target.exists():
        text = urllib.request.urlopen(
            urllib.request.Request("https://raw.githubusercontent.com/lucide-icons/lucide/main/LICENSE",
                                   headers={"User-Agent": UA}), timeout=30).read()
        license_target.write_bytes(text)


ICON_WORDS = {
    "wind": "\\e1af",
    "thermometer": "\\e185",
    "droplets": "\\e0b9",
    "gauge": "\\e1be",
    "globe": "\\e0eb",
    "layers": "\\e52d",
    "plane": "\\e1dd",
    "satellite": "\\e44b",
    "settings": "\\e157",
    "search": "\\e154",
    "sun": "\\e17b",
    "moon": "\\e121",
    "clock": "\\e08b",
    "arrow-up": "\\e04e",
    "arrow-down": "\\e046",
    "chevron-right": "\\e073",
    "chevron-left": "\\e072",
    "chevron-down": "\\e071",
    "chevron-up": "\\e074",
    "refresh-cw": "\\e148",
    "play": "\\e13f",
    "pause": "\\e131",
    "map-pin": "\\e114",
    "cloud": "\\e08c",
    "cloud-sun": "\\e215",
    "cloud-moon": "\\e214",
    "cloud-rain": "\\e092",
    "cloud-lightning": "\\e090",
    "snowflake": "\\e168",
    "waves": "\\e282",
    "navigation": "\\e126",
    "x": "\\e1b1",
    "check": "\\e070",
    "sliders-horizontal": "\\e299",
    "expand": "\\e219",
    "minimize-2": "\\e11e",
    "radio": "\\e145",
    "activity": "\\e038",
    "compass": "\\e09f",
    "orbit": "\\e3ea",
    "route": "\\e542",
    "triangle-alert": "\\e192",
    "info": "\\e0fe",
    "github": "\\e0e9",
    "thermometer-sun": "\\e187",
    "thermometer-snowflake": "\\e186",
}



def write_icon_header() -> None:
    include = ROOT / "src" / "ui"
    include.mkdir(parents=True, exist_ok=True)
    entries = "\n".join(
        f'    {{"{name}", 0x{code[1:]}}},' for name, code in sorted(ICON_WORDS.items())
    )
    (include / "icon_codes.hpp").write_text(f"""// Generated by tools/fetch_assets.py. Lucide icons, ISC license.
#pragma once
#include <cstdint>

namespace nimbus::ui::icons {{
struct IconCode {{ const char *name; char32_t codepoint; }};
inline constexpr IconCode kIcons[] = {{
{entries}
}};
}}  // namespace nimbus::ui::icons
""", encoding="utf-8")
    log("wrote src/ui/icon_codes.hpp")


def prepare_icon(force: bool) -> None:
    target = ROOT / "sce_sys" / "icon0.png"
    if target.exists() and not force:
        return
    target.parent.mkdir(parents=True, exist_ok=True)

    size = 1024
    canvas = Image.new("RGB", (size, size), "#050A10")
    pixels = np.zeros((size, size, 3), dtype=np.float32)

    # Deep space gradient with a faint teal aurora from the lower left.
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32) / size
    top = np.array([0.020, 0.031, 0.050], dtype=np.float32)
    bottom = np.array([0.030, 0.078, 0.096], dtype=np.float32)
    pixels = top[None, None, :] + (bottom - top)[None, None, :] * (yy ** 1.4)[..., None]
    aurora = np.exp(-(((xx - 0.20) ** 2 + (yy - 0.88) ** 2) / 0.16))
    pixels += aurora[..., None] * np.array([0.03, 0.16, 0.14], dtype=np.float32)
    stars = np.random.default_rng(7)
    for _ in range(190):
        sx, sy = stars.random(2)
        brightness = stars.random() ** 2.2 * 0.55
        x, y = int(sx * size), int(sy * size * 0.72)
        pixels[max(0, y - 1):y + 2, max(0, x - 1):x + 2] += brightness * 0.22
    base = Image.fromarray((np.clip(pixels, 0, 1) * 255).astype(np.uint8), "RGB")

    # A globe cropped from the day texture, lit from the upper right.
    day = open_image(ASSETS / "earth_day.jpg").resize((2048, 1024), Image.LANCZOS)
    globe_size = 760
    # Sample a square around the Indian Ocean, near the equator, so the icon
    # reads as a planet rather than a polar cap.
    source = day.crop((1390 - 420, 512 - 420, 1390 + 420, 512 + 420)).resize(
        (globe_size, globe_size), Image.LANCZOS)
    mask = Image.new("L", (globe_size, globe_size), 0)
    draw = ImageDraw.Draw(mask)
    draw.ellipse((0, 0, globe_size - 1, globe_size - 1), fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(0.6))
    globe = Image.new("RGB", (globe_size, globe_size), "#04070B")
    globe.paste(source, (0, 0), mask)

    # Shade the sphere: a radial light in the upper right and a dark limb.
    gx, gy = np.mgrid[0:globe_size, 0:globe_size].astype(np.float32) / globe_size
    light = np.clip(1.15 - 1.9 * np.sqrt((gx - 0.68) ** 2 + (gy - 0.32) ** 2), 0.10, 1.0)
    globe_np = np.asarray(globe, dtype=np.float32) * light[..., None]
    globe_np[..., 1] *= 1.02
    globe_np[..., 2] *= 1.05
    globe = Image.fromarray(np.clip(globe_np, 0, 255).astype(np.uint8), "RGB")

    glow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(glow)
    cx, cy = size // 2, size // 2 + 30
    radius = globe_size // 2 + 18
    draw.ellipse((cx - radius, cy - radius, cx + radius, cy + radius),
                 outline=(84, 231, 213, 255), width=7)
    draw.ellipse((cx - radius - 8, cy - radius - 8, cx + radius + 8, cy + radius + 8),
                 outline=(84, 231, 213, 70), width=2)
    glow = glow.filter(ImageFilter.GaussianBlur(10))
    base = Image.alpha_composite(base.convert("RGBA"), glow)

    base.paste(globe, (cx - globe_size // 2, cy - globe_size // 2), mask)

    # Atmospheric rim.
    rim = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(rim)
    draw.ellipse((cx - globe_size // 2 - 3, cy - globe_size // 2 - 3,
                  cx + globe_size // 2 + 3, cy + globe_size // 2 + 3),
                 outline=(106, 197, 255, 210), width=5)
    rim = rim.filter(ImageFilter.GaussianBlur(6))
    base = Image.alpha_composite(base, rim)

    # A thin orbital arc and a satellite glint.
    orbit = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(orbit)
    bbox = (cx - 470, cy - 360, cx + 470, cy + 360)
    draw.arc(bbox, start=202, end=340, fill=(231, 246, 255, 200), width=3)
    dot_x = cx + int(440 * math.cos(math.radians(340)))
    dot_y = cy + int(340 * math.sin(math.radians(340)))
    draw.ellipse((dot_x - 12, dot_y - 12, dot_x + 12, dot_y + 12), fill=(94, 231, 208, 255))
    dot_glow = orbit.filter(ImageFilter.GaussianBlur(12))
    base = Image.alpha_composite(base, dot_glow)
    base = Image.alpha_composite(base, orbit)

    # Slight vignette.
    vignette = Image.new("L", (size, size), 0)
    vdraw = ImageDraw.Draw(vignette)
    vdraw.ellipse((-180, -180, size + 180, size + 180), fill=255)
    vignette = vignette.filter(ImageFilter.GaussianBlur(150))
    black = Image.new("RGBA", (size, size), (0, 0, 0, 90))
    base = Image.composite(base, Image.alpha_composite(base, black), vignette)

    icon = base.convert("RGB").resize((512, 512), Image.LANCZOS)
    icon.save(target, "PNG", optimize=True)
    icon.save(ASSETS / "icon.png", "PNG", optimize=True)
    log(f"wrote {target} ({target.stat().st_size/1024:.0f} KiB)")


def prepare_ca_bundle(force: bool) -> None:
    target = ASSETS / "cacert.pem"
    if target.exists() and not force:
        return
    data = urllib.request.urlopen(
        urllib.request.Request("https://curl.se/ca/cacert.pem", headers={"User-Agent": UA}),
        timeout=60,
    ).read()
    target.write_bytes(data)
    log(f"wrote assets/cacert.pem ({len(data)/1024:.0f} KiB)")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--force", action="store_true", help="re-download and rebuild everything")
    parser.add_argument("--skip-earth", action="store_true")
    parser.add_argument("--skip-satellites", action="store_true")
    args = parser.parse_args()

    ASSETS.mkdir(parents=True, exist_ok=True)
    CACHE.mkdir(parents=True, exist_ok=True)
    if not args.skip_earth:
        prepare_earth(args.force)
    prepare_cities(args.force)
    prepare_countries(args.force)
    prepare_linework(args.force)
    if not args.skip_satellites:
        prepare_satellites(args.force)
    prepare_fonts(args.force)
    prepare_ca_bundle(args.force)
    write_icon_header()
    prepare_icon(args.force)
    log("done")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
