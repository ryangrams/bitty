import assert from 'node:assert/strict';
import {LibraryClient, crc32, hex32, validateRom, cleanTitle} from '../web/protocol.mjs';
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
  async setSignals(signals) { assert.deepEqual(signals,{dataTerminalReady:false,requestToSend:false}); }
  async close() { if (!this.readable.locked) { try { this.controller.close(); } catch {} } }
  reply(id,text) {
    // Fragment every reply across reads, including a CRLF boundary.
    const wire = encoder.encode(`boot noise\nTP1 ${id}\t${text}\r\n`);
    this.controller.enqueue(wire.subarray(0,9)); this.controller.enqueue(wire.subarray(9));
  }
  handle(line) {
    const [prefix,id,command,...fields] = line.trimEnd().split('\t'); assert.equal(prefix,'TP1');
    if (this.last) this.reply(...this.last); // Delayed duplicate from a previous request.
    if (command === 'HELLO') this.reply(id,'HELLO\t1\tM5StickCPlus\t1966080\t1');
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
// Bounds are enforced even if an unrelated device sends unterminated data.
const floodPort = new Port(), flood = new LibraryClient(floodPort); await flood.open();
floodPort.controller.enqueue(new Uint8Array(16385).fill(120));
await flood.reading; assert(flood.closed); assert.match(flood.readError.message,/limit/);
await flood.close();
console.log('Browser USB CRC, ROM preflight, fragmented replies, delayed duplicates, uploads and receive bounds passed');
