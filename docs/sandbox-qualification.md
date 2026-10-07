# Sandbox Qualification Record

Status: unqualified — no completed runtime restriction/probe evidence. The D1 macOS arm64 preview
builds and runs its native UI/IPC smoke with sandboxing requested; this is not qualification.

Before release, expand the table for every supported OS version/architecture and exact CEF
revision and Chromium version. Record actual state (`enabled`, `disabled`, or `not spawned`)
and evidence for each process, with separate utility service subtypes and GPU hardware/software
modes.

| Platform / CEF revision | Browser | Renderer | GPU | Utility (each subtype) |
| --- | --- | --- | --- | --- |
| Windows / not selected | Unsandboxed broker by design; unmeasured | Unmeasured; sandbox required | Unmeasured | Unmeasured |
| macOS arm64/x64 / CEF 154.0.34, `g14c5a08` | Unsandboxed broker by design; unqualified | Unmeasured; sandbox required | Unmeasured | Unmeasured |
| Linux / not selected | Unsandboxed broker by design; unmeasured | Unmeasured; sandbox required | Unmeasured | Unmeasured |

Each record must include build flags, executable/helper paths and hashes, effective command
lines, `CefSettings.no_sandbox`, `browser_subprocess_path`, sandbox initialization results, and
the `sandbox_info` value's origin and whether it is null (not its raw address).

## D1 engineering-preview configuration

The exact macOS arm64/x64 artifacts are pinned in `../cef/pin.json`:
CEF `154.0.34+g14c5a08+chromium-154.0.8037.98`,
commit `14c5a089e8452874cb1a556dc3fde26ed2d87723`.
The macOS arm64 native smoke has been exercised locally; x64 and other platforms are not measured.

- Browser: privileged broker by design; `no_sandbox=false`, no `browser_subprocess_path`,
  `sandbox_info=null` (Windows-only), no caller-provided Chromium overrides.
- Helpers: all upstream helper variants are packaged. Each calls
  `CefScopedSandboxContext::Initialize` before framework loading, terminating on failure;
  the matching bundled `libcef_sandbox.dylib` is present.
- Build: requires `USE_SANDBOX=ON` and `CEF_USE_SANDBOX`. Framework is dynamically loaded
  in the browser and helpers. No production signing/entitlement workflow exists.
- Evidence still missing: per-process effective sandbox profiles, denied-operation probes,
  process command lines/hashes, GPU hardware/software modes, utility subtypes, and the signed
  packaged release on every supported OS/architecture.

Do not promote any process to `enabled` based on these settings or successful smoke alone.
Keep the matrix unqualified until the complete evidence is recorded.

Production requires `no_sandbox = false` and no sandbox-disabling overrides, including
`--no-sandbox` and `--disable-gpu-sandbox`. A disabled or unverified required sandbox, missing
process evidence, or failed initialization **must fail production qualification**.
