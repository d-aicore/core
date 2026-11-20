"""Decode EchoBoy state blobs (JUCE magic-XML) and diff two consecutive captures."""
import base64, json, os, struct, sys, difflib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from engine_drive import Engine, ECHOBOY, WORK, EV, probe_song, note, save_json

KEY = "fx:src:eb"
eng = Engine("blob")
try:
    eng.cmd("engine.start", backend="null", sampleRate=48000, blockSize=512, listenDb=-15)
    song = probe_song(ECHOBOY, "eb", [note(0, 240, 60, 0.9)], name="blob probe")
    eng.cmd("project.load", project=song)
    b0 = eng.cmd("plugins.states", keys=[KEY])[KEY]
    eng.cmd("plugin.param", key=KEY, id="3", value=1.0)
    eng.cmd("plugin.param", key=KEY, id="11", value=0.6)
    b1 = eng.cmd("plugins.states", keys=[KEY])[KEY]
    b2 = eng.cmd("plugins.states", keys=[KEY])[KEY]
finally:
    eng.close()

def decode(b64):
    raw = base64.b64decode(b64)
    magic, ln = struct.unpack("<II", raw[:8])
    assert magic == 0x21324356, hex(magic)
    return raw[8:8 + ln].decode("utf-8")

x0, x1, x2 = decode(b0), decode(b1), decode(b2)
save_json(os.path.join(EV, "echoboy-blob-default.xml.json"), {"xml": x0})
save_json(os.path.join(EV, "echoboy-blob-mix100.xml.json"), {"xml": x1})
with open(os.path.join(EV, "echoboy-blob-default.xml"), "w", encoding="utf-8") as f: f.write(x0)
with open(os.path.join(EV, "echoboy-blob-mix100.xml"), "w", encoding="utf-8") as f: f.write(x1)
print("=== default blob XML (first 1500 chars) ===")
print(x0[:1500])
print("=== mix100 blob XML (first 1500 chars) ===")
print(x1[:1500])
print("=== b1 == b2 (same live instance, consecutive captures):", b1 == b2)
print("=== diff default vs mix100 ===")
for line in difflib.unified_diff(x0.splitlines(), x1.splitlines(), lineterm="", n=0):
    print(line[:400])
if b1 != b2:
    print("=== diff b1 vs b2 (volatility) ===")
    for line in difflib.unified_diff(x1.splitlines(), x2.splitlines(), lineterm="", n=0):
        print(line[:400])
