# Sandbox Qualification Record

Status: template — no measured results yet. Every row starts `unqualified`.

Before release, expand the table for every supported OS version/architecture and exact CEF
revision and Chromium version. Record actual state (`enabled`, `disabled`, or `not spawned`)
and evidence for each process, with separate utility service subtypes and GPU hardware/software
modes.

| Platform / CEF revision | Browser | Renderer | GPU | Utility (each subtype) |
| --- | --- | --- | --- | --- |
| Windows / not selected | Unsandboxed broker by design; unmeasured | Unmeasured; sandbox required | Unmeasured | Unmeasured |
| macOS / not selected | Unsandboxed broker by design; unmeasured | Unmeasured; sandbox required | Unmeasured | Unmeasured |
| Linux / not selected | Unsandboxed broker by design; unmeasured | Unmeasured; sandbox required | Unmeasured | Unmeasured |

Each record must include build flags, executable/helper paths and hashes, effective command
lines, `CefSettings.no_sandbox`, `browser_subprocess_path`, sandbox initialization results, and
the `sandbox_info` value's origin and whether it is null (not its raw address).

Production requires `no_sandbox = false` and no sandbox-disabling overrides, including
`--no-sandbox` and `--disable-gpu-sandbox`. A disabled or unverified required sandbox, missing
process evidence, or failed initialization **must fail production qualification**.
