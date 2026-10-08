# Kunlun Desktop

The native presentation host for Kunlun Engine. CEF/Chromium renders presentation code in a
sandboxed helper; application and tool logic runs behind a versioned, capability-checked boundary
in `kunlun-runtime` (JavaScriptCore) and the standalone DevTools service.

The first implementation is a **macOS CEF engineering preview**: one native window, an embedded
secure-origin presentation bundle, and a versioned host diagnostics/ping channel. CEF is pinned by
version, commit, and archive SHA-256. The portable host policy and build tools run without CEF.

This is not a functioning debugger or a production release. Sandbox qualification, signed
application bundles, runtime/service integration, crash recovery, and the updater remain pending.

## Build and test

Requirements: CMake 3.21+, a C++17 compiler, Python 3.12+, and Node.js 22+ for the presentation tests.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

For the native macOS preview, install Xcode Command Line Tools and use a C++20-capable Apple Clang.
The fetch step downloads approximately 132–139 MB into the ignored `.deps/` directory and verifies
the pinned archive before extraction; it does not discover an installed Chrome.

```sh
python3 tools/fetch_cef.py
cmake -S . -B build-cef -DCMAKE_BUILD_TYPE=Release -DKUNLUN_BUILD_CEF=ON
cmake --build build-cef --parallel 4
ctest --test-dir build-cef --output-on-failure
open "build-cef/bin/Kunlun Desktop.app"
```

The native test opens and closes the actual packaged window. The preview requests the CEF sandbox
and rejects caller-supplied Chromium overrides; successful startup is **not sandbox qualification**.
macOS arm64 has been exercised locally. macOS x64 is pinned but not yet locally qualified;
Linux currently has policy tests only, not a native Desktop executable.

The current Desktop platform scope is **macOS and Linux**. Windows builds and CI are excluded
until the Kunlun Runtime Windows port is complete and Desktop integration is explicitly re-scoped.

## Documents

- [docs/development.md](./docs/development.md) — build layout, tests, limitations, next steps.
- [docs/host-channel.md](./docs/host-channel.md) — the Desktop-owned D1 diagnostics channel.
- [docs/desktop-integration.md](./docs/desktop-integration.md) — Core/runtime build tools and OS capability handoff.
- [ROADMAP.md](./ROADMAP.md) — milestones and workstreams.
- [docs/architecture.md](./docs/architecture.md) — process model, renderer-backend contract, IPC.
- [docs/sandbox-qualification.md](./docs/sandbox-qualification.md) — per-platform sandbox evidence record.
- [docs/update-policy.md](./docs/update-policy.md) — updater signing, security floor, rollback rules.
- [docs/devtools-showcase.md](./docs/devtools-showcase.md) — why DevTools is the first showcase.
- [THIRD_PARTY_NOTICES.md](./THIRD_PARTY_NOTICES.md) — CEF-derived source and distribution notices.

## Related repositories

- `kunlunengine/runtime` — JSC host, capabilities, Inspector endpoint, protocol fixtures.
- `kunlunengine/core` — application/tool logic, builder and runtime API.

Desktop consumes the runtime as a separately contained service or pinned distribution. It must not
make `kunlun-runtime` depend on CEF, GUI toolkits, or Desktop packaging.
