# Kunlun Desktop

The native presentation host for Kunlun Engine. CEF/Chromium renders presentation code in a
sandboxed helper; application and tool logic runs behind a versioned, capability-checked boundary
in `kunlun-runtime` (JavaScriptCore) and the standalone DevTools service.

This repository is empty of implementation. It currently pins the accepted architecture,
qualification gates, and update/rollback policy; code lands milestone by milestone.

## Documents

- [ROADMAP.md](./ROADMAP.md) — milestones and workstreams.
- [docs/architecture.md](./docs/architecture.md) — process model, renderer-backend contract, IPC.
- [docs/sandbox-qualification.md](./docs/sandbox-qualification.md) — per-platform sandbox evidence record.
- [docs/update-policy.md](./docs/update-policy.md) — updater signing, security floor, rollback rules.
- [docs/devtools-showcase.md](./docs/devtools-showcase.md) — why DevTools is the first showcase.

## Related repositories

- `kunlunengine/runtime` — JSC host, capabilities, Inspector endpoint, protocol fixtures.
- `kunlunengine/core` — application/tool logic, builder and runtime API.

Desktop consumes the runtime as a separately contained service or pinned distribution. It must not
make `kunlun-runtime` depend on CEF, GUI toolkits, or Desktop packaging.
