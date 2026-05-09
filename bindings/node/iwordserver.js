'use strict';
/**
 * iword/server — iwordserver client for Node.js (no native addon required).
 *
 * Connects to a running iwordserver over Unix socket or TCP and exposes
 * the same API surface as the N-API binding in index.js.
 *
 * Quick start:
 *   bin/iwordctl load words.txt
 *   bin/iwordserver -u /tmp/iword.sock -p 0
 *
 *   const { Client } = require('./bindings/node/iwordserver');
 *   const c = await Client.unix('/tmp/iword.sock');
 *   const key     = await c.seek('spam_word');       // 2, or -1
 *   const matches = await c.map('get free prize');   // [{position,length,key}]
 *   const clean   = await c.filterText('get free prize');
 *   await c.close();
 *
 * All methods return Promises and are safe for concurrent use —
 * iwordserver serializes iword calls internally.
 */

const net = require('net');

const MODE_HTML    = 0x1;
const MODE_FORBID  = 0x2;
const MODE_ENGLISH = 0x4;

class IwordServerError extends Error {}

class Client {
  /**
   * @param {net.Socket} socket - connected socket
   */
  constructor(socket) {
    this._socket = socket;
    this._buf = '';
    this._queue = [];  // [{resolve, reject}]
    this._closed = false;

    socket.setEncoding('utf8');
    socket.on('data', (chunk) => this._onData(chunk));
    socket.on('error', (err) => this._onError(err));
    socket.on('close', () => this._onClose());
  }

  // ----------------------------------------------------------------
  // Factory helpers
  // ----------------------------------------------------------------

  /** Connect via Unix socket. Returns Promise<Client>. */
  static unix(path, timeout) {
    return Client._connect({ path }, timeout);
  }

  /** Connect via TCP. Returns Promise<Client>. */
  static tcp(host, port, timeout) {
    return Client._connect({ host, port }, timeout);
  }

  static _connect(opts, timeout) {
    return new Promise((resolve, reject) => {
      const sock = net.createConnection(opts, () => {
        if (timeout) sock.setTimeout(0);
        resolve(new Client(sock));
      });
      if (timeout) sock.setTimeout(timeout);
      sock.on('timeout', () => { sock.destroy(); reject(new IwordServerError('connect timeout')); });
      sock.on('error', reject);
    });
  }

  // ----------------------------------------------------------------
  // Internal protocol
  // ----------------------------------------------------------------

  _onData(chunk) {
    this._buf += chunk;
    let nl;
    while ((nl = this._buf.indexOf('\n')) !== -1) {
      const line = this._buf.slice(0, nl);
      this._buf = this._buf.slice(nl + 1);
      const entry = this._queue.shift();
      if (!entry) continue;
      try {
        const resp = JSON.parse(line);
        if (resp.error) entry.reject(new IwordServerError(resp.error));
        else entry.resolve(resp);
      } catch (e) {
        entry.reject(e);
      }
    }
  }

  _onError(err) {
    const pending = this._queue.splice(0);
    for (const e of pending) e.reject(err);
  }

  _onClose() {
    this._closed = true;
    const pending = this._queue.splice(0);
    for (const e of pending) e.reject(new IwordServerError('connection closed'));
  }

  _call(req) {
    return new Promise((resolve, reject) => {
      if (this._closed) { reject(new IwordServerError('connection closed')); return; }
      this._queue.push({ resolve, reject });
      this._socket.write(JSON.stringify(req) + '\n');
    });
  }

  // ----------------------------------------------------------------
  // API
  // ----------------------------------------------------------------

  /** Close the connection. Returns Promise. */
  close() {
    return new Promise((resolve) => {
      this._closed = true;
      this._socket.end(resolve);
    });
  }

  /** Verify the connection is alive. Returns Promise<void>. */
  async ping() {
    const resp = await this._call({ op: 'ping' });
    if (!resp.pong) throw new IwordServerError('unexpected ping response');
  }

  /** Return category key (0–14), or -1 if not found. Returns Promise<number>. */
  async seek(word) {
    const resp = await this._call({ op: 'seek', word });
    return resp.found ? resp.key : -1;
  }

  /**
   * Extract all matching words from text.
   * Returns Promise<Array<{position, length, key}>>.
   */
  async map(text, mode) {
    if (mode === undefined) mode = MODE_HTML | MODE_FORBID;
    const resp = await this._call({ op: 'map', text, mode });
    return (resp.matches || []).map(m => ({
      position: m.pos,
      length:   m.len,
      key:      m.key,
    }));
  }

  /** Return bitmask of category keys present in the loaded dictionary. Returns Promise<number>. */
  async mask() {
    const resp = await this._call({ op: 'mask' });
    return resp.mask;
  }

  /** Return server status. Returns Promise<{loaded, version}>. */
  async status() {
    return this._call({ op: 'status' });
  }

  /** Replace all matched words in text with '*'. Returns Promise<string>. */
  async filterText(text, mode) {
    if (mode === undefined) mode = MODE_HTML | MODE_FORBID;
    const matches = await this.map(text, mode);
    if (matches.length === 0) return text;
    const buf = Buffer.from(text, 'utf8');
    for (const m of matches) buf.fill(0x2a, m.position, m.position + m.length);
    return buf.toString('utf8');
  }

  /** Extract only matches with a specific category key. Returns Promise<Array>. */
  async extractByKey(text, key, mode) {
    if (mode === undefined) mode = MODE_HTML;
    const matches = await this.map(text, mode);
    return matches.filter(m => m.key === key);
  }
}

module.exports = {
  Client,
  IwordServerError,
  MODE_HTML,
  MODE_FORBID,
  MODE_ENGLISH,
};
