# Security Policy

Kunlun Desktop hosts a pinned Chromium presentation engine and brokers typed commands between it
and `kunlun-runtime`. Capability enforcement, navigation policy, IPC authorization, sandbox
configuration, updater signature verification, and rollback authorization are all
security-sensitive boundaries.

## Supported Versions

Kunlun Desktop is currently an early engineering preview with no release. Security fixes will be
made only on the latest `main` branch.

| Version | Supported |
| --- | --- |
| Latest `main` | Yes, on a best-effort basis |
| Older commits and preview artifacts | No |

## Reporting a Vulnerability

Please open a private security advisory on GitHub rather than a public issue.
