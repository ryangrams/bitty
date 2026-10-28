export function crc32(bytes) {
  let crc = 0xffffffff;
  for (const byte of bytes) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit++) crc = (crc >>> 1) ^ (0xedb88320 & -(crc & 1));
  }
  return (crc ^ 0xffffffff) >>> 0;
}
export const hex32 = value => (value >>> 0).toString(16).padStart(8, '0');
export function validateRom(bytes) {
  if (bytes.length < 16 || bytes.length > 1024 * 1024 || bytes[0] !== 78 || bytes[1] !== 69 || bytes[2] !== 83 || bytes[3] !== 26)
    throw new Error('Choose an uncompressed .nes cartridge, up to 1 MiB.');
  const nes2 = (bytes[7] & 12) === 8;
  const mapper = (bytes[6] >>> 4) | (bytes[7] & 240) | (nes2 ? (bytes[8] & 15) << 8 : 0);
  if (![0, 1, 4, 37, 118].includes(mapper)) throw new Error(`Mapper ${mapper} is not supported.`);
  if ((nes2 ? bytes[12] & 3 : bytes[9] & 1) === 1) throw new Error('Choose an NTSC cartridge.');
  if (bytes[6] & 4) throw new Error('Cartridges with trainer data are not supported.');
  const prg = bytes[4] | (nes2 ? (bytes[9] & 15) << 8 : 0);
  const chr = bytes[5] | (nes2 ? (bytes[9] >>> 4) << 8 : 0);
  if (!prg || bytes.length !== 16 + prg * 16384 + chr * 8192) throw new Error('The ROM length does not match its header.');
  return {mapper, crc: crc32(bytes)};
}
export function cleanTitle(title) {
  return title.replace(/\.nes$/i, '').replace(/[^\x20-\x7e]/g, '').trim().slice(0, 32) || 'NES cartridge';
}

export function localClockFields(date = new Date()) {
  if (!(date instanceof Date) || !Number.isFinite(date.getTime()) || date.getFullYear() < 2000 || date.getFullYear() > 2099)
    throw new Error('Set the computer clock to a date between 2000 and 2099.');
  return [date.getFullYear(),date.getMonth()+1,date.getDate(),date.getHours(),date.getMinutes(),date.getSeconds()];
}

// One request at a time. Receive bounds and deadlines also apply to boot noise.
export class LibraryClient {
  constructor(port) { this.port = port; this.queue = []; this.buffer = ''; this.closed = false; this.waiter = null; this.sequence = 0; this.busy = false; }
  async open({library = true} = {}) {
    this.librarySession = library;
    const nativeUsb = this.port.getInfo?.().usbVendorId === 0x303a;
    const signals = {dataTerminalReady:nativeUsb, requestToSend:false};
    if (!nativeUsb) {
      await this.port.open({baudRate:9600});
      await this.port.setSignals(signals);
      await this.port.close();
    }
    await this.port.open({baudRate:115200});
    await this.port.setSignals(signals);
    this.reader = this.port.readable.getReader();
    this.writer = this.port.writable.getWriter();
    this.reading = this.readLoop();
    if (!library) return;
    const hello = await this.request('HELLO', [], 'HELLO', 8000);
    if (hello[1] !== '1' || !['M5StickCPlus','M5StickS3'].includes(hello[2])) throw new Error('This device is running an incompatible library protocol.');
    if (hello[4] !== '1') throw new Error('Cartridge storage is unavailable. Read the recovery instructions before reinstalling or erasing the device.');
    return hello;
  }
  async syncClock(date = new Date()) {
    const fields = localClockFields(date);
    const reply = await this.request('TIME',fields);
    if (reply.length !== 8 || fields.some((value,index) => Number(reply[index+2]) !== value))
      throw new Error('The device did not confirm the computer clock. Reconnect and try again.');
    return date;
  }
  async readLoop() {
    const decoder = new TextDecoder();
    try {
      while (!this.closed) {
        const {value, done} = await this.reader.read();
        if (done) break;
        this.buffer += decoder.decode(value, {stream: true});
        if (this.buffer.length > 16384) throw new Error('The serial response exceeded its limit.');
        let end;
        while ((end = this.buffer.indexOf('\n')) >= 0) {
          const line = this.buffer.slice(0, end).replace(/\r$/, ''); this.buffer = this.buffer.slice(end + 1);
          if (!line.startsWith('TP1 ')) continue;
          if (this.queue.length >= 32) throw new Error('Too many serial responses.');
          this.queue.push(line.slice(4).split('\t'));
          if (this.waiter) { const wake = this.waiter; this.waiter = null; wake(); }
        }
      }
    } catch (error) { this.readError = error; }
    finally { this.closed = true; if (this.waiter) { const wake = this.waiter; this.waiter = null; wake(); } }
  }
  async response(deadline) {
    while (!this.queue.length) {
      if (this.closed) throw this.readError || new Error('USB disconnected. Reconnect to check the library.');
      const left = deadline - Date.now();
      if (left <= 0) throw new Error('The M5 did not respond in time.');
      await new Promise(resolve => {
        const timer = setTimeout(() => { this.waiter = null; resolve(); }, left);
        this.waiter = () => { clearTimeout(timer); resolve(); };
      });
    }
    return this.queue.shift();
  }
  async request(command, fields = [], expected = 'OK', timeout = 8000) {
    if ([command, ...fields].some(field => /[\r\n\t]/.test(String(field)))) throw new Error('Invalid command field.');
    if (this.busy) throw new Error('A USB request is already in progress.');
    this.busy = true;
    const sequence = String(this.sequence = this.sequence >= 0xfffffffe ? 1 : this.sequence + 1);
    try {
    await this.writer.write(new TextEncoder().encode(['TP1', sequence, command, ...fields].join('\t') + '\n'));
    const deadline = Date.now() + timeout;
    const items = [];
    for (;;) {
      const response = await this.response(deadline);
      if (response[0] !== sequence) continue;
      const reply = response.slice(1);
      if (reply[0] === 'ERROR') {
        if (command === 'TIME') throw new Error('The firmware could not set the clock. Install a build with USB clock sync and try again.');
        throw new Error(`Device: ${reply[1]}. Your previous cartridge remains installed until a replacement commits.`);
      }
      if (reply[0] === 'BYE') throw new Error('Library session ended. Reconnect to continue.');
      if (reply[0] === 'ITEM') { items.push(reply.slice(1)); continue; }
      if (reply[0] === expected) {
        if (command === 'LIST') reply.items = items;
        else if (expected === 'OK' && command !== 'CHUNK' && reply[1] !== command) continue;
        return reply;
      }
    }
    } finally { this.busy = false; }
  }
  async install(id, kind, title, bytes, onProgress = () => {}) {
    const {crc} = validateRom(bytes);
    await this.request('BEGIN', [id, kind, bytes.length, hex32(crc), cleanTitle(title)]);
    try {
      for (let offset = 0; offset < bytes.length; offset += 256) {
        const chunk = bytes.subarray(offset, offset + 256);
        const hex = Array.from(chunk, byte => byte.toString(16).padStart(2, '0')).join('');
        const reply = await this.request('CHUNK', [offset, hex32(crc32(chunk)), hex]);
        if (Number(reply[1]) !== offset + chunk.length) throw new Error('Unexpected upload acknowledgement.');
        onProgress((offset + chunk.length) / bytes.length);
      }
      await this.request('COMMIT', [], 'OK', 15000);
    } catch (error) {
      if (!this.closed) { try { await this.request('ABORT'); } catch {} }
      throw error;
    }
  }
  async close() {
    if (!this.closed && this.writer && this.librarySession) { try { await this.request('EXIT'); } catch {} }
    this.closed = true;
    try { await this.reader?.cancel(); } catch {}
    await this.reading;
    this.reader?.releaseLock(); this.writer?.releaseLock();
    try { await this.port.close(); } catch {}
  }
}
