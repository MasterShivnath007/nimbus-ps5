# PS5 packaging

The native-title build lives in [`../BUILD-PS5.md`](../BUILD-PS5.md) and
[`build_ps5.sh`](build_ps5.sh). `sce_sys/param.json` declares the title identity
`PPSA42716` and version `01.000.000`; the launcher icon is
`sce_sys/icon0.png`.

Quick check of the metadata:

```bash
python3 - <<'PY'
import json, re
p = json.load(open('sce_sys/param.json'))
assert re.fullmatch(r'PPSA\d{5}', p['titleId'])
assert re.fullmatch(r'[A-Z]{2}\d{4}-PPSA\d{5}_00-[A-Z0-9]{16}', p['contentId'])
print(p['titleId'], p['contentVersion'], p['localizedParameters']['en-US']['titleName'])
PY
```

The build is a title folder, not an ELF:

```
PPSA42716/
  eboot.bin
  sce_module/libc.prx
  sce_sys/param.json
  sce_sys/icon0.png
  sce_sys/pic0.dds
  sce_sys/pic1.dds
  sce_sys/snd0.at9
  assets/
```

Zip the folder with `PPSA42716/` at the archive root and attach the zip to a
GitHub release. The catalog listing requires that exact artifact plus its
SHA-256.
