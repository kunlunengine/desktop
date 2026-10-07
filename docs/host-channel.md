# D1 Desktop Host Channel

This is a **Desktop-owned diagnostics channel**, not the runtime-owned application/provider or
DevTools protocol. It exposes no filesystem, process, socket, secret, evaluation, or service API.
It proves the native presentation boundary while those services remain disconnected.

## Sender and transport

CEF's message router provides `window.kunlunQuery` and `window.kunlunQueryCancel`. The renderer
adapter binds them only in the bundled main-frame document at `kunlun://desktop/index.html`
(or the origin root). The browser independently validates the actual CEF browser and main frame
for every query. Renderer-provided application/window/session/capability fields are not trusted;
indeed, extra envelope fields are rejected. Future service routing must consume the runtime
contract and authorize its capabilities, not extend diagnostics into a generic `invoke` API.

Only canonical embedded asset-table entries can load. Main-frame navigation is limited to the
bundled document; subframes, arbitrary URLs, popups, downloads, permissions, and OS protocol
execution are denied. Headers enforce a restrictive CSP with no remote or inline code.

## Request and response

Each request is a JSON object with exactly these five fields:

```json
{
  "protocol": "kunlun.desktop.host",
  "version": 1,
  "id": "d1-1",
  "method": "host.ping",
  "params": { "token": "hello" }
}
```

- `protocol` must match exactly; `version` must be the integer `1`.
- `id` is 1–64 characters from `[A-Za-z0-9_-]`.
- `host.describe` takes exactly `{}` and reports the Desktop/CEF/Chromium versions, requested
  sandbox configuration, unqualified status, and disconnected runtime.
- `host.ping` takes exactly one `token`: 0–128 printable ASCII characters. It returns that token
  and a positive per-browser sequence number. Sequence exhaustion rejects further pings.
- Unknown methods, fields, types, versions, and parameters fail closed.

Successful replies retain the protocol/version/request ID:

```json
{
  "protocol": "kunlun.desktop.host",
  "version": 1,
  "id": "d1-1",
  "result": { "token": "hello", "sequence": 1 }
}
```

Failures use the router's `onFailure(code, message)` callback, not a JSON success envelope.
Codes: `400` invalid/unsupported request, `403` untrusted sender, `413` size limit,
`429` sequence exhausted. Saturated admission returns the router's immediate `-1` cancellation,
without registering another pending query; transport/binary queries may also receive that
rejection. The JavaScript bridge rejects invalid response versions, IDs, and result shapes.

## Bounds and lifecycle

- Request: at most 4,096 UTF-8 bytes; checked again by the browser before JSON parsing.
- Response: at most 16,384 UTF-8 bytes; checked before sending and after receipt.
- Client: at most 16 active calls, five-second deadline per call, cancellation on timeout,
  no persistent subscriptions, late callbacks ignored.
- Browser: admits at most 16 pending queries across the router, including error replies. Request
  validation and diagnostics compute synchronously; CEF retains admitted callbacks until its posted
  completion task runs. Saturated requests do not enter that registry. There is no service queue.
- Navigation, renderer termination, context release, and browser close are forwarded to CEF's
  router cancellation lifecycle.
- Activity displayed by the UI is bounded to eight entries.

These are application-level bounds, not a claim that all Chromium transport allocations are
bounded by this protocol. D2 service forwarding and large streams require explicit admission,
flow control, backpressure, and audit.

`sandboxRequested=true` means only that this preview requests sandboxing; it is not measured
sandbox evidence. `sandboxQualified=false` and `runtimeConnected=false` are intentional D1 facts.
