# Asset attributions

These files are committed so the build is reproducible and the app works before
its first network request.

| File | Source | License |
| --- | --- | --- |
| `earth_day.jpg`, `earth_night.jpg`, `earth_clouds.jpg`, `earth_moon.jpg`, `moon_disc.png` | [Solar System Scope textures](https://www.solarsystemscope.com/textures/) | CC BY 4.0, based on NASA imagery |
| `earth_ocean.png` | Derived from the day map by `tools/fetch_assets.py` | CC BY 4.0 (same source) |
| `cities.tsv`, `countries.tsv`, `borders.bin`, `coastlines.bin` | [Natural Earth](https://www.naturalearthdata.com/) | Public domain |
| `satellites.tle`, `satcat.tsv` | [CelesTrak](https://celestrak.org/) | Public GP data; attribution requested |
| `fonts/Manrope-*.ttf` | [Manrope](https://fonts.google.com/specimen/Manrope), Google Fonts | SIL OFL 1.1 |
| `fonts/SpaceGrotesk-*.ttf` | [Space Grotesk](https://fonts.google.com/specimen/Space+Grotesk), Google Fonts | SIL OFL 1.1 |
| `fonts/lucide.ttf` | [Lucide](https://lucide.dev/) | ISC |
| `cacert.pem` | [curl CA bundle](https://curl.se/ca/) | MPL-2.0-style bundle terms |
| `shell/nimbus_background.png`, `shell/nimbus_icon_source.png` | Provided by the project operator | GPL-3.0-or-later |
| `icon.png`, `shell/pic*.png`, `sce_sys/icon0.png`, `sce_sys/pic*.dds` | Generated from the operator art by `tools/make_shell_art.py` and `tools/encode_shell_dds.sh` | GPL-3.0-or-later |

The launcher background DDS files in `sce_sys/` are encoded from
`assets/shell/pic*.png`. The selection audio `sce_sys/snd0.at9` comes from the
ps5-native-app-boilerplate (GPL-3.0-or-later, BlackBearReloaded).

Runtime map tiles, weather, flights and satellite positions are fetched from the
public services credited in the app and in `THIRD_PARTY_NOTICES.md`; they are not
bundled here.
