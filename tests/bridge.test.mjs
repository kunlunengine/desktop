import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

const source = await readFile(new URL('../assets/bridge.js', import.meta.url), 'utf8');
const { createBridge } = await import(`data:text/javascript;base64,${Buffer.from(source).toString('base64')}`);
const diagnostics = {
  name: 'Kunlun Desktop', version: '0.1.0-dev', backend: 'cef',
  cefVersion: 'test-cef', chromiumVersion: 'test-chromium',
  sandboxRequested: true, sandboxQualified: false, runtimeConnected: false,
};
function fixture() {
  const calls = [];
  const canceled = [];
  const timers = new Set();
  const host = {
    kunlunQuery(options) { calls.push(options); return calls.length; },
    kunlunQueryCancel(id) { canceled.push(id); },
  };
  const bridge = createBridge({
    host,
    setTimer(fn) { timers.add(fn); return fn; },
    clearTimer(fn) { timers.delete(fn); },
  });
  const reply = (index, result, overrides = {}) => {
    const call = calls[index];
    const request = JSON.parse(call.request);
    call.onSuccess(JSON.stringify({
      protocol: request.protocol, version: 1, id: request.id, result, ...overrides,
    }));
  };
  return { bridge, host, calls, canceled, timers, reply };
}

test('describe and ping use the host contract and unique IDs', async () => {
  const f = fixture();
  const describe = f.bridge.describe();
  assert.equal(f.calls[0].persistent, false);
  assert.deepEqual(JSON.parse(f.calls[0].request).params, {});
  f.reply(0, diagnostics);
  assert.deepEqual(await describe, diagnostics);
  const ping = f.bridge.ping('smoke');
  f.reply(1, { token: 'smoke', sequence: 1 });
  assert.deepEqual(await ping, { token: 'smoke', sequence: 1 });
  assert.notEqual(JSON.parse(f.calls[0].request).id, JSON.parse(f.calls[1].request).id);
  assert.equal(f.timers.size, 0);
});

test('missing query or cancellation bridge is unavailable', async () => {
  for (const host of [{}, { kunlunQuery() {} }]) {
    await assert.rejects(createBridge({ host }).describe(), { code: 'unavailable' });
  }
});

test('version, ID, and protocol mismatches fail closed', async () => {
  for (const overrides of [{ version: 2 }, { id: 'other' }, { protocol: 'runtime' }]) {
    const f = fixture();
    const promise = f.bridge.describe();
    f.reply(0, diagnostics, overrides);
    await assert.rejects(promise, { code: 'response' });
    assert.equal(f.timers.size, 0);
  }
});

test('pending calls are bounded and slots are released', async () => {
  const f = fixture();
  const promises = Array.from({ length: 16 }, () => f.bridge.describe());
  await assert.rejects(f.bridge.describe(), { code: 'busy' });
  assert.equal(f.calls.length, 16);
  f.reply(0, diagnostics);
  await promises[0];
  const replacement = f.bridge.describe();
  for (let i = 1; i <= 16; i++) f.reply(i, diagnostics);
  await Promise.all([...promises, replacement]);
});

test('timeout cancels query, ignores late callbacks, and releases slot', async () => {
  const f = fixture();
  const promise = f.bridge.describe();
  [...f.timers][0]();
  await assert.rejects(promise, { code: 'timeout' });
  assert.deepEqual(f.canceled, [1]);
  f.reply(0, diagnostics);
  f.calls[0].onFailure(1, 'late failure');
  const retry = f.bridge.describe();
  f.reply(1, diagnostics);
  await retry;
});

test('synchronous success and failure callbacks do not leak timers', async () => {
  for (const success of [true, false]) {
    const f = fixture();
    f.host.kunlunQuery = options => {
      const request = JSON.parse(options.request);
      if (success) options.onSuccess(JSON.stringify({
        protocol: request.protocol, version: request.version, id: request.id, result: diagnostics,
      }));
      else options.onFailure(403, 'Denied');
      return 12;
    };
    if (success) assert.deepEqual(await f.bridge.describe(), diagnostics);
    else await assert.rejects(f.bridge.describe(), { code: 'host' });
    assert.equal(f.timers.size, 0);
  }
});

test('malformed, oversized, and invalid method results are rejected', async () => {
  for (const response of ['{', 'null', '[]', 'x'.repeat(16385), 42]) {
    const f = fixture();
    const promise = f.bridge.describe();
    f.calls[0].onSuccess(response);
    await assert.rejects(promise, { code: 'response' });
  }
  for (const result of [{}, { ...diagnostics, sandboxQualified: true }]) {
    const f = fixture();
    const promise = f.bridge.describe();
    f.reply(0, result);
    await assert.rejects(promise, { code: 'response' });
  }
  for (const result of [
    { token: 'wrong', sequence: 1 }, { token: 'ok', sequence: 0 },
    { token: 'ok', sequence: 1.5 },
  ]) {
    const f = fixture();
    const promise = f.bridge.ping('ok');
    f.reply(0, result);
    await assert.rejects(promise, { code: 'response' });
  }
});

test('extra response envelope and method result fields fail closed', async () => {
  const envelope = fixture();
  const describe = envelope.bridge.describe();
  envelope.reply(0, diagnostics, { capability: 'unexpected' });
  await assert.rejects(describe, { code: 'response' });

  const extraDiagnostics = fixture();
  const diagnosticsCall = extraDiagnostics.bridge.describe();
  extraDiagnostics.reply(0, { ...diagnostics, service: 'unexpected' });
  await assert.rejects(diagnosticsCall, { code: 'response' });

  const extraPing = fixture();
  const ping = extraPing.bridge.ping('ok');
  extraPing.reply(0, { token: 'ok', sequence: 1, service: 'unexpected' });
  await assert.rejects(ping, { code: 'response' });
});

test('invalid tokens never reach host; UTF-8 response limit is measured in bytes', async () => {
  const f = fixture();
  for (const token of ['é', '\n', '\x7f', 'x'.repeat(129), null]) {
    await assert.rejects(f.bridge.ping(token), { code: 'request' });
  }
  assert.equal(f.calls.length, 0);
  const promise = f.bridge.describe();
  f.reply(0, { ...diagnostics, cefVersion: '界'.repeat(6000) });
  await assert.rejects(promise, { code: 'response' });
});

test('transport throws release pending requests', async () => {
  const f = fixture();
  f.host.kunlunQuery = () => { throw new Error('transport'); };
  for (let i = 0; i < 17; i++) {
    await assert.rejects(f.bridge.describe(), { code: 'transport' });
  }
  assert.equal(f.timers.size, 0);
});

test('boundary tokens produce bounded requests and accept echoed results', async () => {
  const f = fixture();
  for (const token of ['', '"'.repeat(128)]) {
    const promise = f.bridge.ping(token);
    const index = f.calls.length - 1;
    const request = JSON.parse(f.calls[index].request);
    assert.deepEqual(Object.keys(request).sort(), ['id', 'method', 'params', 'protocol', 'version']);
    assert.match(request.id, /^[A-Za-z0-9_-]{1,64}$/);
    assert.equal(request.method, 'host.ping');
    assert.deepEqual(request.params, { token });
    assert.ok(Buffer.byteLength(f.calls[index].request, 'utf8') <= 4096);
    f.reply(index, { token, sequence: 1 });
    await promise;
  }
});

test('default timer implementation cancels a nonresponsive host', async () => {
  const canceled = [];
  const bridge = createBridge({
    timeoutMs: 10,
    host: {
      kunlunQuery() { return 0; },
      kunlunQueryCancel(id) { canceled.push(id); },
    },
  });
  await assert.rejects(bridge.describe(), { code: 'timeout' });
  assert.deepEqual(canceled, [0]);
});
