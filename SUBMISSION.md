# Submitting Nimbus to ProsperoStore

ProsperoStore lists **native PS5 homebrew titles** whose artifacts are published
as GitHub release assets. This page is the release and submission checklist for
Nimbus.

Title ID: **PPSA42716** (checked against the catalog's JSON feed; unused at the
time of writing). Name: **Nimbus** (no catalog name collision).

## Status — 2026-11-23

- Repository: <https://github.com/MasterShivnath007/nimbus-ps5>
- Release: <https://github.com/MasterShivnath007/nimbus-ps5/releases/tag/01.000.000>
- Artifact SHA-256: `97159d16b6a5e8546b6b1ac0a4f1339002117909c4c3f114fd8d669dd66fb8cc`
- Catalog record: filled in `catalog/PPSA42716.json`; the PR is **not open yet**.
- Hardware pass: **pending**. Do not open the catalog PR until the title has
  been launched on a PS5 and the checks in section 5 pass.

## 1. Publish the repository

Create a public GitHub repository from this source tree, for example:

```bash
gh repo create MasterShivnath007/nimbus-ps5 --public --source=. --remote=origin --push
```

The catalog requires:

- source, or a clear pointer to it, in the repository;
- a license that allows redistribution (this project is GPL-3.0);
- a unique title ID in `sce_sys/param.json`;
- an icon (`sce_sys/icon0.png`, 512×512 PNG) at a stable HTTPS URL.

## 2. Publish a release artifact

Build on Linux (or download the `PPSA42716-ps5` artifact from the GitHub Actions
`PS5 · native title folder` job; the workflow deliberately does not attach it to
a release, so the published hash cannot change under you):

```bash
PS5_NATIVE_APP_TEMPLATE=/opt/ps5-native-app-boilerplate \
PS5_OPENGL_PREFIX=/opt/ps5-opengl/ps5-opengl-sdk-1.0.2/sdk \
bash ps5/build_ps5.sh
```

The artifact is a `.zip` with the title folder at the top:

```
PPSA42716-01.000.000.zip
└── PPSA42716/
    ├── eboot.bin
    ├── sce_module/libc.prx
    ├── sce_sys/...
    └── assets/...
```

Publish it as a GitHub release:

```bash
shasum -a 256 out/PPSA42716-01.000.000.zip
gh release create 01.000.000 \
  out/PPSA42716-01.000.000.zip \
  out/PPSA42716-01.000.000.zip.sha256 \
  --title "Nimbus 01.000.000" \
  --notes "First release. Live Earth, weather layers, flights, satellites."
```

Do not rebuild or replace this asset after listing it: the catalog validates the
exact SHA-256 of the release asset. Publish a new tag for any new build.

Rules used by the catalog:

- the release tag is the version the site shows (`01.000.000`);
- upload the zip as a **release asset**, never a repository file;
- do not replace an asset after it has been listed; publish a new tag;
- raise `contentVersion` in `sce_sys/param.json` for every release;
- keep the artifact URL stable.

Compute the digest:

```bash
shasum -a 256 out/PPSA42716-01.000.000.zip
```

## 3. Fill the catalog record

The record in `catalog/PPSA42716.json` has been filled with the published
release. It contains:

```json
{
  "titleid": "PPSA42716",
  "name": "Nimbus",
  "kind": "app",
  "category": "utilities",
  "description": "A live 3D Earth for PS5: real-time weather layers, city-level zoom, flights and satellites, piloted with the DualSense.",
  "license": "GPL-3.0",
  "author": "MasterShivnath007",
  "version": "01.000.000",
  "source_repo": "https://github.com/MasterShivnath007/nimbus-ps5",
  "artifact_url": "https://github.com/MasterShivnath007/nimbus-ps5/releases/download/01.000.000/PPSA42716-01.000.000.zip",
  "sha256": "<64 lowercase hex characters>",
  "icon_url": "https://raw.githubusercontent.com/MasterShivnath007/nimbus-ps5/01.000.000/sce_sys/icon0.png"
}
```

Every field must be a string. The source repository capitalization must match
GitHub exactly. The artifact URL must point at a specific tag and end in `.zip`.
The icon must be a direct image URL ending in `.png`, `.jpg`, `.jpeg` or `.webp`,
at least 256×256, square, and at most 2 MiB.

## 4. Open the pull request

Fork `darkxex/ps5-homebrew-catalog`, add the record as
`apps/PPSA42716.json`, and open the pull request **from the GitHub account that
owns the source repository**. The automated `Validate submission` check verifies
the digest, release, icon and metadata. A maintainer reviews the following:

- **Publisher** — the repository owner is the submitter.
- **Provenance** — Nimbus' source is in the repository; the build instructions
  are public and pinned to the native-app boilerplate and ps5-opengl release.
- **Distinct app** — a live Earth with weather, flights, satellites and a
  controller-first screensaver; not a fork or reskin of an existing listing.
- **Honesty** — name, description and icon describe the app; no official Sony
  branding or affiliation.
- **Scope** — native PS5 title with `eboot.bin` + `sce_sys/param.json`; no ELF
  payload, ROMs or web page.
- **License** — GPL-3.0 and all bundled assets permit redistribution.

## 5. Before clicking submit

- [ ] The PS5 title launches and closes cleanly on the target homebrew firmware.
- [ ] The DualSense is detected; sticks, triggers and face buttons work.
- [ ] Weather, flights, satellites and tiles load on the console.
- [ ] The app works after a cold boot with no cache.
- [ ] `assets/fonts` and the image sources are credited.
- [ ] The icon is 512×512 and under 2 MiB.
- [ ] The release zip contains exactly the title folder.
- [ ] `contentVersion` is higher than any previous build.
- [ ] The record's `sha256` matches the exact uploaded asset.
- [ ] The PR changes only `apps/PPSA42716.json`.

## 6. Licensing notes for the reviewer

- App source: GPL-3.0-or-later. The PS5 build links GPL-3.0 ps5-opengl and
  GPL-3.0 boilerplate tooling; notices are retained.
- Bundled textures: Solar System Scope CC BY 4.0; Natural Earth public domain.
- Runtime map/weather/aircraft data is fetched, not bundled, with attribution
  shown in the app and listed in `THIRD_PARTY_NOTICES.md`.
- High-zoom Esri basemap tiles are used under Esri's public basemap terms; the
  app is free and non-commercial, and credits the source in the UI. The
  developer must confirm these terms for their own distribution.
- No Sony SDK, proprietary key, ROM or copyrighted game asset is included.

A reservation record (`catalog/PPSA42716.json` with null links) can hold the
title ID before the first build is published. Releasing it requires the same
GitHub account that opened the reservation.
