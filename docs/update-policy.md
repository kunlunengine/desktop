# Update and Rollback Acceptance Policy

Extracted from the accepted architecture decision. The future Desktop updater and launcher must
implement these rules; required security acceptance tests are listed below.

## Rules

- Release security metadata must specify a **minimum safe CEF version** per platform and channel,
  its Chromium security baseline, and approved exact CEF revisions and artifact hashes. Compare
  parsed numeric versions, not version strings or commit hashes. A valid manifest signature
  proves authenticity, not current safety: reject any manifest below this floor.
- The updater must authenticate security metadata independently of the candidate bundle and
  retain the highest accepted policy/revocation sequences and security floor outside the
  installation being rolled back. Neither an old signed manifest nor rollback authorization may
  lower that floor.
- Before activation and on startup: verify manifest signatures, approved CEF revisions, artifact
  hashes, platform/channel and version compatibility, and the current signed revocation list.
  Missing, expired, or invalid metadata or a sequence below the retained value fails closed.
- A downgrade additionally requires an explicit, signed rollback authorization from a trusted
  rollback-authority role. Validate authority and its revocation status, authorization expiry and
  replay protection, and bindings to the failed source and target manifest digests,
  platform/channel, and failure condition.
- If no safe authorized target exists, fail closed and require a safe recovery update instead of
  starting vulnerable CEF.

## Required security acceptance tests

Use signed fixtures and otherwise valid metadata so negative cases reach the intended check.
Let `F` be the installed security floor and `A` the failed release; candidate `B` is an older
compatible release.

| Case | Expected result |
| --- | --- |
| Validly signed update manifest with CEF below `F` | Reject before activation, despite a valid signature. |
| `B` has CEF below `F` and a valid rollback authorization | Reject; authorization cannot bypass the floor. |
| Failure of `A`; `B` has CEF equal to `F` or above it, no revocations, and valid scoped rollback authorization | Accept and atomically restore the complete Desktop/CEF bundle; startup succeeds and retains `F` and the policy/revocation sequences. |
| Safe `B`, but authorization missing, invalidly signed, expired, replayed, or bound to another source, target, platform/channel, or failure | Reject each variant; an old signed manifest alone never permits downgrade. |
| Otherwise valid update/rollback with a revoked revision, artifact, manifest, signing key, or authorization | Reject each revocation variant, including revocation after staging but before activation. |
| Missing, invalid, expired security/revocation metadata or a sequence below the retained value; attempt to lower the retained floor | Reject each variant; rollback and restart never restore older security policy. |
| Required subprocess sandbox disabled, initialization fails, or runtime evidence disagrees with configuration | Fail qualification on each platform. |
| All required subprocess probes pass with the documented privileged browser broker | Pass the sandbox gate without claiming that the browser itself is sandboxed. |
