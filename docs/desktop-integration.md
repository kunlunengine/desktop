# Desktop Build Tools and OS Integration

Status: D2/D3 coordination plan, not an implemented application ABI or CLI target.

The Core/runtime coordination thread confirmed that the existing BuildEngine promises selected
`client`, `server`, and `worker` build outputs only. It does not yet promise a CEF application
bundle, signing, or service launch. Desktop integration must be a separate cross-repository work
package, not a new label on those existing outputs.

The current Desktop integration scope is macOS and Linux. Windows implementation, native
tooling, and packaging/distribution work are deferred until Kunlun Runtime is ported to Windows
and the Desktop target is explicitly re-scoped.

## Ownership

| Owner | Responsibility | Not its responsibility |
| --- | --- | --- |
| Core / CLI Build Tools | Compile the presentation and application/service graphs; produce a versioned asset manifest; coordinate build stages and diagnostics. | Claim native installation, sandbox, signing, or execution readiness from a successful JS build. |
| Runtime | Own application/profile/ABI and execution/service-launch negotiation; expose restricted file/process/socket/credential capabilities through a separately contained service. | Depend on CEF/GUI packaging, or treat `version`/`doctor`/`check-artifact` as application execution or Inspector transport. |
| Desktop | Validate consumed artifacts and pins; host CEF; broker native windows/views/menus/dialogs/clipboard/navigation/lifecycle with explicit authorization. | Invent application/debugger semantics, inject privileged native objects into the renderer, or duplicate runtime-owned schemas. |

The D1 `host.describe`/`host.ping` channel remains separate. No application or native OS command
is added to that diagnostics protocol as a shortcut to integration.

## Three admission gates

### 1. Core build artifact contract

Core should provide the authoritative source types/schema and conformance fixtures for a
versioned presentation/application/service artifact manifest, including relative asset paths,
content digests, entry points, and source-map associations. Decide supported targets and
application/profile metadata with Runtime before defining a desktop build target.

Desktop then pins an immutable contract/fixture revision and verifies the consumed artifact.
Tests must cover unsupported versions, missing/modified assets, invalid path mappings, duplicate
entries, and incompatible target/profile pairings. Digests alone do not authenticate the publisher:
signed bundle admission remains a separate Desktop release gate.

### 2. Runtime execution and launch contract

Runtime must expose a real execution/service-launch boundary with application/profile/ABI
negotiation and an honest capability report. Schema identity, runtime distribution, and fixtures
must be pinned together. Unknown ABI/profile/version or unavailable service execution fails closed.

The existing runtime manifest/provider contracts and draft DevTools fixtures are useful evidence,
not a substitute implementation. A successful provider `doctor` or artifact check must not make
Desktop display a running application, debugger target, or connected Inspector.

### 3. Desktop broker contract

Desktop should consume shared command/event types and fixtures, binding authorization to the
actual application/window/session identity. Establish cancellation, deadlines, flow control,
capability revocation, error behavior, audit, and process/window shutdown before service routing.

The first native UI scope is window/view/menu/dialog/clipboard/navigation/lifecycle. A file picker
is native UI, not permission to give the renderer unrestricted filesystem access. File contents,
subprocesses, sockets, and credentials remain behind restricted Runtime service capabilities.
Additional OS APIs—such as notifications, tray integration, shortcuts, and platform-specific
facilities—need an explicit capability inventory and supported-platform scope before implementation.
This list is planning scope, not a report of currently available APIs.

## Native tooling is a distinct pipeline

The cross-platform CLI must coordinate and diagnose native stages separately:

- Pin/fetch/verify CEF and discover the required compiler/SDK without substituting installed Chrome.
- macOS: C++/Objective-C++, bundle and helper layout, entitlements, signing, and notarization.
- Linux: native packaging and qualified namespace/seccomp/helper requirements for the chosen
  distribution, architecture, and feature profile.
- Carry resource manifests, license notices, service distributions, and protocol pins in the
  installation; Desktop/CEF activation and rollback follow the retained security-floor policy.
- Separate developer `doctor`/build diagnostics from measured sandbox and production release gates.

Desktop's present CMake preview and SDK fetch tool are not yet this general CLI integration.
No installer, updater, signing identity, application-execution service, or new CLI target is
claimed here.

## Handoff needed before implementation

Core and Runtime should return:

1. Authoritative schema/source-type locations, immutable revisions, and shared positive/negative
   fixtures for the asset and application/launch contracts.
2. An explicit execution/service transport availability statement and supported profile/ABI.
3. The first OS capability scope, authority/lifetime model, and owner of each shared command/event.

Desktop can then implement adapters and conformance tests against those sources rather than
maintaining a second application ABI. See [development.md](./development.md),
[architecture.md](./architecture.md), and [update-policy.md](./update-policy.md).
