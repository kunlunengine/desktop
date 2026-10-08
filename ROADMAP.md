# Kunlun Desktop Roadmap

Status date: 2026-10-07

Implementation has started with a macOS CEF engineering preview. The roadmap follows the accepted
architecture decision; the runtime repository owns milestones M0–M3 for the JSC host.

## Platform scope

Desktop work currently targets **macOS and Linux**. Windows is excluded from implementation,
CI, packaging plans, and qualification gates because Kunlun Runtime has not been ported to
Windows. Completing that Runtime port is a prerequisite, not automatic Desktop support:
Windows must be explicitly re-scoped and separately integrated and qualified before returning.

## Product decisions

1. **CEF/Chromium renders; Kunlun owns the host.** A pinned CEF distribution is the sole
   reference renderer. It is presentation only — never the application runtime, never Electron's
   Node-in-the-renderer architecture.
2. **Application logic stays in `kunlun-runtime`.** The renderer receives typed commands and
   events across a versioned, capability-checked boundary. It gets no ambient filesystem,
   subprocess, socket, secret, or native-addon access.
3. **Renderer sandboxing is required; the browser broker is privileged by design.** Production
   requires `no_sandbox = false` and a completed per-platform qualification record.
4. **DevTools is the first showcase.** It proves the host and application model end to end.
5. **Updates are atomic and fail closed.** Desktop and CEF update together; signed manifests,
   revocation lists, a retained security floor, and authorized rollback only.

## Milestones

- **D0 — Repository bootstrap**: architecture, sandbox qualification template, update policy,
  license, CMake policy tests, dependency-free presentation tests, and CI. **Implemented.**
- **D1 — CEF pin and renderer adapter spike**: minimal browser process, one window, loads a
  local secure-origin bundle, versioned bidirectional IPC channel. **In progress:** exact
  CEF/Chromium pin and verified archive fetch; macOS bundle/helpers; embedded development assets;
  browser-authorized diagnostics/ping; native smoke and rejection tests. macOS arm64 startup has
  been exercised. Linux native startup, signed application bundles, and the general
  renderer-backend contract are not implemented.
- **D2 — Host contract**: profiles/windows/views lifecycle, navigation and permission policy,
  renderer-crash recovery, capability-checked service IPC routing and audit. **Pending.**
- **D3 — DevTools showcase shell**: windows/menus lifecycle, DevTools service process
  integration, self-inspection endpoint in development builds. **Pending:** the D1 diagnostics
  screen is not a debugger and exposes no application or Inspector API.
- **D4 — Qualification**: sandbox matrix measured on all supported platforms, update/rollback
  security tests against the packaged build, accessibility and interaction gates. **Pending.**

## Cross-repository dependencies

D2/D3 require a separate Core/runtime/Desktop work package for the versioned build asset manifest,
real service execution/launch and ABI negotiation, shared capability command/event fixtures, and
native packaging/doctor integration. Existing `client/server/worker` output and runtime provider
diagnostics do not establish a desktop build target or running application. See
[docs/desktop-integration.md](./docs/desktop-integration.md) for ownership and admission gates.
