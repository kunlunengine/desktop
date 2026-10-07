import { createBridge } from './bridge.js';

const bridge = createBridge();
const element = id => document.getElementById(id);
let pingNumber = 0;
function activity(message) {
  const list = element('activity');
  list.querySelector('.empty')?.remove();
  const item = document.createElement('li');
  const time = document.createElement('time');
  const now = new Date();
  time.dateTime = now.toISOString();
  time.textContent = now.toLocaleTimeString();
  item.append(time, document.createTextNode(message.slice(0, 360)));
  list.prepend(item);
  while (list.children.length > 8) list.lastElementChild.remove();
}

async function describe() {
  element('refresh').disabled = true;
  element('host-status').textContent = 'Contacting desktop host…';
  try {
    const host = await bridge.describe();
    for (const [id, value] of Object.entries({
      'host-name': host.name,
      'host-version': host.version,
      backend: host.backend.toUpperCase(),
      'cef-version': host.cefVersion,
      'chromium-version': host.chromiumVersion,
      'sandbox-requested': host.sandboxRequested ? 'Yes · not qualification evidence' : 'No',
    })) element(id).textContent = value;
    element('host-status').textContent = 'Host diagnostics received. Sandbox remains unqualified.';
    activity('Host diagnostics received.');
  } catch (error) {
    for (const id of ['host-name', 'host-version', 'backend', 'cef-version', 'chromium-version', 'sandbox-requested']) {
      element(id).textContent = 'Unavailable';
    }
    element('host-status').textContent = error.message;
    activity(error.message);
  } finally {
    element('refresh').disabled = false;
  }
}

element('refresh').addEventListener('click', describe);
element('ping').addEventListener('click', async () => {
  element('ping').disabled = true;
  element('ping-status').textContent = 'Waiting for host echo…';
  try {
    const result = await bridge.ping(`d1-ui-${++pingNumber}`);
    const message = `Host replied · sequence ${result.sequence} · token ${result.token}`;
    element('ping-status').textContent = message;
    activity(message);
  } catch (error) {
    element('ping-status').textContent = error.message;
    activity(error.message);
  } finally {
    element('ping').disabled = false;
  }
});
describe();
