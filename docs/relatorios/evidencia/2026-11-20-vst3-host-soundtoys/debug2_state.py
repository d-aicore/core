"""Discriminate: CoreTestGain control vs EchoBoy state restore; async-restore timing; blob volatility."""
import json, os, sys, time, subprocess
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from engine_drive import (Engine, EXE, ECHOBOY, WORK, EV, probe_song, note, save_json,
                          sha256_text, last_above, echo_peaks)

GAIN_FILE = r"D:\Core-build\gate-round1d\app\selftest-plugins\CoreTestGain_artefacts\Release\VST3\Core Test Gain.vst3"
scan = json.loads(subprocess.run([EXE, "scan", GAIN_FILE], capture_output=True, text=True).stdout)
GAIN = scan[0]
out = {"gain_scan": scan}
KEY = "fx:src:eb"
GKEY = "fx:src:g"
eng = Engine("debug2")
try:
    eng.cmd("engine.start", backend="null", sampleRate=48000, blockSize=512, listenDb=-15)

    # --- control: CoreTestGain ---
    gsong = probe_song(GAIN, "g", [note(0, 960, 60, 0.9)], name="gain probe")
    eng.cmd("project.load", project=gsong)
    gp = eng.cmd("plugin.params", key=GKEY)
    out["gain_params"] = gp
    gid = gp[0]["id"]
    eng.cmd("plugin.param", key=GKEY, id=gid, value=0.25)
    gs1 = eng.cmd("plugins.states", keys=[GKEY])
    gs1b = eng.cmd("plugins.states", keys=[GKEY])
    out["gain_blob_stable_live"] = gs1[GKEY] == gs1b[GKEY]
    eng.cmd("plugins.release")
    eng.cmd("project.load", project=gsong, states=gs1)
    gp2 = eng.cmd("plugin.params", key=GKEY)
    gs2 = eng.cmd("plugins.states", keys=[GKEY])
    out["gain_restore"] = {"paramAfter": gp2[0], "bitIdentical": gs1[GKEY] == gs2[GKEY],
                           "sha1": sha256_text(gs1[GKEY]), "sha2": sha256_text(gs2[GKEY])}

    # --- EchoBoy: fresh capture, volatility, restore timing ---
    esong = probe_song(ECHOBOY, "eb", [note(0, 240, 60, 0.9)], name="eb probe")
    eng.cmd("project.load", project=esong)
    eng.cmd("plugin.param", key=KEY, id="3", value=1.0)
    eng.cmd("plugin.param", key=KEY, id="11", value=0.6)
    es1 = eng.cmd("plugins.states", keys=[KEY])
    es1b = eng.cmd("plugins.states", keys=[KEY])
    out["eb_blob_stable_live"] = es1[KEY] == es1b[KEY]
    save_json(os.path.join(WORK, "eb-state-mix1-fb06.json"), es1)
    eng.cmd("plugins.release")
    eng.cmd("project.load", project=esong, states=es1)
    def snap():
        p = eng.cmd("plugin.params", key=KEY)
        return {"Mix": next(x for x in p if x["name"] == "Mix")["text"],
                "Feedback": next(x for x in p if x["name"] == "Feedback")["text"]}
    out["eb_restore_t0"] = snap()
    time.sleep(1.0); out["eb_restore_t1"] = snap()
    time.sleep(2.0); out["eb_restore_t3"] = snap()
    es2 = eng.cmd("plugins.states", keys=[KEY])
    out["eb_restore_bitIdentical"] = es1[KEY] == es2[KEY]
    r = eng.cmd("render", path=os.path.join(WORK, "dbg2-tail.wav"), bits=24, tailMs=6000, timeout=600)
    out["eb_restore_render"] = {"last": last_above(os.path.join(WORK, "dbg2-tail.wav"), 1e-4),
                                "peaks": echo_peaks(os.path.join(WORK, "dbg2-tail.wav"), 10, 120, 0.05)}

    # --- EchoBoy: does a SECOND release/reload of the recaptured blob restore? ---
    eng.cmd("plugins.release")
    eng.cmd("project.load", project=esong, states=es2)
    out["eb_restore2_t0"] = snap()
    time.sleep(2.0); out["eb_restore2_t2"] = snap()
finally:
    out["engine_exit"] = eng.close()
save_json(os.path.join(EV, "echoboy-debug2.json"), out)
print(json.dumps(out, indent=1))
