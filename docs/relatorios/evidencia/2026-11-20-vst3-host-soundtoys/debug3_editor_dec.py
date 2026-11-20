"""debug3: editor-initialized state capture; Decapitator restore control; volatility with editor."""
import base64, json, os, re, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from engine_drive import (Engine, ECHOBOY, DECAP, WORK, EV, probe_song, note, save_json, sha256_text)

def parts(b64):
    raw = base64.b64decode(b64)
    magic, ln = struct.unpack("<II", raw[:8])
    x = raw[8:8 + ln].decode("utf-8")
    comp = re.search(r"<IComponent>(.*?)</IComponent>", x, re.S)
    ctrl = re.search(r"<IEditController>(.*?)</IEditController>", x, re.S)
    return {"componentLen": len(comp.group(1)) if comp else 0,
            "controllerPresent": ctrl is not None,
            "controllerLen": len(ctrl.group(1)) if ctrl else 0,
            "xmlHead": x[:120]}

out = {}
eng = Engine("debug3")
try:
    eng.cmd("engine.start", backend="null", sampleRate=48000, blockSize=512, listenDb=-15)
    EK, DK = "fx:src:eb", "fx:src:dec"

    # --- EchoBoy with editor opened first ---
    esong = probe_song(ECHOBOY, "eb", [note(0, 240, 60, 0.9)], name="eb")
    eng.cmd("project.load", project=esong)
    eng.cmd("plugin.editor", key=EK, open=True, timeout=120)
    time.sleep(3.0)
    eng.cmd("plugin.param", key=EK, id="3", value=1.0)
    eng.cmd("plugin.param", key=EK, id="11", value=0.6)
    time.sleep(1.0)
    eb1 = eng.cmd("plugins.states", keys=[EK])[EK]
    eb2 = eng.cmd("plugins.states", keys=[EK])[EK]
    out["eb_with_editor"] = {"blob1": parts(eb1), "blob2": parts(eb2), "stable": eb1 == eb2}
    eng.cmd("plugin.editor", key=EK, open=False, timeout=120)
    save_json(os.path.join(WORK, "eb-state-with-editor.json"), {EK: eb1})
    eng.cmd("plugins.release")
    eng.cmd("project.load", project=esong, states={EK: eb1})
    p = eng.cmd("plugin.params", key=EK)
    out["eb_restore_after_editor"] = {"Mix": next(x for x in p if x["name"] == "Mix")["text"],
                                      "Feedback": next(x for x in p if x["name"] == "Feedback")["text"]}

    # --- Decapitator restore ---
    dsong = probe_song(DECAP, "dec", [note(0, 960, 60, 0.9)], name="dec")
    eng.cmd("project.load", project=dsong)
    dp = eng.cmd("plugin.params", key=DK)
    drive = next(x for x in dp if x["name"] == "Drive")
    eng.cmd("plugin.param", key=DK, id=drive["id"], value=0.7)
    d1 = eng.cmd("plugins.states", keys=[DK])[DK]
    d2 = eng.cmd("plugins.states", keys=[DK])[DK]
    out["dec_live"] = {"DriveText": next(x for x in eng.cmd("plugin.params", key=DK) if x["name"] == "Drive")["text"],
                       "blob": parts(d1), "stable": d1 == d2}
    save_json(os.path.join(WORK, "dec-state-drive07.json"), {DK: d1})
    eng.cmd("plugins.release")
    eng.cmd("project.load", project=dsong, states={DK: d1})
    p2 = eng.cmd("plugin.params", key=DK)
    d3 = eng.cmd("plugins.states", keys=[DK])[DK]
    out["dec_restore"] = {"Drive": next(x for x in p2 if x["name"] == "Drive"),
                          "bitIdentical": d1 == d3}
finally:
    out["engine_exit"] = eng.close()
save_json(os.path.join(EV, "debug3.json"), out)
print(json.dumps(out, indent=1))
