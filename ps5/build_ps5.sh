#!/usr/bin/env bash
# Build Nimbus as a native PS5 title folder.
#
# The title is assembled by ps5-native-app-boilerplate and renders through the
# ps5-opengl SDK.  This script follows the integration used by ps5-opengl's own
# native test titles:
#   * the boilerplate supplies the CRT, FSELF converter, runtime shim and
#     title-folder assembly;
#   * ps5-opengl's linker layout, runtime shims and app heap are overlaid;
#   * libPS5OpenGLCore33.a is linked in one static group with libc++/libunwind.
#
# Requirements (Linux x86-64):
#   PS5_NATIVE_APP_TEMPLATE   boilerplate checkout after `make deps libc`
#   PS5_OPENGL_PREFIX         extracted ps5-opengl-sdk-*/sdk directory
# Optional:
#   PS5_OPENGL_SOURCE         ps5-opengl v1.0.2 source checkout (auto-downloaded)
#   PS5_CLANG / PS5_CXX       clang-18 / clang++-18
#   PS5_PAYLOAD_SDK           defaults to the boilerplate's .deps/native SDK
#   WORK                      staging directory (default: .ps5build)
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
template=${PS5_NATIVE_APP_TEMPLATE:?set PS5_NATIVE_APP_TEMPLATE to the boilerplate checkout}
gl=${PS5_OPENGL_PREFIX:?set PS5_OPENGL_PREFIX to ps5-opengl-sdk-*/sdk}
sdk=${PS5_PAYLOAD_SDK:-"$template/.deps/native/ps5-payload-sdk"}
clang=${PS5_CLANG:-clang-18}
cxx=${PS5_CXX:-clang++-18}
work=${WORK:-"$root/.ps5build"}
title=PPSA42716
app="$work/$title"
out="$root/out"
version=01.000.000

for path in "$template/Makefile" "$template/tooling/prospero-clang18" \
            "$template/runtime/libc.prx" "$sdk/bin/prospero-lld" \
            "$gl/lib/libPS5OpenGLCore33.a" "$root/sce_sys/param.json" \
            "$root/sce_sys/icon0.png" "$root/sce_sys/pic0.dds" \
            "$root/sce_sys/pic1.dds"; do
    [[ -e $path ]] || { echo "missing required path: $path" >&2; exit 2; }
done
command -v "$clang" >/dev/null || { echo "missing compiler: $clang" >&2; exit 2; }
command -v "$cxx" >/dev/null || { echo "missing compiler: $cxx" >&2; exit 2; }
command -v python3 >/dev/null || { echo "missing python3" >&2; exit 2; }

# The prebuilt SDK package deliberately omits the ps5-opengl-native integration
# files.  They are part of the tagged source release.
glsrc=${PS5_OPENGL_SOURCE:-}
if [[ -z $glsrc ]]; then
    glsrc="$work/ps5-opengl-src"
    if [[ ! -f "$glsrc/native-app/ps5-pie.ld" ]]; then
        mkdir -p "$work"
        archive="$work/ps5-opengl-v1.0.2.tar.gz"
        curl -L --fail --retry 3 -o "$archive" \
            https://github.com/darkxex/ps5-opengl/archive/refs/tags/v1.0.2.tar.gz
        rm -rf "$glsrc"
        mkdir -p "$glsrc"
        tar -xzf "$archive" -C "$glsrc" --strip-components=1
    fi
fi
for file in runtime_shims.c app_heap.c ps5-pie.ld app-symbols.map; do
    [[ -f "$glsrc/native-app/$file" ]] || {
        echo "ps5-opengl source is missing native-app/$file" >&2
        exit 2
    }
done

# --- Stage the boilerplate skeleton ------------------------------------------
rm -rf -- "$work/$title"
mkdir -p "$app" "$out"
cp -a "$template/Makefile" "$app/Makefile"
for directory in tools tooling runtime; do
    cp -a "$template/$directory" "$app/$directory"
done
mkdir -p "$app/.deps"
cp -a "$template/.deps/native" "$app/.deps/native"
sdk="$app/.deps/native/ps5-payload-sdk"
[[ -x "$sdk/bin/prospero-lld" ]] || { echo "staged payload SDK is incomplete" >&2; exit 2; }

# The boilerplate ships a demo in src/; start clean so only Nimbus is linked.
rm -rf -- "$app/src" "$app/include" "$app/vendor" "$app/build"
mkdir -p "$app/src" "$app/include" "$app/third_party" "$app/vendor"
cp -a "$root/src/." "$app/src/"
rm -f -- "$app/src/platform/desktop.cpp"   # SDL desktop backend is not part of the title
cp -a "$root/third_party/stb" "$app/third_party/stb"
cp -a "$root/third_party/sgp4" "$app/third_party/sgp4"
mkdir -p "$app/src/sgp4"
cp "$root/third_party/sgp4/SGP4.c" "$root/third_party/sgp4/TLE.c" "$app/src/sgp4/"
cp -a "$root/assets" "$app/assets"
mkdir -p "$app/sce_sys"
cp -a "$root/sce_sys/." "$app/sce_sys/"
for asset in icon0.png pic0.dds pic1.dds snd0.at9; do
    [[ -f "$app/sce_sys/$asset" ]] || cp "$template/sce_sys/$asset" "$app/sce_sys/$asset"
done

# --- OpenGL public headers and the ps5-opengl native runtime -----------------
for header_dir in EGL GL KHR; do
    cp -a "$gl/include/$header_dir" "$app/include/"
done
for optional in ps5_opengl_display.h ps5_opengl_display_modes.h; do
    [[ -f "$gl/include/$optional" ]] && cp "$gl/include/$optional" "$app/include/"
done
cp "$glsrc/native-app/runtime_shims.c" "$app/src/runtime_shims.c"
cp "$glsrc/native-app/app_heap.c" "$app/src/app_heap.c"
cp "$glsrc/native-app/ps5-pie.ld" "$app/tooling/native/ps5-pie.ld"
cp "$glsrc/native-app/app-symbols.map" "$app/tooling/native/app-symbols.map"

# --- Boilerplate adaptations used by ps5-opengl native titles ----------------
python3 - "$app" <<'PY'
import sys
from pathlib import Path

app = Path(sys.argv[1])

# 256 MiB process heap rather than the boilerplate default.
writer = app / "tooling/native/sce_module_writer.cpp"
text = writer.read_text()
needle = "write_u64(result.data, result.heap_size, std::numeric_limits<std::uint64_t>::max());"
if needle not in text:
    raise SystemExit("unexpected heap constant in sce_module_writer.cpp")
writer.write_text(text.replace(needle, "write_u64(result.data, result.heap_size, 0x10000000ULL);"))

# The complete allocator wrap set is required by the ps5-opengl app heap.
build = app / "tools/build.sh"
text = build.read_text()
marker = "--eh-frame-hdr \\"
if marker not in text:
    raise SystemExit("unexpected linker line in tools/build.sh")
text = text.replace(
    marker,
    "--eh-frame-hdr --wrap=malloc --wrap=calloc --wrap=realloc --wrap=free "
    "--wrap=posix_memalign --wrap=malloc_usable_size \\",
)
build.write_text(text)
PY

# --- AGC import stubs ---------------------------------------------------------
mkdir -p "$work/imports"
PS5_PAYLOAD_SDK="$sdk" sh "$app/tooling/prospero-clang18" -std=c11 -O2 -fPIC \
    -ffunction-sections -fdata-sections -c "$glsrc/native-app/agc_link_stub.c" \
    -o "$work/imports/agc_link_stub.o"
"$sdk/bin/prospero-lld" --shared -soname libSceAgc.prx \
    -o "$sdk/target/lib/libSceAgc.so" "$work/imports/agc_link_stub.o"
PS5_PAYLOAD_SDK="$sdk" sh "$app/tooling/prospero-clang18" -std=c11 -O2 -fPIC \
    -ffunction-sections -fdata-sections -c "$glsrc/native-app/agc_driver_link_stub.c" \
    -o "$work/imports/agc_driver_link_stub.o"
"$sdk/bin/prospero-lld" --shared -soname libSceAgcDriver.prx \
    -o "$sdk/target/lib/libSceAgcDriver.so" "$work/imports/agc_driver_link_stub.o"

# --- Static group -------------------------------------------------------------
compiler_runtime=$("$clang" --print-resource-dir)/lib/linux/libclang_rt.builtins-x86_64.a
[[ -f $compiler_runtime ]] || {
    echo "compiler builtins not found: $compiler_runtime" >&2
    exit 2
}
group="$app/vendor/libnimbus_group.a"
{
    printf 'SEARCH_DIR("%s")\n' "$sdk/target/lib"
    printf 'SEARCH_DIR("%s")\n' "$gl/lib"
    printf 'EXTERN(ps5_agc_gate2_run)\n'
    printf 'GROUP (\n'
    for library in \
        "$gl/lib/libPS5OpenGLCore33.a" \
        "$sdk/target/lib/libunwind.a" \
        "$sdk/target/lib/libc++abi.a" \
        "$sdk/target/lib/libc++.a" \
        "$sdk/target/lib/libpthread.a" \
        "$compiler_runtime"; do
        printf '  "%s"\n' "$library"
    done
    printf ')\n'
} > "$group"

cat > "$app/.env" <<'EOF'
APP_DEFINITIONS = NIMBUS_PS5=1 GL_GLEXT_PROTOTYPES=1
APP_INCLUDE_PATHS = src:include:third_party/stb:third_party/sgp4
APP_STATIC_ARCHIVES = vendor/libnimbus_group.a
EOF

# --- Build --------------------------------------------------------------------
CXX="$cxx" make -C "$app" --no-print-directory -j"$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN)" app

dist="$app/dist/$title"
for file in eboot.bin sce_module/libc.prx sce_sys/param.json sce_sys/icon0.png; do
    [[ -s "$dist/$file" ]] || { echo "build did not produce $dist/$file" >&2; exit 2; }
done

archive="$out/$title-$version.zip"
rm -f -- "$archive"
(cd "$app/dist" && zip -qr "$archive" "$title")
if command -v sha256sum >/dev/null; then
    (cd "$out" && sha256sum "$(basename "$archive")" | tee "$archive.sha256")
else
    (cd "$out" && shasum -a 256 "$(basename "$archive")" | tee "$archive.sha256")
fi
printf '\nNative title: %s\nArtifact:     %s\n' "$dist" "$archive"
