"""Diff editor-initialized vs headless EchoBoy blobs (same Mix/FB); export .vstpreset both ways."""
import base64, json, os, re, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from engine_drive import Engine, ECHOBOY, WORK, EV, probe_song, note, save_json

EK = "fx:src:eb"

def comp(b64):
    raw = base64.b64decode(b64)
    ln = struct.unpack("<I", raw[4:8])[0]
    x = raw[8:8 + ln].decode("utf-8")
    m = re.search(r"<IComponent>(.*?)</IComponent>", x, re.S)
    return m.group(1)

def first_diffs(a, b, limit=30):
    out = []
    n = min(len(a), len(b))
    i = 0
    while i < n and len(out) < limit:
        if a[i] != b[i]:
            j = i
            while j < n and a[j] != b[j]:
                j += 1
            out.append((i, a[max(0, i - 10):j + 10], b[max(0, i - 10):j + 10]))
            i = j
        else:
            i += 1
    return out, len(a), len(b)

headless = json.load(open(os.path.join(WORK, "eb-state-mix1-fb06.json"), encoding="utf-8"))[EK]
withed = json.load(open(os.path.join(WORK, "eb-state-with-editor.json"), encoding="utf-8"))[EK]
c_h, c_e = comp(headless), comp(withed)
diffs, lh, le = first_diffs(c_h, c_e)
out = {"lenHeadless": lh, "lenWithEditor": le, "diffCount": len(diffs),
       "diffs": [{"pos": p, "headless": a, "withEditor": b} for p, a, b in diffs]}

eng = Engine("preset")
try:
    eng.cmd("engine.start", backend="null", sampleRate=48000, blockSize=512, listenDb=-15)
    song = probe_song(ECHOBOY, "eb", [note(0, 240, 60, 0.9)], name="preset")
    # headless preset (no editor ever)
    eng.cmd("project.load", project=song)
    eng.cmd("plugin.param", key=EK, id="3", value=1.0)
    eng.cmd("plugin.param", key=EK, id="11", value=0.6)
    ph = eng.cmd("plugin.preset", key=EK)["preset"]
    open(os.path.join(EV, "echoboy-headless-mix100.vstpreset"), "wb").write(base64.b64decode(ph))
    # editor-initialized preset
    eng.cmd("plugin.editor", key=EK, open=True, timeout=120)
    time.sleep(2.0)
    pe = eng.cmd("plugin.preset", key=EK)["preset"]
    open(os.path.join(EV, "echoboy-editor-mix100.vstpreset"), "wb").write(base64.b64decode(pe))
    eng.cmd("plugin.editor", key=EK, open=False, timeout=120)
    out["presetBytes"] = {"headless": len(base64.b64decode(ph)), "editor": len(base64.b64decode(pe))}
    out["presetsIdentical"] = ph == pe
finally:
    out["engine_exit"] = eng.close()
save_json(os.path.join(EV, "echoboy-blob-editor-diff.json"), out)
print(json.dumps(out, indent=1)[:3000])
