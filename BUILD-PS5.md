# Building Nimbus for PlayStation 5

Nimbus is a **native PS5 title**, not an ELF payload. The build produces:

```
PPSA42716/
  eboot.bin
  sce_module/libc.prx
  sce_sys/param.json
  sce_sys/icon0.png
  sce_sys/pic0.dds
  sce_sys/pic1.dds
  sce_sys/snd0.at9
  assets/...
```

The title is packaged by
[ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate)
and renders through
[ps5-opengl](https://github.com/blackbearreloaded/ps5-opengl). Both are
GPL-3.0-or-later; their notices are retained.

## Requirements

- Linux x86-64 (Ubuntu 24.04 is used by CI) or WSL2.
- Clang 18 + LLD 18, `make`, Python 3 with `pyelftools`, zlib headers, `zip`,
  `sha256sum`, `wget`.
- A checkout of **ps5-native-app-boilerplate** at commit
  `470695e0c557f99ff2df0e36e4df713c5e636526`.
- The **ps5-opengl SDK 1.0.2** release archive from
  `https://github.com/darkxex/ps5-opengl/releases/tag/v1.0.2`.
- No proprietary Sony SDK is used.

```bash
sudo apt-get update
sudo apt-get install -y clang-18 clang++-18 lld-18 make cmake ninja-build \
  python3 python3-pyelftools pkg-config curl wget unzip zip zlib1g-dev
```

## 1. Prepare the boilerplate

```bash
git clone https://github.com/drakmor/ps5-native-app-boilerplate /opt/ps5-native-app-boilerplate
git -C /opt/ps5-native-app-boilerplate checkout 470695e0c557f99ff2df0e36e4df713c5e636526
make -C /opt/ps5-native-app-boilerplate deps libc
```

`deps` downloads the public ps5-payload-sdk and zlib. `libc` builds the
clean-room runtime shim (`runtime/libc.prx`).

## 2. Fetch the OpenGL SDK

```bash
mkdir -p /opt/ps5-opengl
curl -L -o /opt/ps5-opengl/sdk.tar.gz \
  https://github.com/darkxex/ps5-opengl/releases/download/v1.0.2/ps5-opengl-sdk-1.0.2.tar.gz
tar -xzf /opt/ps5-opengl/sdk.tar.gz -C /opt/ps5-opengl
```

The SDK's `sdk/` directory is passed to the build as `PS5_OPENGL_PREFIX`.

## 3. Build Nimbus

```bash
PS5_NATIVE_APP_TEMPLATE=/opt/ps5-native-app-boilerplate \
PS5_OPENGL_PREFIX=/opt/ps5-opengl/ps5-opengl-sdk-1.0.2/sdk \
PS5_CLANG=clang-18 \
PS5_CXX=clang++-18 \
bash ps5/build_ps5.sh
```

The script downloads the tagged ps5-opengl v1.0.2 source (for the native-app
linker layout and runtime shim) unless `PS5_OPENGL_SOURCE` points at a checkout,
then:

1. copies the tested boilerplate skeleton into `.ps5build/PPSA42716`;
2. overlays Nimbus' sources, assets and `sce_sys`;
3. installs the OpenGL headers, `app_heap.c`, `runtime_shims.c`, linker script
   and symbol map from the tagged ps5-opengl source;
4. applies the 256 MiB heap, allocator wraps and linker layout used by
   ps5-opengl native titles;
5. builds the AGC import stubs;
6. links the title against `libPS5OpenGLCore33.a`, libc++, libunwind and the
   SDK stubs (DualSense, user service, HTTP/SSL/Net);
7. produces `out/PPSA42716-01.000.000.zip` and its SHA-256.

The output is reproducible from the pinned boilerplate commit and SDK release.

## 4. Deploy to a console

The artifact is a folder zip for **ShadowMountPlus** or a similar native-title
loader. Unzip it so that `PPSA42716/` sits at the target path (for example
`/data/homebrew/PPSA42716`), then register/relaunch the title. The console needs
an already-configured homebrew environment; Nimbus does not provide enablement.

For a quick loop, the boilerplate's FTP deployment can be used from Linux:

```bash
make -C .ps5build/PPSA42716 deploy PS5_HOST=192.168.1.100
```

## 5. CI

`.github/workflows/build.yml` has a `PS5 · native title folder` job that follows
the same steps on Ubuntu and uploads the zip. The `release` job attaches the zip
to a GitHub release. The catalog reads the version from the release tag and the
`contentVersion` in `sce_sys/param.json`; raise both for every release.

## Networking on PS5

The native backend uses `libSceHttp`/`libSceSsl`/`libSceNet` from the console
firmware, not libcurl. The desktop build keeps libcurl. HTTP requests are made on
worker threads; tile decoding happens off the render thread. TLS uses the
console's certificate store (the public HTTPS callback is installed because some
homebrew environments have an incomplete trust store). No credentials are sent.

## Verification status

- **Desktop:** `nimbus_tests` covers the math, JSON, time conversion, weather
  grid and city/settings persistence; the screenshots in `docs/screenshots`
  are rendered by the desktop build.
- **Native build:** completed on 2026-11-23 on x86-64 Ubuntu 26.04 with Clang
  18.1.8, LLD 18, boilerplate commit
  `470695e0c557f99ff2df0e36e4df713c5e636526` and ps5-opengl SDK 1.0.2.
  The generated `eboot.bin` is 23,449,389 bytes, a signed FSELF with 12
  segments and a valid integrity digest. The title artifact
  `PPSA42716-01.000.000.zip` is 17 MB; its SHA-256 is
  `97159d16b6a5e8546b6b1ac0a4f1339002117909c4c3f114fd8d669dd66fb8cc`.
  The same workflow passed on GitHub Actions for tag `01.000.000`
  (run `45100543735`), including the native PS5 build job.
- **Console:** not yet launched on a PS5. The remaining hardware pass is to
  confirm the title starts, the DualSense is detected, tiles and weather load,
  the network fetches succeed, and the system closes the title cleanly. The
  prebuilt ps5-opengl SDK is documented upstream as host-checked, not
  console-validated, so this pass is mandatory before a public listing.
