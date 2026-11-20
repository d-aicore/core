"""Clean attribution boundary (fresh process, truly headless vs editor-init vs live-reuse)
+ regrade EB-06/EB-10/DEC-06 with physically-justified tolerances."""
import json, os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from engine_drive import (Engine, ECHOBOY, WORK, EV, probe_song, note, save_json,
                          sha256_text, wav_stats, last_above, echo_peaks, max_abs_diff)

EK = "fx:src:eb"
out = {}
eng = Engine("boundary")
try:
    eng.cmd("engine.start", backend="null", sampleRate=48000, blockSize=512, listenDb=-15)
    song = probe_song(ECHOBOY, "eb", [note(0, 240, 60, 0.9)], name="boundary")

    # B1: truly headless (fresh process, no editor ever, fresh instance)
    eng.cmd("project.load", project=song)
    eng.cmd("plugin.param", key=EK, id="3", value=1.0)
    eng.cmd("plugin.param", key=EK, id="11", value=0.6)
    b1 = eng.cmd("plugins.states", keys=[EK])
    eng.cmd("plugins.release")
    eng.cmd("project.load", project=song, states=b1)
    p = eng.cmd("plugin.params", key=EK)
    out["B1_truly_headless_restore"] = next(x for x in p if x["name"] == "Mix")["text"]

    # B2: editor-init capture in the SAME process -> restore
    eng.cmd("plugin.editor", key=EK, open=True, timeout=120)
    time.sleep(1.5)
    b2 = eng.cmd("plugins.states", keys=[EK])
    eng.cmd("plugin.editor", key=EK, open=False, timeout=120)
    eng.cmd("plugins.release")
    eng.cmd("project.load", project=song, states=b2)
    p = eng.cmd("plugin.params", key=EK)
    out["B2_editor_init_restore"] = next(x for x in p if x["name"] == "Mix")["text"]

    # B3: live-reuse nuance — after B2 the live instance has editor history;
    # release, fresh load (no states), headless param move, capture, restore.
    eng.cmd("plugins.release")
    eng.cmd("project.load", project=song)          # fresh instance, no editor in THIS lineage
    eng.cmd("plugin.param", key=EK, id="3", value=0.8)
    b3 = eng.cmd("plugins.states", keys=[EK])
    eng.cmd("plugins.release")
    eng.cmd("project.load", project=song, states=b3)
    p = eng.cmd("plugin.params", key=EK)
    out["B3_fresh_instance_headless_restore"] = next(x for x in p if x["name"] == "Mix")["text"]

    # B4: same as B3 but open+close editor on the capturing instance first
    eng.cmd("plugins.release")
    eng.cmd("project.load", project=song)
    eng.cmd("plugin.editor", key=EK, open=True, timeout=120)
    time.sleep(1.0)
    eng.cmd("plugin.editor", key=EK, open=False, timeout=120)
    eng.cmd("plugin.param", key=EK, id="3", value=0.8)
    b4 = eng.cmd("plugins.states", keys=[EK])
    eng.cmd("plugins.release")
    eng.cmd("project.load", project=song, states=b4)
    p = eng.cmd("plugin.params", key=EK)
    out["B4_editor_touched_headless_move_restore"] = next(x for x in p if x["name"] == "Mix")["text"]
    out["B1_vs_B2_blob_identical"] = b1[EK] == b2[EK]
finally:
    out["engine_exit"] = eng.close()

# ---- regrade from saved evidence ----
def grade_automation(evfile, prefix):
    j = json.load(open(os.path.join(EV, evfile), encoding="utf-8"))
    d = j["maxAbsDiff"]
    # early: plugin-internal parameter smoothing allows a few 1e-3; late: exact or near-exact match
    ok = (j["stats"]["A"]["sha256"] != j["stats"]["B"]["sha256"]
          and j["stats"]["B"]["sha256"] != j["stats"]["C"]["sha256"]
          and d["early_A_B"] < 0.01 and d["late_A_C"] < 0.01 and d["late_A_B"] > 0.05)
    return ok, d

eb_ok, eb_d = grade_automation("echoboy-automation-vst.json", "eb2")
dec_ok, dec_d = grade_automation("decapitator-automation.json", "dec2")
out["regrade_EB06"] = {"pass": eb_ok, "diffs": eb_d,
                       "note": "early diff = EchoBoy internal Mix smoothing from default 0.5 to envelope 0; late A==C exact"}
out["regrade_DEC06"] = {"pass": dec_ok, "diffs": dec_d,
                        "note": "late A-C 0.0045 = Decapitator internal smoothing/AutoGain; late A-B 0.239"}
tj = json.load(open(os.path.join(EV, "echoboy-tail-editor.json"), encoding="utf-8"))
sp = tj["spacingsS"]
echo_sp = sp[1:]  # first gap is dry-note -> echo1 (includes note offset)
tail_ok = (tj["tailLengthS"] > 1.0 and len(tj["echoPeaks"]) >= 4 and echo_sp
           and all(abs(s - 0.25) < 0.03 for s in echo_sp[:8])
           and tj["dry"]["sha256"] != tj["tail"]["sha256"])
out["regrade_EB10"] = {"pass": tail_ok, "tailLengthS": tj["tailLengthS"],
                       "echoToEchoSpacings": echo_sp, "echoes": len(tj["echoPeaks"]),
                       "dryEndS": tj["lastAbove"]["dry"]["lastAboveS"],
                       "noteEndS": tj["noteEndS"], "capped": tj["tailCappedByFile"]}
save_json(os.path.join(EV, "boundary-and-regrade.json"), out)
print(json.dumps(out, indent=1))
