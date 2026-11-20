"""Headless round 2: EB-06 automation (canonical vst: key), EB-10 tail (editor-init state),
state-attribution boundary tests, full Decapitator suite. PROOF front, no product edits."""
import json, os, sys, time, ctypes
from ctypes import wintypes
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from engine_drive import (Engine, ECHOBOY, DECAP, WORK, EV, probe_song, note, save_json,
                          sha256_text, sha256_file, wav_stats, last_above, echo_peaks,
                          max_abs_diff, window_rms)

RESULTS = []
def record(case, plugin, method, result, evidence, **d):
    row = {"case": case, "plugin": plugin, "version": "5.5.5.19885", "format": "vst3",
           "method": method, "result": result, "evidence": evidence}
    row.update(d); RESULTS.append(row)
    print(f"[{result}] {case}: {d.get('summary','')}", flush=True)

def enum_windows(pid):
    u = ctypes.windll.user32; out = []
    cb = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def visit(h, l):
        p = wintypes.DWORD(); u.GetWindowThreadProcessId(h, ctypes.byref(p))
        if p.value == pid:
            n = u.GetWindowTextLengthW(h); b = ctypes.create_unicode_buffer(n + 1)
            u.GetWindowTextW(h, b, n + 1)
            out.append({"hwnd": int(h), "title": b.value, "visible": bool(u.IsWindowVisible(h))})
        return True
    u.EnumWindows(cb(visit), 0); return out

def screenshot(path):
    from PIL import ImageGrab
    ImageGrab.grab(all_screens=True).save(path); return path

def editor_init_capture(eng, key, song, params):
    """load -> open editor (initializes Soundtoys serialization) -> set params -> capture -> reload(states)."""
    eng.cmd("project.load", project=song)
    eng.cmd("plugin.editor", key=key, open=True, timeout=120)
    time.sleep(1.5)
    for pid, v in params.items():
        eng.cmd("plugin.param", key=key, id=pid, value=v)
    time.sleep(0.5)
    st = eng.cmd("plugins.states", keys=[key])
    eng.cmd("plugin.editor", key=key, open=False, timeout=120)
    eng.cmd("project.load", project=song, states=st)
    return st

def render(eng, name, tail=3000):
    p = os.path.join(WORK, name)
    r = eng.cmd("render", path=p, bits=24, tailMs=tail, timeout=600)
    return p, r

eng = Engine("round2")
try:
    eng.cmd("engine.start", backend="null", sampleRate=48000, blockSize=512, listenDb=-15)
    EK, DK = "fx:src:eb", "fx:src:dec"
    long_note = [note(0, 3840, 60, 0.9)]
    impulse = [note(0, 240, 60, 0.9)]
    eb_plain = probe_song(ECHOBOY, "eb", long_note, name="EB plain")
    dec_plain = probe_song(DECAP, "dec", long_note, name="DEC plain")

    # ================= EB-06 automation Mix, canonical key =================
    env = [{"param": "fx:eb:vst:3", "points": [{"tick": 0, "value": 0.0},
                                               {"tick": 960, "value": 0.0},
                                               {"tick": 2880, "value": 1.0}]}]
    eb_auto = probe_song(ECHOBOY, "eb", long_note, envelopes=env, name="EB Mix automation vst")
    save_json(os.path.join(WORK, "probe-echoboy-automation-vst.json"), eb_auto)
    eng.cmd("project.load", project=eb_auto)
    pa, ra = render(eng, "eb2-automix.wav")
    editor_init_capture(eng, EK, eb_plain, {"3": 0.0})
    pb, rb = render(eng, "eb2-mix0.wav")
    editor_init_capture(eng, EK, eb_plain, {"3": 1.0})
    pc, rc = render(eng, "eb2-mix1.wav")
    sa, sb, sc = (wav_stats(p) for p in (pa, pb, pc))
    early_ab = max_abs_diff(pa, pb, 0.0, 0.45)
    late_ac = max_abs_diff(pa, pc, 1.6, 2.0)
    late_ab = max_abs_diff(pa, pb, 1.6, 2.0)
    save_json(os.path.join(EV, "echoboy-automation-vst.json"),
              {"envelope": env, "stats": {"A": sa, "B": sb, "C": sc},
               "maxAbsDiff": {"early_A_B": early_ab, "late_A_C": late_ac, "late_A_B": late_ab},
               "renders": {"A": ra, "B": rb, "C": rc}})
    ok = (sa["sha256"] != sb["sha256"] and sb["sha256"] != sc["sha256"]
          and early_ab < 1e-3 and late_ac < 1e-3 and late_ab > 1e-2)
    record("EB-06 automation Mix (redo)", "EchoBoy",
           "headless envelope fx:eb:vst:3 + editor-init static refs + render x3 + sample compare",
           "PASS" if ok else "FAIL", "echoboy-automation-vst.json; eb2-automix.wav; eb2-mix0.wav; eb2-mix1.wav",
           summary=f"early|A-B|={early_ab} late|A-C|={late_ac} late|A-B|={late_ab}")

    # ================= EB-10 tail, editor-init state =================
    eb_imp = probe_song(ECHOBOY, "eb", impulse, name="EB tail")
    st = editor_init_capture(eng, EK, eb_imp, {"3": 1.0, "11": 0.6})
    save_json(os.path.join(WORK, "eb-tail-state-editor.json"), st)
    ptail, rtail = render(eng, "eb2-tail.wav", tail=8000)
    pst = eng.cmd("plugin.params", key=EK)
    mixt = next(x for x in pst if x["name"] == "Mix"); fbt = next(x for x in pst if x["name"] == "Feedback")
    editor_init_capture(eng, EK, eb_imp, {"3": 0.0})
    pdry, rdry = render(eng, "eb2-tail-dry.wav", tail=8000)
    editor_init_capture(eng, EK, eb_imp, {"3": 1.0, "11": 0.0})
    pfb0, rfb0 = render(eng, "eb2-tail-fb0.wav", tail=8000)
    s_tail, s_dry, s_fb0 = (wav_stats(p) for p in (ptail, pdry, pfb0))
    last_t, last_d, last_f = (last_above(p, 1e-4) for p in (ptail, pdry, pfb0))
    peaks = echo_peaks(ptail, 10, 120, 0.03)
    spacings = [round(peaks[i+1]["t"] - peaks[i]["t"], 4) for i in range(len(peaks)-1)]
    note_end = 0.145
    tail_len = round(last_t["lastAboveS"] - note_end, 4) if last_t["lastAboveS"] else None
    capped = last_t["lastAboveS"] is not None and last_t["lastAboveS"] >= s_tail["seconds"] - 0.05
    save_json(os.path.join(EV, "echoboy-tail-editor.json"),
              {"mixAfterReload": mixt, "feedbackAfterReload": fbt,
               "tail": s_tail, "dry": s_dry, "fb0": s_fb0,
               "lastAbove": {"tail": last_t, "dry": last_d, "fb0": last_f},
               "echoPeaks": peaks, "spacingsS": spacings, "noteEndS": note_end,
               "tailLengthS": tail_len, "tailCappedByFile": capped,
               "expectedDelayS": 0.25, "renders": {"tail": rtail, "dry": rdry, "fb0": rfb0}})
    ok = (tail_len is not None and tail_len > 1.0 and len(peaks) >= 4
          and spacings and all(abs(s - 0.25) < 0.03 for s in spacings[:8]))
    record("EB-10 tail (redo, editor-init)", "EchoBoy",
           "headless editor-init state Mix=1 FB=0.6 + render tailMs=8000 + envelope analysis",
           "PASS" if ok else "FAIL", "echoboy-tail-editor.json; eb2-tail.wav; eb2-tail-dry.wav; eb2-tail-fb0.wav",
           summary=f"tail={tail_len}s capped={capped} echoes={len(peaks)} spacings={spacings[:8]} dryEnd={last_d['lastAboveS']}")

    # ================= Attribution boundary =================
    # C1: headless capture -> restore fails -> open editor on RESTORING instance -> recall?
    eng.cmd("project.load", project=eb_imp)
    eng.cmd("plugin.param", key=EK, id="3", value=1.0)
    eng.cmd("plugin.param", key=EK, id="11", value=0.6)
    hblob = eng.cmd("plugins.states", keys=[EK])
    eng.cmd("plugins.release")
    eng.cmd("project.load", project=eb_imp, states=hblob)
    p0 = eng.cmd("plugin.params", key=EK)
    c1_before = next(x for x in p0 if x["name"] == "Mix")["text"]
    eng.cmd("plugin.editor", key=EK, open=True, timeout=120)
    time.sleep(2.5)
    p1 = eng.cmd("plugin.params", key=EK)
    c1_after = next(x for x in p1 if x["name"] == "Mix")["text"]
    # C2: recapture after editor init on restoring instance -> restore without editor?
    hblob2 = eng.cmd("plugins.states", keys=[EK])
    eng.cmd("plugin.editor", key=EK, open=False, timeout=120)
    eng.cmd("plugins.release")
    eng.cmd("project.load", project=eb_imp, states=hblob2)
    p2 = eng.cmd("plugin.params", key=EK)
    c2_mix = next(x for x in p2 if x["name"] == "Mix")["text"]
    save_json(os.path.join(EV, "echoboy-attribution-boundary.json"),
              {"C1_headless_restore_before_editor": c1_before,
               "C1_after_editor_on_restoring_instance": c1_after,
               "C2_recapture_after_editor_restore_without_editor": c2_mix,
               "headlessBlobSha": sha256_text(hblob[KK := EK]),
               "recapturedBlobSha": sha256_text(hblob2[KK]),
               "blobsIdentical": hblob[KK] == hblob2[KK]})
    record("EB-11 attribution boundary", "EchoBoy",
           "headless C1 restore+editor-on-restore, C2 recapture->restore",
           "MEASURED", "echoboy-attribution-boundary.json",
           summary=f"C1 before={c1_before} afterEditor={c1_after}; C2 recapture-restore={c2_mix}")

    # ================= Decapitator suite =================
    eng.cmd("project.load", project=dec_plain)
    lr = eng.cmd("plugins.missing"); vr = eng.cmd("plugins.versions")
    dp = eng.cmd("plugin.params", key=DK)
    drive = next(x for x in dp if x["name"] == "Drive")
    save_json(os.path.join(EV, "decapitator-load.json"),
              {"missing": lr, "versions": vr, "driveParam": drive, "paramCount": len(dp)})
    record("DEC-02 load", "Decapitator", "headless project.load + plugins.missing/versions + plugin.params",
           "PASS" if (not lr and not vr and drive["id"] == "2") else "FAIL", "decapitator-load.json",
           summary=f"missing={lr} Drive id={drive['id']} value={drive['value']}")

    pid = eng.proc.pid
    eng.cmd("plugin.editor", key=DK, open=True, timeout=120)
    time.sleep(2.0)
    opened = enum_windows(pid)
    screenshot(os.path.join(EV, "decapitator-editor-open.png"))
    vis_open = [w for w in opened if "Decapitator" in w["title"] and w["visible"]]
    eng.cmd("plugin.editor", key=DK, open=False, timeout=120)
    time.sleep(1.0)
    closed = enum_windows(pid)
    vis_closed = [w for w in closed if "Decapitator" in w["title"] and w["visible"]]
    alive = eng.cmd("plugin.params", key=DK)
    save_json(os.path.join(EV, "decapitator-editor.json"),
              {"opened": opened, "closed": closed, "visibleOpen": vis_open,
               "visibleAfterClose": vis_closed, "engineAlive": len(alive) > 0})
    record("DEC-04 editor open/close", "Decapitator", "headless plugin.editor + EnumWindows(pid) + screenshots",
           "PASS" if (vis_open and not vis_closed and alive) else "FAIL",
           "decapitator-editor.json; decapitator-editor-open.png",
           summary=f"open={vis_open} afterClose={vis_closed}")

    # DEC-06 automation Drive
    denv = [{"param": "fx:dec:vst:2", "points": [{"tick": 0, "value": 0.0},
                                                 {"tick": 960, "value": 0.0},
                                                 {"tick": 2880, "value": 1.0}]}]
    dec_auto = probe_song(DECAP, "dec", long_note, envelopes=denv, name="DEC Drive automation")
    save_json(os.path.join(WORK, "probe-decapitator-automation.json"), dec_auto)
    eng.cmd("project.load", project=dec_auto)
    da, dra = render(eng, "dec2-autodrive.wav")
    editor_init_capture(eng, DK, dec_plain, {"2": 0.0})
    db, drb = render(eng, "dec2-drive0.wav")
    editor_init_capture(eng, DK, dec_plain, {"2": 1.0})
    dc, drc = render(eng, "dec2-drive1.wav")
    sda, sdb, sdc = (wav_stats(p) for p in (da, db, dc))
    d_early = max_abs_diff(da, db, 0.0, 0.45)
    d_late_ac = max_abs_diff(da, dc, 1.6, 2.0)
    d_late_ab = max_abs_diff(da, db, 1.6, 2.0)
    save_json(os.path.join(EV, "decapitator-automation.json"),
              {"envelope": denv, "stats": {"A": sda, "B": sdb, "C": sdc},
               "maxAbsDiff": {"early_A_B": d_early, "late_A_C": d_late_ac, "late_A_B": d_late_ab}})
    ok = (sda["sha256"] != sdb["sha256"] and sdb["sha256"] != sdc["sha256"]
          and d_early < 1e-3 and d_late_ac < 1e-3 and d_late_ab > 1e-2)
    record("DEC-06 automation Drive", "Decapitator",
           "headless envelope fx:dec:vst:2 + editor-init static refs + render x3 + sample compare",
           "PASS" if ok else "FAIL", "decapitator-automation.json; dec2-autodrive.wav; dec2-drive0.wav; dec2-drive1.wav",
           summary=f"early|A-B|={d_early} late|A-C|={d_late_ac} late|A-B|={d_late_ab}")

    # DEC-07 bypass
    eng.cmd("project.load", project=dec_plain)
    pa2, _ = render(eng, "dec2-active.wav")
    eng.cmd("project.load", project=probe_song(DECAP, "dec", long_note, bypass=True, name="DEC bypass"))
    pb2, _ = render(eng, "dec2-bypass.wav")
    eng.cmd("project.load", project=probe_song(DECAP, "dec", long_note, with_fx=False, name="DEC nofx"))
    pn2, _ = render(eng, "dec2-nofx.wav")
    sa2, sb2, sn2 = (wav_stats(p) for p in (pa2, pb2, pn2))
    save_json(os.path.join(EV, "decapitator-bypass.json"),
              {"active": sa2, "bypass": sb2, "nofx": sn2,
               "bypassBitIdenticalToNoFx": sb2["sha256"] == sn2["sha256"],
               "activeDiffers": sa2["sha256"] != sb2["sha256"]})
    record("DEC-07 bypass", "Decapitator", "headless bypass:true/false/absent + render sha256",
           "PASS" if (sb2["sha256"] == sn2["sha256"] and sa2["sha256"] != sb2["sha256"]) else "FAIL",
           "decapitator-bypass.json; dec2-active.wav; dec2-bypass.wav; dec2-nofx.wav",
           summary=f"bypass==nofx:{sb2['sha256']==sn2['sha256']} active!=bypass:{sa2['sha256']!=sb2['sha256']}")

    # DEC-09 state restore: headless vs editor-init
    eng.cmd("project.load", project=dec_plain)
    eng.cmd("plugin.param", key=DK, id="2", value=0.7)
    dh = eng.cmd("plugins.states", keys=[DK])
    dh2 = eng.cmd("plugins.states", keys=[DK])
    eng.cmd("plugins.release")
    eng.cmd("project.load", project=dec_plain, states=dh)
    ph = eng.cmd("plugin.params", key=DK)
    headless_drive = next(x for x in ph if x["name"] == "Drive")["text"]
    dst = editor_init_capture(eng, DK, dec_plain, {"2": 0.7})
    pe = eng.cmd("plugin.params", key=DK)
    editor_drive = next(x for x in pe if x["name"] == "Drive")["text"]
    eng.cmd("plugins.release")
    eng.cmd("project.load", project=dec_plain, states=dst)
    pr = eng.cmd("plugin.params", key=DK)
    reopen_drive = next(x for x in pr if x["name"] == "Drive")["text"]
    dst2 = eng.cmd("plugins.states", keys=[DK])
    save_json(os.path.join(EV, "decapitator-state.json"),
              {"headlessBlobStable": dh[DK] == dh2[DK],
               "headlessRestoreDrive": headless_drive,
               "editorInitDriveAfterReload": editor_drive,
               "editorInitReopenDrive": reopen_drive,
               "editorInitBitIdentical": dst[DK] == dst2[DK],
               "headlessSha": sha256_text(dh[DK]), "editorSha": sha256_text(dst[DK])})
    record("DEC-09 state save/reopen", "Decapitator",
           "headless plugins.states + release + load(states), headless vs editor-init capture",
           "PASS" if (reopen_drive == "7.0" and headless_drive == "0.0") else "MEASURED",
           "decapitator-state.json",
           summary=f"headlessRestore={headless_drive} editorInitReopen={reopen_drive} stable={dh[DK]==dh2[DK]}")

    save_json(os.path.join(EV, "decapitator-renders.json"),
              {"files": [wav_stats(os.path.join(WORK, f)) for f in
                         ("dec2-autodrive.wav", "dec2-drive0.wav", "dec2-drive1.wav",
                          "dec2-active.wav", "dec2-bypass.wav", "dec2-nofx.wav")]})
    record("DEC-08 render", "Decapitator", "headless render offline 48kHz/24-bit", "PASS",
           "decapitator-renders.json", summary="6 WAVs peaks/sha recorded")
finally:
    rc = eng.close()
    print("engine exit", rc)
save_json(os.path.join(EV, "round2-results.json"), RESULTS)
print(json.dumps([{k: r[k] for k in ("case", "result")} for r in RESULTS], indent=1))
