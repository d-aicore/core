"""Debug EB-09/EB-10 state flow + redo EB-06 with the canonical fx:<id>:vst:<param> key."""
import json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from engine_drive import (Engine, ECHOBOY, WORK, EV, probe_song, note, save_json,
                          sha256_text, wav_stats, last_above, echo_peaks, max_abs_diff)

KEY = "fx:src:eb"
eng = Engine("debug")
out = {}
try:
    eng.cmd("engine.start", backend="null", sampleRate=48000, blockSize=512, listenDb=-15)
    song = probe_song(ECHOBOY, "eb", [note(0, 240, 60, 0.9)], name="debug")

    # 1) inline-state document (the EB-09 saved file), fresh engine
    doc = json.load(open(os.path.join(WORK, "probe-echoboy-saved.core-project.json"), encoding="utf-8"))
    lr = eng.cmd("project.load", project=doc)
    p = eng.cmd("plugin.params", key=KEY)
    out["inline_load"] = {"load": lr, "missing": eng.cmd("plugins.missing"),
                          "Mix": next(x for x in p if x["name"] == "Mix"),
                          "Feedback": next(x for x in p if x["name"] == "Feedback")}

    # 2) states-arg flow with live readback
    eng.cmd("project.load", project=song)
    eng.cmd("plugin.param", key=KEY, id="3", value=0.8)
    eng.cmd("plugin.param", key=KEY, id="11", value=0.6)
    p_live = eng.cmd("plugin.params", key=KEY)
    out["live_after_param"] = {"Mix": next(x for x in p_live if x["name"] == "Mix"),
                               "Feedback": next(x for x in p_live if x["name"] == "Feedback")}
    s1 = eng.cmd("plugins.states", keys=[KEY])
    eng.cmd("project.load", project=song, states=s1)
    p2 = eng.cmd("plugin.params", key=KEY)
    s2 = eng.cmd("plugins.states", keys=[KEY])
    out["states_arg_flow"] = {"Mix": next(x for x in p2 if x["name"] == "Mix"),
                              "Feedback": next(x for x in p2 if x["name"] == "Feedback"),
                              "sha_s1": sha256_text(s1[KEY]), "sha_s2": sha256_text(s2[KEY]),
                              "bitIdentical": s1[KEY] == s2[KEY]}

    # 3) release + reload with states arg (the reopen flow)
    eng.cmd("plugins.release")
    lr3 = eng.cmd("project.load", project=song, states=s1)
    p3 = eng.cmd("plugin.params", key=KEY)
    s3 = eng.cmd("plugins.states", keys=[KEY])
    out["release_reopen_states_arg"] = {"load": lr3, "Mix": next(x for x in p3 if x["name"] == "Mix"),
                                        "Feedback": next(x for x in p3 if x["name"] == "Feedback"),
                                        "bitIdentical": s1[KEY] == s3[KEY]}

    # 4) release + reload inline doc again, readback
    eng.cmd("plugins.release")
    eng.cmd("project.load", project=doc)
    p4 = eng.cmd("plugin.params", key=KEY)
    out["release_reopen_inline"] = {"Mix": next(x for x in p4 if x["name"] == "Mix"),
                                    "Feedback": next(x for x in p4 if x["name"] == "Feedback")}

    # 5) tail render with the states-arg configured plugin (Mix 1, FB 0.6)
    eng.cmd("project.load", project=song)
    eng.cmd("plugin.param", key=KEY, id="3", value=1.0)
    eng.cmd("plugin.param", key=KEY, id="11", value=0.6)
    pt = eng.cmd("plugin.params", key=KEY)
    out["tail_params_live"] = {"Mix": next(x for x in pt if x["name"] == "Mix"),
                               "Feedback": next(x for x in pt if x["name"] == "Feedback")}
    st = eng.cmd("plugins.states", keys=[KEY])
    eng.cmd("project.load", project=song, states=st)
    r = eng.cmd("render", path=os.path.join(WORK, "dbg-tail.wav"), bits=24, tailMs=6000, timeout=600)
    out["tail_render"] = r
    out["tail_last"] = last_above(os.path.join(WORK, "dbg-tail.wav"), 1e-4)
    out["tail_peaks"] = echo_peaks(os.path.join(WORK, "dbg-tail.wav"), 10, 120, 0.05)

    # 6) automation with canonical key fx:eb:vst:3
    long_note = [note(0, 3840, 60, 0.9)]
    env = [{"param": "fx:eb:vst:3", "points": [{"tick": 0, "value": 0.0},
                                               {"tick": 960, "value": 0.0},
                                               {"tick": 2880, "value": 1.0}]}]
    song_a = probe_song(ECHOBOY, "eb", long_note, envelopes=env, name="EB auto vst")
    eng.cmd("project.load", project=song_a)
    pg = eng.cmd("project.get")
    out["auto_envelopes_in_project"] = pg["tracks"][0].get("envelopes")
    eng.cmd("render", path=os.path.join(WORK, "dbg-auto.wav"), bits=24, tailMs=3000, timeout=600)
    song_plain = probe_song(ECHOBOY, "eb", long_note, name="plain")
    eng.cmd("project.load", project=song_plain)
    eng.cmd("plugin.param", key=KEY, id="3", value=0.0)
    s0 = eng.cmd("plugins.states", keys=[KEY])
    eng.cmd("project.load", project=song_plain, states=s0)
    eng.cmd("render", path=os.path.join(WORK, "dbg-mix0.wav"), bits=24, tailMs=3000, timeout=600)
    eng.cmd("project.load", project=song_plain)
    eng.cmd("plugin.param", key=KEY, id="3", value=1.0)
    s1m = eng.cmd("plugins.states", keys=[KEY])
    eng.cmd("project.load", project=song_plain, states=s1m)
    eng.cmd("render", path=os.path.join(WORK, "dbg-mix1.wav"), bits=24, tailMs=3000, timeout=600)
    a, b, c = (os.path.join(WORK, f) for f in ("dbg-auto.wav", "dbg-mix0.wav", "dbg-mix1.wav"))
    out["auto_compare"] = {"early_A_B": max_abs_diff(a, b, 0, 0.45),
                           "late_A_C": max_abs_diff(a, c, 1.6, 2.0),
                           "late_A_B": max_abs_diff(a, b, 1.6, 2.0),
                           "sha": {f: wav_stats(p)["sha256"] for f, p in
                                   (("A", a), ("B", b), ("C", c))}}
finally:
    rc = eng.close()
    out["engine_exit"] = rc
save_json(os.path.join(EV, "echoboy-debug.json"), out)
print(json.dumps(out, indent=1))
