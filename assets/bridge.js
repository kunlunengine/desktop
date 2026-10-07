// Desktop-host introspection only. This is not the runtime app/service protocol.
const PROTOCOL = 'kunlun.desktop.host';
const bytes = new TextEncoder();
const ascii = (value, limit) => typeof value === 'string' &&
  value.length <= limit && /^[\x00-\x7f]*$/.test(value);
const record = value => value !== null && typeof value === 'object' && !Array.isArray(value);
const exactKeys = (value, keys) => record(value) && Object.keys(value).length === keys.length &&
  keys.every(key => Object.hasOwn(value, key));
let nextId = 0n;

export class BridgeError extends Error {
  constructor(code, message) {
    super(message);
    this.name = 'BridgeError';
    this.code = code;
  }
}

export function createBridge({
  host = globalThis,
  timeoutMs = 5000,
  setTimer = setTimeout,
  clearTimer = clearTimeout,
} = {}) {
  const pending = new Set();
  if (!Number.isFinite(timeoutMs) || timeoutMs <= 0) throw new RangeError('Invalid timeout');

  function call(method, params) {
    return new Promise((resolve, reject) => {
      const fail = (code, message) => reject(new BridgeError(code, message));
      if (typeof host.kunlunQuery !== 'function' || typeof host.kunlunQueryCancel !== 'function') {
        fail('unavailable', 'Desktop host bridge unavailable. Open this screen in the D1 native shell.');
        return;
      }
      if (pending.size >= 16) {
        fail('busy', 'Host request limit reached. Wait for an active request to finish.');
        return;
      }
      const id = `d1-${(++nextId).toString(36)}`;
      const request = JSON.stringify({ protocol: PROTOCOL, version: 1, id, method, params });
      if (!ascii(id, 64) || bytes.encode(request).length > 4096) {
        fail('request', 'Host request exceeds its size limit.');
        return;
      }
      const entry = {};
      pending.add(entry);
      let settled = false;
      let queryId;
      let cancelOnReturn = false;
      let timer;
      const cancel = () => {
        try { host.kunlunQueryCancel(queryId); } catch { /* Rejection still completes locally. */ }
      };
      const finish = (error, result) => {
        if (settled) return;
        settled = true;
        pending.delete(entry);
        clearTimer(timer);
        if (error) reject(error);
        else resolve(result);
      };
      timer = setTimer(() => {
        if (settled) return;
        finish(new BridgeError('timeout', 'Host request timed out. You can retry.'));
        if (queryId !== undefined) cancel();
        else cancelOnReturn = true;
      }, timeoutMs);
      try {
        queryId = host.kunlunQuery({
          request,
          persistent: false,
          onSuccess(response) {
            if (settled) return;
            try {
              if (typeof response !== 'string' || bytes.encode(response).length > 16384) {
                throw new Error('Response exceeds size limit or is not text');
              }
              const envelope = JSON.parse(response);
              if (!exactKeys(envelope, ['protocol', 'version', 'id', 'result']) ||
                  envelope.protocol !== PROTOCOL ||
                  envelope.version !== 1 || envelope.id !== id || !record(envelope.result)) {
                throw new Error('Invalid response envelope, version, or request ID');
              }
              const r = envelope.result;
              if (method === 'host.ping') {
                if (!exactKeys(r, ['token', 'sequence']) || r.token !== params.token ||
                    !Number.isSafeInteger(r.sequence) || r.sequence <= 0) {
                  throw new Error('Invalid ping response');
                }
              } else if (!exactKeys(r, ['name', 'version', 'backend', 'cefVersion',
                  'chromiumVersion', 'sandboxRequested', 'sandboxQualified', 'runtimeConnected']) ||
                  r.name !== 'Kunlun Desktop' || r.version !== '0.1.0-dev' ||
                  r.backend !== 'cef' || typeof r.cefVersion !== 'string' ||
                  typeof r.chromiumVersion !== 'string' || r.sandboxRequested !== true ||
                  r.sandboxQualified !== false || r.runtimeConnected !== false) {
                throw new Error('Invalid D1 host diagnostics');
              }
              finish(null, r);
            } catch (error) {
              finish(new BridgeError('response', `Host response rejected: ${error.message}`));
            }
          },
          onFailure(code, message) {
            finish(new BridgeError('host', `Host rejected request (${String(code)}): ${String(message).slice(0, 240)}`));
          },
        });
        if (cancelOnReturn) cancel();
      } catch {
        finish(new BridgeError('transport', 'Host bridge could not send the request. You can retry.'));
      }
    });
  }

  return Object.freeze({
    describe: () => call('host.describe', {}),
    ping(token) {
      if (!ascii(token, 128) || !/^[\x20-\x7e]*$/.test(token)) {
        return Promise.reject(new BridgeError('request', 'Ping token must be printable ASCII and at most 128 characters.'));
      }
      return call('host.ping', { token });
    },
  });
}
