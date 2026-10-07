#ifndef KUNLUN_DESKTOP_CEF_SMOKE_TEST_H_
#define KUNLUN_DESKTOP_CEF_SMOKE_TEST_H_

namespace kunlun {

// Fixed, host-injected test code, executed only with --smoke-test. There is no
// user-supplied eval/inspection endpoint. These checks do not qualify a sandbox.
inline constexpr char kNativeSmokeScript[] = R"JS(
(async () => {
  const check = (condition, message) => {
    if (!condition) throw new Error(message);
  };
  const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
  const query = (request, persistent = false) => new Promise((resolve, reject) => {
    kunlunQuery({
      request: typeof request === 'string' ? request : JSON.stringify(request),
      persistent,
      onSuccess: value => resolve(JSON.parse(value)),
      onFailure: (code, message) => reject(Object.assign(new Error(message), {code})),
    });
  });
  const envelope = (method, params = {}) => ({
    protocol: 'kunlun.desktop.host', version: 1, id: 'native-smoke', method, params,
  });
  const denied = async (request, persistent = false) => {
    try { await query(request, persistent); }
    catch (error) {
      check([400, 403, 413, 429].includes(error.code), 'unexpected rejection code');
      return;
    }
    throw new Error('invalid request was accepted');
  };
  try {
    check(location.href === 'kunlun://desktop/index.html', 'wrong document origin');
    check(isSecureContext, 'application origin is not a secure context');
    check(typeof window.kunlunQuery === 'function', 'missing renderer bridge');
    const {createBridge} = await import('kunlun://desktop/bridge.js');
    const bridge = createBridge();
    const host = await bridge.describe();
    check(host.backend === 'cef' && host.sandboxRequested &&
          !host.sandboxQualified && !host.runtimeConnected, 'wrong host diagnostics');
    const ping = await bridge.ping('native-smoke-token');
    check(ping.token === 'native-smoke-token', 'ping echo mismatch');
    await denied({...envelope('host.describe'), version: 2});
    await denied({...envelope('host.describe'), version: '1'});
    await denied({...envelope('host.describe'), id: 'forged/id'});
    await denied({...envelope('host.describe'), protocol: 'kunlun.runtime-provider/v0.2'});
    await denied(envelope('app.eval'));
    await denied({...envelope('host.describe'), capability: 'forged'});
    await denied(envelope('host.describe', {unexpected: true}));
    await denied(envelope('host.ping', {token: '\n'}));
    await denied(envelope('host.ping', {token: 'x'.repeat(129)}));
    await denied('x'.repeat(4097));
    await denied(envelope('host.describe'), true);

    // Bypass the client admission limit to test the native router itself.
    // CEF completion/IPC tasks may interleave, so overload rejection counts
    // vary; the browser checks its pending registry after every routed message.
    const burst = await Promise.all(Array.from({length: 64}, (_, index) =>
      query({...envelope('host.ping', {token: 'burst'}), id: `burst-${index}`})
        .then(value => ({value}), error => ({error}))));
    check(burst.some(reply => reply.value?.result.token === 'burst'),
          'native burst made no progress');
    check(burst.every(reply => reply.value?.result.token === 'burst' ||
                               reply.error?.code === -1),
          'unexpected native overload result');
    const invalidBurst = await Promise.all(Array.from({length: 64}, (_, index) =>
      query({...envelope('app.eval'), id: `invalid-${index}`})
        .then(() => false, error => [-1, 400].includes(error.code))));
    check(invalidBurst.every(Boolean), 'invalid burst escaped native admission');
    await bridge.ping('after-burst');

    const popup = window.open('https://example.invalid/', '_blank');
    check(popup === null, 'popup was not blocked');
    const inline = document.createElement('script');
    inline.textContent = 'window.kunlunInlineExecuted = true';
    document.head.append(inline);
    await delay(100);
    check(window.kunlunInlineExecuted !== true, 'CSP allowed inline code');

    // Exercise the actual UI handlers and wait for their asynchronous response.
    for (let attempt = 0; attempt < 100; ++attempt) {
      if (document.querySelector('#host-name').textContent === 'Kunlun Desktop') break;
      await delay(25);
    }
    check(document.querySelector('#host-name').textContent === 'Kunlun Desktop',
          'presentation module did not report the host');
    document.querySelector('#ping').click();
    for (let attempt = 0; attempt < 100; ++attempt) {
      if (document.querySelector('#ping-status').textContent.includes('Host replied')) break;
      await delay(25);
    }
    check(document.querySelector('#ping-status').textContent.includes('Host replied'),
          'presentation ping interaction failed');
    check(getComputedStyle(document.documentElement).colorScheme === 'dark',
          'bundled stylesheet did not load');
    console.log('KUNLUN_NATIVE_SMOKE_PASS');
  } catch (error) {
    console.error('KUNLUN_NATIVE_SMOKE_FAIL: ' + String(error.message).slice(0, 240));
  }
})();
)JS";

}  // namespace kunlun

#endif  // KUNLUN_DESKTOP_CEF_SMOKE_TEST_H_
