# Kunlun Desktop Roadmap

Status date: 2026-10-06

This repository is pre-implementation. The roadmap mirrors the accepted architecture decision;
the runtime repository owns milestones M0–M3 for the JSC host.

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

## Milestones (TBD — no code yet)

- **D0 — Repository bootstrap**: architecture, sandbox qualification template, update policy,
  license/CI skeleton. *(this revision)*
- **D1 — CEF pin and renderer adapter spike**: minimal browser process, one window, loads a
  local secure-origin bundle, versioned bidirectional IPC channel.
- **D2 — Host contract**: profiles/windows/views lifecycle, navigation and permission policy,
  renderer-crash recovery, capability-checked IPC routing and audit.
- **D3 — DevTools showcase shell**: windows/menus lifecycle, DevTools service process
  integration, self-inspection endpoint in development builds.
- **D4 — Qualification**: sandbox matrix measured on all supported platforms, update/rollback
  security tests against the packaged build, accessibility and interaction gates.
