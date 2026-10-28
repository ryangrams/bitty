import assert from 'node:assert/strict';
import {LibraryClient, crc32, hex32, validateRom, cleanTitle, localClockFields} from '../web/protocol.mjs';
const encoder = new TextEncoder();
assert.equal(hex32(crc32(encoder.encode('123456789'))),'cbf43926');
assert.equal(cleanTitle('One\tTwo\nGame.nes'),'OneTwoGame');
const rom = new Uint8Array(16+16384+8192); rom.set([78,69,83,26,1,1]);
assert.equal(validateRom(rom).mapper,0);
assert.throws(() => validateRom(new Uint8Array(16)));
const pal = rom.slice(); pal[9] = 1; assert.throws(() => validateRom(pal),/NTSC/);
class Port {
  constructor() { this.opens=[]; this.received=0; this.last=null; this.late=false; }
  async open(options) {
    this.opens.push(options.baudRate);
    this.readable = new ReadableStream({start: c => {this.controller=c;}});
    this.writable = new WritableStream({write: bytes => this.handle(new TextDecoder().decode(bytes))});
  }
  getInfo() { return {usbVendorId:this.s3 ? 0x303a : 0x1a86}; }
  async setSignals(signals) { assert.deepEqual(signals,{dataTerminalReady:!!this.s3,requestToSend:false}); }
  async close() { if (!this.readable.locked) { try { this.controller.close(); } catch {} } }
  reply(id,text) {
    // Fragment every reply across reads, including a CRLF boundary.
    const wire = encoder.encode(`boot noise\nTP1 ${id}\t${text}\r\n`);
    this.controller.enqueue(wire.subarray(0,9)); this.controller.enqueue(wire.subarray(9));
  }
  handle(line) {
    const [prefix,id,command,...fields] = line.trimEnd().split('\t'); assert.equal(prefix,'TP1');
    if (this.last) this.reply(...this.last); // Delayed duplicate from a previous request.
    if (command === 'HELLO') this.reply(id,`HELLO\t1\t${this.s3 ? 'M5StickS3' : 'M5StickCPlus'}\t1966080\t1`);
    else if (command === 'TIME') {
      if (this.noClock) this.reply(id,'ERROR\tCOMMAND');
      else { this.clock=fields; this.reply(id,`OK\tTIME\t${fields.join('\t')}`); }
    }
    else if (command === 'LIST') { this.reply(id,'ITEM\t7\t1\t24592\t12345678\tSynthetic <cart>\t0'); this.reply(id,'OK\t1957888\t28672\t1929216'); }
    else if (command === 'BEGIN') { this.received=0; this.reply(id,'OK\tBEGIN'); }
    else if (command === 'CHUNK') {
      const [offset,crc,hex]=fields, bytes=Uint8Array.from(hex.match(/../g),v=>parseInt(v,16));
      assert.equal(Number(offset),this.received); assert.equal(hex32(crc32(bytes)),crc);
      this.received+=bytes.length; this.reply(id,`OK\t${this.received}`);
    } else this.reply(id,`OK\t${command}`);
    this.last=[id,`OK\t${command}`];
  }
}
const port = new Port(), client = new LibraryClient(port);
await client.open(); assert.deepEqual(port.opens,[9600,115200]);
const list = await client.request('LIST'); assert.equal(list.items[0][4],'Synthetic <cart>'); assert.equal(list.items.length,1);
let complete=0; await client.install(9,1,'Synthetic',rom,n=>complete=n); assert.equal(complete,1); assert.equal(port.received,rom.length);
await client.close();
// Clock fields use local wall time, including the calendar date across UTC boundaries.
for (const date of [new Date(2028,1,29,23,59,58),new Date(2026,9,28,0,0,1)]) {
  assert.deepEqual(localClockFields(date),[date.getFullYear(),date.getMonth()+1,date.getDate(),date.getHours(),date.getMinutes(),date.getSeconds()]);
  const clockPort = new Port(); clockPort.s3 = true;
  const clockClient = new LibraryClient(clockPort);
  await clockClient.open({library:false});
  assert.deepEqual(clockPort.opens,[115200]);
  await clockClient.syncClock(date);
  assert.deepEqual(clockPort.clock,localClockFields(date).map(String));
  await clockClient.close();
}
assert.throws(() => localClockFields(new Date(NaN)),/computer clock/);
assert.throws(() => localClockFields(new Date(2100,0,1)),/computer clock/);
const oldPort = new Port(); oldPort.noClock = true;
const oldClient = new LibraryClient(oldPort); await oldClient.open();
await assert.rejects(() => oldClient.syncClock(),/firmware could not set the clock/);
assert.equal((await oldClient.request('LIST')).items[0][0],'7');
await oldClient.close();
const s3Port = new Port(); s3Port.s3 = true;
const s3Client = new LibraryClient(s3Port); await s3Client.open();
await s3Client.syncClock(new Date(2026,9,28,12,34,56));
await s3Client.close();
// Bounds are enforced even if an unrelated device sends unterminated data.
const floodPort = new Port(), flood = new LibraryClient(floodPort); await flood.open();
floodPort.controller.enqueue(new Uint8Array(16385).fill(120));
await flood.reading; assert(flood.closed); assert.match(flood.readError.message,/limit/);
await flood.close();
console.log('Browser USB CRC, ROM preflight, fragmented replies, delayed duplicates, uploads and receive bounds passed');
