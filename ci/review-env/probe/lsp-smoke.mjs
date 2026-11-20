// lsp-smoke.mjs - drives clangd over JSON-RPC/stdio like the dsh-cpp-lsp plugin does, and proves
// cross-file go-to-definition and find-references on the Core compile database.
// It retries while clangd's background index warms up (a cold index returns the declaration and
// zero references - ENV-06 in the 2026-11-19 environment report).
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';

const args = process.argv.slice(2);
const get = (k, d) => { const i = args.indexOf(k); return i >= 0 ? args[i + 1] : d; };
const clangd = get('--clangd', 'clangd');
const ccdir = get('--ccdir', '.');
const src = get('--src', '.');

const hpp = path.join(src, 'engine', 'src', 'core', 'tempo_map.hpp');
const text = fs.readFileSync(hpp, 'utf8');
const lines = text.split(/\r?\n/);
let line = -1, character = -1;
for (let i = 0; i < lines.length; ++i) {
  const j = lines[i].indexOf('sampleAt');
  if (j >= 0 && lines[i].includes('sampleAt(double')) { line = i; character = j; break; }
}
if (line < 0) { console.log('FAIL: sampleAt declaration not found in tempo_map.hpp'); process.exit(2); }
console.log('query: ' + path.basename(hpp) + ':' + (line + 1) + ':' + (character + 1) + ' (symbol sampleAt)');

const clangdArgs = ['--background-index', '--compile-commands-dir=' + ccdir];
if (process.env.CLANGD_QUERY_DRIVER) clangdArgs.push('--query-driver=' + process.env.CLANGD_QUERY_DRIVER);
console.log('clangd args: ' + clangdArgs.join(' '));
console.log('INCLUDE env present: ' + (process.env.INCLUDE ? 'yes' : 'no'));

const uri = (p) => 'file:///' + path.resolve(p).replace(/\\/g, '/');
const child = spawn(clangd, clangdArgs, { stdio: ['pipe', 'pipe', 'pipe'] });
let nextId = 1;
const pending = new Map();
const stderrTail = [];
child.stderr.on('data', (d) => { stderrTail.push(String(d)); if (stderrTail.length > 60) stderrTail.shift(); });

function write(msg) {
  const payload = Buffer.from(JSON.stringify(msg), 'utf8');
  child.stdin.write('Content-Length: ' + payload.length + '\r\n\r\n');
  child.stdin.write(payload);
}
function send(method, params) {
  const id = nextId++;
  return new Promise((resolve, reject) => {
    pending.set(id, { resolve, reject });
    write({ jsonrpc: '2.0', id, method, params });
  });
}
function notify(method, params) { write({ jsonrpc: '2.0', method, params }); }

let buf = Buffer.alloc(0);
child.stdout.on('data', (d) => {
  buf = Buffer.concat([buf, d]);
  for (;;) {
    const sep = buf.indexOf('\r\n\r\n');
    if (sep < 0) break;
    const header = buf.slice(0, sep).toString('ascii');
    const m = /Content-Length:\s*(\d+)/i.exec(header);
    if (!m) { buf = buf.slice(sep + 4); continue; }
    const len = parseInt(m[1], 10);
    if (buf.length < sep + 4 + len) break;
    const body = buf.slice(sep + 4, sep + 4 + len).toString('utf8');
    buf = buf.slice(sep + 4 + len);
    let msg;
    try { msg = JSON.parse(body); } catch { continue; }
    if (msg.id !== undefined && pending.has(msg.id)) {
      const p = pending.get(msg.id); pending.delete(msg.id);
      if (msg.error) p.reject(new Error(JSON.stringify(msg.error))); else p.resolve(msg.result);
    }
  }
});

const withTimeout = (ms, tag, promise) => Promise.race([
  promise, new Promise((_, rej) => setTimeout(() => rej(new Error('timeout ' + tag)), ms))
]);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function warm(method, params, accept, label, attempts = 12) {
  let last = null;
  for (let i = 1; i <= attempts; ++i) {
    last = await withTimeout(180000, label + '#' + i, send(method, params));
    if (accept(last)) { if (i > 1) console.log(label + ': warm after ' + i + ' attempts'); return last; }
    console.log(label + ': attempt ' + i + ' not warm yet (background index), waiting ' + Math.min(5 * i, 20) + 's');
    await sleep(Math.min(5000 * i, 20000));
  }
  return last;
}

let failures = 0;
try {
  await withTimeout(120000, 'initialize', (async () => {
    await send('initialize', {
      processId: null, rootUri: uri(src),
      capabilities: { textDocument: { definition: {}, references: {}, hover: {} } },
      workspaceFolders: [{ uri: uri(src), name: 'core' }]
    });
    notify('initialized', {});
  })());
  console.log('PASS: initialize handshake');
  notify('textDocument/didOpen', { textDocument: { uri: uri(hpp), languageId: 'cpp', version: 1, text } });

  const def = await warm('textDocument/definition', { textDocument: { uri: uri(hpp) }, position: { line, character } },
    (r) => (Array.isArray(r) ? r : r ? [r] : []).some((d) => d.uri && d.uri.includes('tempo_map.cpp')), 'definition');
  const defs = Array.isArray(def) ? def : def ? [def] : [];
  console.log('definition -> ' + defs.length + ' location(s):');
  for (const d of defs) console.log('  ' + d.uri.replace('file:///', '') + ':' + (d.range.start.line + 1) + ':' + (d.range.start.character + 1));
  if (defs.some((d) => d.uri && d.uri.includes('tempo_map.cpp'))) console.log('PASS: cross-file go-to-definition (hpp -> tempo_map.cpp)');
  else { console.log('FAIL: no definition in tempo_map.cpp'); ++failures; }

  const refs = await warm('textDocument/references',
    { textDocument: { uri: uri(hpp) }, position: { line, character }, context: { includeDeclaration: false } },
    (r) => (r || []).length >= 3, 'references');
  const list = refs || [];
  const files = new Set(list.map((r) => r.uri.split('/').pop()));
  console.log('references -> ' + list.length + ' in ' + files.size + ' file(s): ' + [...files].join(', '));
  if (list.length >= 3 && files.size >= 2) console.log('PASS: find-references returns usages in multiple files');
  else { console.log('FAIL: too few references'); ++failures; }
} catch (e) {
  console.log('FAIL: ' + e.message); ++failures;
  console.log('--- clangd stderr tail ---');
  console.log(stderrTail.join('').slice(-3000));
}
child.kill();
console.log(failures === 0 ? 'LSP SMOKE: PASS' : 'LSP SMOKE: FAIL (' + failures + ')');
process.exit(failures === 0 ? 0 : 1);
