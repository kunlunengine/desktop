# DevTools Showcase

The first Kunlun Desktop showcase is the standalone DevTools debugger. It exercises native windows
and menus, high-volume streaming data, local IPC, crash recovery, profiling, accessibility, secure
capability brokering, and self-inspection. It should prove the Kunlun host and application model,
not prove that the project can maintain a browser engine before it can ship a debugger.

Roles are kept honest:

- Chromium/V8 executes less-privileged presentation code inside the renderer sandbox.
- Kunlun/JSC executes application and tool logic in a separately contained service process.
- The DevTools core joins Chromium/CDP, Kunlun/JSC/WIP, and later native targets without
  pretending they share an engine or wire protocol.

See [architecture.md](./architecture.md) and the runtime repository's `docs/devtools.md`.
