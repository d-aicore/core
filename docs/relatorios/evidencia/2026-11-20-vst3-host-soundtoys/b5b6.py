import json, os, sys, time
sys.path.insert(0, r"D:\Core-build\vst3-host")
from engine_drive import Engine, ECHOBOY, WORK, EV, probe_song, note, save_json
EK = "fx:src:eb"
out = {}
eng = Engine("b5")
try:
    eng.cmd("engine.start", backend="null", sampleRate=48000, blockSize=512, listenDb=-15)
    song = probe_song(ECHOBOY, "eb", [note(0, 240, 60, 0.9)], name="b5")
    # B5: editor open -> param move WHILE open -> close -> capture -> restore
    eng.cmd("project.load", project=song)
    eng.cmd("plugin.editor", key=EK, open=True, timeout=120)
    time.sleep(1.5)
    eng.cmd("plugin.param", key=EK, id="3", value=0.8)
    time.sleep(0.5)
    eng.cmd("plugin.editor", key=EK, open=False, timeout=120)
    b5 = eng.cmd("plugins.states", keys=[EK])
    eng.cmd("plugins.release")
    eng.cmd("project.load", project=song, states=b5)
    p = eng.cmd("plugin.params", key=EK)
    out["B5_move_while_open_close_then_capture"] = next(x for x in p if x["name"] == "Mix")["text"]
    # B6: editor open -> param move -> capture WHILE open -> close -> restore (debug3 replication)
    eng.cmd("plugins.release")
    eng.cmd("project.load", project=song)
    eng.cmd("plugin.editor", key=EK, open=True, timeout=120)
    time.sleep(1.5)
    eng.cmd("plugin.param", key=EK, id="3", value=0.8)
    time.sleep(0.5)
    b6 = eng.cmd("plugins.states", keys=[EK])
    eng.cmd("plugin.editor", key=EK, open=False, timeout=120)
    eng.cmd("plugins.release")
    eng.cmd("project.load", project=song, states=b6)
    p = eng.cmd("plugin.params", key=EK)
    out["B6_capture_while_open"] = next(x for x in p if x["name"] == "Mix")["text"]
    out["B5_vs_B6_blob_identical"] = b5[EK] == b6[EK]
finally:
    out["engine_exit"] = eng.close()
save_json(os.path.join(EV, "boundary-b5b6.json"), out)
print(json.dumps(out, indent=1))
