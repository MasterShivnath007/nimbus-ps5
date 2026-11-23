#!/usr/bin/env bash
# Encode assets/shell/pic0.png and pic1.png as the BC7 DDS files used by the
# PS5 launcher. Requires bc7enc (richgel999/bc7enc_rdo) in PATH or BC7ENC.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
encoder=${BC7ENC:-}
if [[ -z $encoder ]]; then
    if command -v bc7enc >/dev/null; then
        encoder=$(command -v bc7enc)
    elif [[ -x "$root/.cache/bc7enc_rdo-master/bc7enc" ]]; then
        encoder="$root/.cache/bc7enc_rdo-master/bc7enc"
    else
        echo "bc7enc not found; set BC7ENC to the encoder binary" >&2
        exit 2
    fi
fi
for i in 0 1; do
    "$encoder" -q "$root/assets/shell/pic$i.png" "$root/sce_sys/pic$i.dds"
    python3 - "$root/sce_sys/pic$i.dds" <<'PY'
import struct, sys
from pathlib import Path
path = Path(sys.argv[1])
data = bytearray(path.read_bytes())
struct.pack_into("<I", data, 28, 1)  # one mip level
path.write_bytes(data)
PY
done
python3 - "$root/sce_sys" <<'PY'
import struct, sys
from pathlib import Path
for name in ("pic0.dds", "pic1.dds"):
    path = Path(sys.argv[1]) / name
    data = path.read_bytes()
    height, width = struct.unpack_from("<II", data, 12)
    assert data[:4] == b"DDS " and (width, height) == (3840, 2160)
    assert struct.unpack_from("<I", data, 28)[0] == 1
    assert struct.unpack_from("<I", data, 128)[0] == 98  # DXGI_FORMAT_BC7_UNORM
    print(f"{name}: {width}x{height} BC7, one mip")
PY
