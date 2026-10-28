import {LibraryClient, cleanTitle, validateRom} from './protocol.mjs';
import {project} from './config.mjs';
const $ = id => document.getElementById(id);
let client = null, records = [], busy = false;
const kinds = {1:'NES',2:'Tetris GYM',3:'Tetris Zero',4:'Geometry Dash'};
const bytesLabel = bytes => `${(bytes / 1024).toFixed(1)} KiB`;
function message(id, text) { $(id).textContent = text; }
function buttons() {
  $('connect').disabled = busy || !('serial' in navigator) || !isSecureContext;
  $('disconnect').disabled = busy;
  $('sync-clock').disabled = busy || !('serial' in navigator) || !isSecureContext;
  $('install-cart').disabled = busy || !client || !$('rights').checked || !$('file').files.length;
  for (const button of $('cartridges').querySelectorAll('button')) button.disabled = busy || button.dataset.quarantined === 'true';
}
async function operation(work) {
  if (busy) return;
  busy = true; buttons();
  try { await work(); }
  catch (error) { message('connection', error.message); }
  finally { busy = false; buttons(); }
}
async function list() {
  const reply = await client.request('LIST');
  records = reply.items.map(([id,kind,bytes,crc,title,quarantined]) => ({id:Number(id),kind:Number(kind),bytes:Number(bytes),crc,title,quarantined:quarantined === '1'}));
  const capacity = Number(reply[1]), used = Number(reply[2]), largest = Number(reply[3]);
  $('storage').hidden = false; $('space').max = capacity; $('space').value = used;
  message('space-label', `${bytesLabel(used)} used / ${bytesLabel(capacity)} cartridge storage`);
  message('largest', `Largest free block: ${bytesLabel(largest)}`);
  $('cartridges').replaceChildren();
  if (!records.length) {
    const empty = document.createElement('p'); empty.className = 'empty'; empty.textContent = 'Your library is empty. Add a cartridge below.';
    $('cartridges').append(empty);
  }
  for (const record of records) {
    const card = document.createElement('article'); card.className = 'cartridge';
    const icon = document.createElement('span'); icon.className = 'cart-icon'; icon.textContent = kinds[record.kind];
    const title = document.createElement('h3'); title.textContent = record.title;
    const detail = document.createElement('p'); detail.className = 'quiet'; detail.textContent = `${bytesLabel(record.bytes)} · CRC ${record.crc}${record.quarantined ? ' · Checksum failed. Reinstall this file.' : ''}`;
    card.append(icon,title,detail);
    for (const [label,command] of [['Play','LAUNCH'],['Game at boot','BOOT'],['Remove','REMOVE']]) {
      const button = document.createElement('button'); button.textContent = label; button.dataset.quarantined = String(record.quarantined && command !== 'REMOVE');
      button.addEventListener('click', () => {
        if (command === 'REMOVE') {
          message('remove-name', record.title); $('remove-dialog').showModal();
          $('remove-dialog').onclose = () => {
            if ($('remove-dialog').returnValue === 'remove') operation(async () => { await client.request(command,[record.id]); await list(); message('connection','Cartridge removed. Its flash allocation is available again.'); });
          };
        } else operation(async () => {
          await client.request(command,[record.id]);
          if (command === 'LAUNCH') await disconnect(false);
          else message('connection', `${record.title} will start at boot.`);
        });
      });
      card.append(button);
    }
    $('cartridges').append(card);
  }
  buttons();
}
async function disconnect(exit = true) {
  const previous = client; client = null;
  if (previous) {
    if (!exit) previous.closed = true;
    await previous.close();
  }
  $('connect').hidden = false; $('disconnect').hidden = true;
  message('connection', 'Disconnected. The device has returned to its game.');
}
$('connect').addEventListener('click', () => operation(async () => {
  const port = await navigator.serial.requestPort();
  const next = new LibraryClient(port);
  try { await next.open(); } catch (error) { await next.close(); throw error; }
  client = next;
  $('connect').hidden = true; $('disconnect').hidden = false;
  message('connection', 'Connected. The M5 is in USB library mode.');
  await list();
  try { await syncClock(next); } catch (error) { message('clock-status',error.message); }
}));
async function syncClock(connection) {
  const date = await connection.syncClock();
  const label = new Intl.DateTimeFormat(undefined,{dateStyle:'medium',timeStyle:'medium'}).format(date);
  const zone = Intl.DateTimeFormat().resolvedOptions().timeZone;
  message('clock-status',`Clock set to ${label} (${zone}).`);
}
$('sync-clock').addEventListener('click', () => operation(async () => {
  if (client) { await syncClock(client); return; }
  // This short session leaves the current game running and needs no cartridge storage.
  const port = await navigator.serial.requestPort();
  const next = new LibraryClient(port);
  try { await next.open({library:false}); await syncClock(next); }
  finally { await next.close(); }
}));
$('disconnect').addEventListener('click', () => operation(() => disconnect()));
$('file').addEventListener('change', () => { if ($('file').files[0]) $('title').value = cleanTitle($('file').files[0].name); buttons(); });
$('rights').addEventListener('change', buttons);
$('upload').addEventListener('submit', event => {
  event.preventDefault();
  operation(async () => {
    if (!client || !$('rights').checked) throw new Error('Connect the M5 and confirm your rights first.');
    const file = $('file').files[0];
    if (!file || file.size > 1024 * 1024) throw new Error('Choose a .nes file up to 1 MiB.');
    const bytes = new Uint8Array(await file.arrayBuffer()); validateRom(bytes);
    const id = Array.from({length:255},(_,i) => i+1).find(id => !records.some(record => record.id === id));
    if (!id) throw new Error('Remove a cartridge to make a slot available.');
    $('progress').hidden = false; $('progress').value = 0;
    message('upload-status','Installing over USB. Keep the cable connected.');
    await client.install(id,Number($('kind').value),$('title').value,bytes,value => {
      $('progress').value = value; message('upload-status',`Uploading: ${Math.floor(value*100)}%`);
    });
    message('upload-status','Installed and verified by the M5.');
    await list();
  });
});
// Keep an idle browser session alive. Uploads supply their own activity.
setInterval(() => {
  if (client && !busy && !client.busy) operation(async () => { await client.request('PING'); });
},10000);
if (!('serial' in navigator)) message('connection','This browser does not expose Web Serial. Use a supported desktop browser. iOS is unsupported.');
else if (!isSecureContext) message('connection','Open the HTTPS site to use USB access.');
if (project.sponsors) { $('support').hidden = false; $('sponsor').href = project.sponsors; }
if (project.discord) { $('discord').hidden = false; $('discord').href = project.discord; }
if (project.hardware.length) {
  $('hardware').hidden = false;
  for (const item of project.hardware) {
    const li = document.createElement('li'), link = document.createElement('a');
    link.href = item.url; link.textContent = `${item.name} (affiliate link)`; link.rel = 'sponsored noopener';
    li.append(link); $('hardware-links').append(li);
  }
}
async function installer() {
  const status = await fetch('release.json', {cache:'no-cache'});
  if (!status.ok) throw new Error('The release status is unavailable.');
  if (!(await status.json()).available) {
    message('release-status','Interface preview. Firmware downloads will open after hardware installation and transfer checks.');
    $('installer').querySelector('button').textContent = 'Hardware verification pending';
    return;
  }
  const response = await fetch('firmware/manifest.json',{cache:'no-cache'});
  if (!response.ok) throw new Error('The installable release is not published yet.');
  const manifest = await response.json();
  if (manifest.name !== 'Bitty' || !manifest.builds?.length) throw new Error('The firmware manifest is invalid.');
  message('release-status',`Release ${manifest.version}. Project-created starter firmware. No commercial game files included.`);
  await import('./installer.js');
  const install = document.createElement('esp-web-install-button'); install.setAttribute('manifest','firmware/manifest.json');
  const button = document.createElement('button'); button.slot = 'activate'; button.textContent = 'Install Bitty over USB';
  button.disabled = true; install.append(button);
  const enabled = () => { button.disabled = !$('board').checked || !$('replace').checked || !!client; };
  $('board').addEventListener('change',enabled); $('replace').addEventListener('change',enabled);
  $('installer').replaceChildren(install);
  const unsupported = document.createElement('span'); unsupported.slot = 'unsupported'; unsupported.textContent = 'Use a desktop browser with Web Serial.'; install.append(unsupported);
  setInterval(enabled,1000);
}
installer().catch(error => message('release-status',error.message));
buttons();
