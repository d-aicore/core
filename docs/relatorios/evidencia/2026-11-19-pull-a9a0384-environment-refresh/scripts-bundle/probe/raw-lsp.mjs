import { spawn } from 'node:child_process';
const clangd = process.argv[2];
const child = spawn(clangd, ['--background-index'], { stdio: ['pipe','pipe','pipe'] });
let out = Buffer.alloc(0);
child.stdout.on('data', d => { out = Buffer.concat([out, d]); console.log('RECV', d.length, 'bytes'); });
child.stderr.on('data', d => console.log('ERR', String(d).slice(0,120)));
const payload = Buffer.from(JSON.stringify({jsonrpc:'2.0',id:1,method:'initialize',params:{processId:null,rootUri:'file:///D:/CORE/CORE',capabilities:{},workspaceFolders:[{uri:'file:///D:/CORE/CORE',name:'core'}]}}));
child.stdin.write('Content-Length: ' + payload.length + '\r\n\r\n');
child.stdin.write(payload);
setTimeout(() => {
  console.log('total bytes received:', out.length);
  console.log('first 200 chars:', out.slice(0,200).toString('utf8').replace(/\r?\n/g,' '));
  child.kill();
  process.exit(0);
}, 15000);
