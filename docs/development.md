# Developing the D1 Preview

## Scope and implementation choice

Use C++ directly against the pinned CEF SDK, with Objective-C++ only for macOS application
startup. This follows CEF's supported helper, framework-loading, and window lifecycle patterns
without introducing a second GUI toolkit or an additional language-binding layer. The host policy
library is renderer-independent; the CEF adapter is currently a deliberately small, single-window
spike, not the complete D2 backend interface.

The current platform scope is macOS and Linux. Windows builds are rejected before compiler
discovery; Windows work is deferred until the Kunlun Runtime port is complete and Desktop
integration is explicitly re-scoped. Linux currently runs portable tests only.

Presentation assets use HTML/CSS and JavaScript modules with no npm dependencies or bundler.
They render host diagnostics and verify a ping. Application semantics, debugger targets, JSC
execution, and service protocols are not implemented in the renderer or replaced with mocks.

## Source map

| Path | Responsibility |
| --- | --- |
| `src/host/` | Portable URL, launcher, and typed host-request policy; no CEF dependency. |
| `src/cef/` | Browser/helper startup, CEF Views window, resource handler and diagnostics router. |
| `assets/` | Accessible engineering-preview screen and bounded asynchronous bridge client. |
| `cef/pin.json` | Exact CEF version/commit, Chromium version, per-architecture archive hashes/sizes. |
| `tools/fetch_cef.py` | Workspace-only archive download, checksum verification, safe extraction. |
| `tools/embed_assets.py` | Fixed asset table compiled into the executable; no runtime file mapping. |
| `tools/check_native.py` | Packaged launcher rejection tests and real CEF smoke run. |
| `tests/` | Portable policy, Python build-tool, and Node bridge tests. |

## Portable build

Requires CMake 3.21+, a C++17 compiler, Python 3.12+, and Node.js 22+:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

This does not download CEF and does not build a GUI. CTest warns and omits the bridge suite if Node
is missing; CI explicitly installs Node and runs all suites. Python 3.12 is required for tarfile's
safe data extraction filter.

## macOS native build

Install Xcode Command Line Tools (`xcode-select --install`) if needed. CEF requires C++20.
Only one architecture per build is accepted: `arm64` or `x86_64`, with its matching pinned SDK.
The SDK's macOS 12 deployment baseline is not a claim of qualification on every macOS release.

```sh
python3 tools/fetch_cef.py
cmake -S . -B build-cef -DCMAKE_BUILD_TYPE=Release -DKUNLUN_BUILD_CEF=ON
cmake --build build-cef --parallel 4
ctest --test-dir build-cef --output-on-failure
open "build-cef/bin/Kunlun Desktop.app"
```

The fetch tool defaults to the native macOS architecture. For cross-compilation, fetch the matching
artifact with `--platform macosx64` or `--platform macosarm64`, then configure
`-DCMAKE_OSX_ARCHITECTURES=x86_64` or `arm64`. CMake automatically finds the pinned SDK under
`.deps/cef/`; an explicit `-DCEF_ROOT=/path/to/sdk` is also accepted, but must match the exact
header version and framework architecture. Browser and helpers verify the loaded framework's full
version before starting CEF. These checks do not authenticate a packaged installation.

The approximately 132–139 MB archive is verified with the committed SHA-256 and exact size before
extraction. A matching extraction receipt permits reuse of the SDK directory on subsequent runs;
the receipt is a developer cache, not a tamper-resistant installation manifest. Use a fresh
workspace if an extracted SDK is untrusted. The binary distribution includes CEF and Chromium
license notices, copied into the preview's `Contents/Resources/licenses/`.

Build the default target or `kunlun_desktop_bundle`, not just the browser linker target, to include
all helpers and notices. The resulting layout is:

```text
build-cef/bin/Kunlun Desktop.app/Contents/
  MacOS/Kunlun Desktop
  Frameworks/
    Chromium Embedded Framework.framework/
    Kunlun Desktop Helper.app/
    Kunlun Desktop Helper (Alerts).app/
    Kunlun Desktop Helper (GPU).app/
    Kunlun Desktop Helper (Plugin).app/
    Kunlun Desktop Helper (Renderer).app/
  Resources/
    licenses/
    pin.json
```

The official CEF framework-copy macro creates its versioned framework layout. The helper starts
`CefScopedSandboxContext` before dynamically loading the framework. `USE_SANDBOX=OFF` is rejected
at configuration time; helper/browser builds also require `CEF_USE_SANDBOX`. The browser sets
`no_sandbox=false`, supplies no helper override, and correctly passes null Windows-only
`sandbox_info` on macOS.

The build refreshes the generated framework before browser relinking so repeated copies cannot
accumulate broken directory symlinks. Close the preview before rebuilding; this development
packaging step is not an atomic installation/update mechanism.

This is a **build-tree preview**, not an installer. Its profile lives beside the bundle under
`build-cef/bin/.kunlun-preview/`; smoke runs use distinct subdirectories. Do not install it into
`/Applications` or distribute it as a signed/release-qualified product. Production bundle signing,
entitlements, update security metadata, retained security floor, and runtime sandbox evidence are
not implemented.

## Native verification

`ctest --test-dir build-cef --output-on-failure` runs all portable suites plus `native_host`.
Alternatively:

```sh
python3 tools/check_native.py \
  "build-cef/bin/Kunlun Desktop.app/Contents/MacOS/Kunlun Desktop"
```

The native check first validates framework links, sandbox library, helpers, notices, and pin
metadata. It rejects seven dangerous launcher overrides before framework loading. It then
opens the actual CEF window with `--smoke-test`, validates the secure custom origin, loads the real
modules/stylesheet, negotiates the Desktop host channel, exercises diagnostics and ping through
both the bridge and UI, rejects malformed/unsupported/oversized/persistent messages, and verifies
native admission under valid and invalid request bursts, with no more than 16 retained router
queries. It also verifies that popups and inline scripts are blocked. An expected CSP violation is
printed for the inline script negative test. The window closes through the CEF lifecycle; the
harness requires a PASS marker **and exit code zero**, so a teardown failure is not success.

No remote debugging port or user-supplied script is accepted. The fixed native smoke script is
host-injected test code, not a renderer-facing evaluation API. A 15-second in-process deadline and
an outer harness timeout make a stuck smoke fail.

GitHub CI runs portable tests on Linux/macOS and a packaged CEF lane on the native
architecture of `macos-latest`. It caches only the upstream archive, revalidating it before every
extraction. CI is configured here; a local successful run does not imply GitHub CI has already run
or qualify other operating systems/architectures.

## Next boundaries

- Consume runtime-owned protocol schemas/fixtures at an immutable revision when service transport
  is connected. Runtime currently provides manifest/provider contracts and draft DevTools fixtures,
  but the documented provider CLI is not a general application or Inspector RPC daemon.
- Keep the D1 host diagnostics protocol distinct from that boundary. Do not add `app.eval` or
  invent debugger/application semantics here.
- Implement the D2 profile/window/view contract, renderer/service crash lifecycle, capability
  authorization, audit, and bounded streaming before connecting privileged services.
- Add Linux native startup and qualified namespace/seccomp setup; Linux has no native host
  in this revision.
- Complete signed application-bundle admission, installer/update/rollback tests, the sandbox
  evidence matrix, and accessibility/input qualification before any production release.

See [desktop-integration.md](./desktop-integration.md) for the coordinated build-tools/OS API
handoff, [host-channel.md](./host-channel.md), [sandbox-qualification.md](./sandbox-qualification.md),
and [ROADMAP.md](../ROADMAP.md). Upstream startup references:
[macOS sample](https://github.com/chromiumembedded/cef/blob/14c5a089e8452874cb1a556dc3fde26ed2d87723/tests/cefsimple/cefsimple_mac.mm),
[helper sample](https://github.com/chromiumembedded/cef/blob/14c5a089e8452874cb1a556dc3fde26ed2d87723/tests/cefsimple/process_helper_mac.cc),
[sandbox setup](https://github.com/chromiumembedded/cef/blob/14c5a089e8452874cb1a556dc3fde26ed2d87723/docs/sandbox_setup.md).
