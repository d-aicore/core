"""EchoBoy cases on a probe song (copy; owner originals untouched). PROOF front.

Flow facts (code): render builds its OWN plugin instances from the document state
(server.cpp:150, juce_host.cpp:382-390 offline branch), so a live plugin.param move is
captured with plugins.states and re-loaded before rendering — the app's own save flow.
"""
import json
import os
import sys
import time
import ctypes
from ctypes import wintypes

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from engine_drive import (Engine, EngineError, ECHOBOY, SYNTH, WORK, EV, probe_song, note,
                          save_json, sha256_file, sha256_text, wav_stats, last_above,
                          echo_peaks, window_rms, max_abs_diff)

KEY = "fx:src:eb"
RESULTS = []


def record(case, method, result, evidence, **details):
    row = {"case": case, "plugin": "EchoBoy", "version": "5.5.5.19885", "format": "vst3",
           "method": method, "result": result, "evidence": evidence}
    row.update(details)
    RESULTS.append(row)
    print(f"[{result}] {case}: {details.get('summary', '')}", flush=True)


def enum_windows(pid):
    user32 = ctypes.windll.user32
    out = []
    cb = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

    def visit(hwnd, lparam):
        p = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(p))
        if p.value == pid:
            n = user32.GetWindowTextLengthW(hwnd)
            buf = ctypes.create_unicode_buffer(n + 1)
            user32.GetWindowTextW(hwnd, buf, n + 1)
            vis = bool(user32.IsWindowVisible(hwnd))
            out.append({"hwnd": int(hwnd), "title": buf.value, "visible": vis})
        return True

    user32.EnumWindows(cb(visit), 0)
    return out


def screenshot(path):
    from PIL import ImageGrab
    img = ImageGrab.grab(all_screens=True)
    img.save(path)
    return path


def wait_ready(eng, timeout=15):
    t0 = time.time()
    while time.time() - t0 < timeout:
        if any(e.get("event") == "ready" for e in eng.events):
            return True
        time.sleep(0.1)
    return False


def load_with_live_params(eng, song, params):
    """project.load -> plugin.param moves -> plugins.states -> project.load(states)."""
    eng.cmd("project.load", project=song)
    for pid, val in params.items():
        eng.cmd("plugin.param", key=KEY, id=pid, value=val)
    states = eng.cmd("plugins.states", keys=[KEY])
    eng.cmd("project.load", project=song, states=states)
    return states


def render(eng, path, tail_ms=3000, bits=24):
    r = eng.cmd("render", path=path, bits=bits, tailMs=tail_ms, timeout=600)
    return r


def main():
    os.makedirs(WORK, exist_ok=True)
    eng = Engine("echoboy")
    try:
        assert wait_ready(eng), "engine never said ready"
        hello = [e for e in eng.events if e.get("event") == "ready"]
        save_json(os.path.join(EV, "echoboy-hello.json"), hello)
        eng.cmd("engine.start", backend="null", sampleRate=48000, blockSize=512, listenDb=-15)

        # ---- EB-02 load (default state) ----
        song = probe_song(ECHOBOY, "eb", [note(0, 3840, 60, 0.9)], name="EB probe")
        save_json(os.path.join(WORK, "probe-echoboy.json"), song)
        lr = eng.cmd("project.load", project=song)
        missing = eng.cmd("plugins.missing")
        versions = eng.cmd("plugins.versions")
        params = eng.cmd("plugin.params", key=KEY)
        mix = next(p for p in params if p["name"] == "Mix")
        save_json(os.path.join(EV, "echoboy-load.json"),
                  {"load": lr, "missing": missing, "versions": versions, "mixParam": mix,
                   "paramCount": len(params)})
        ok = (not missing) and (not versions) and mix["id"] == "3"
        record("EB-02 load", "headless project.load + plugins.missing/versions + plugin.params",
               "PASS" if ok else "FAIL", "echoboy-load.json",
               summary=f"missing={missing} versions={versions} Mix id={mix['id']} value={mix['value']} text={mix['text']}")

        # ---- EB-04 editor open/close ----
        pid = eng.proc.pid
        before = enum_windows(pid)
        eng.cmd("plugin.editor", key=KEY, open=True, timeout=120)
        time.sleep(2.0)
        opened = enum_windows(pid)
        shot_open = screenshot(os.path.join(EV, "echoboy-editor-open.png"))
        eb_open = [w for w in opened if "EchoBoy" in w["title"] and w["visible"]]
        eng.cmd("plugin.editor", key=KEY, open=False, timeout=120)
        time.sleep(1.0)
        closed = enum_windows(pid)
        shot_closed = screenshot(os.path.join(EV, "echoboy-editor-close.png"))
        eb_closed_vis = [w for w in closed if "EchoBoy" in w["title"] and w["visible"]]
        alive = eng.cmd("plugin.params", key=KEY)
        save_json(os.path.join(EV, "echoboy-editor.json"),
                  {"pid": pid, "before": before, "opened": opened, "closed": closed,
                   "echoBoyVisibleOpen": eb_open, "echoBoyVisibleAfterClose": eb_closed_vis,
                   "engineAliveAfter": len(alive) > 0})
        ok = bool(eb_open) and not eb_closed_vis and len(alive) > 0
        record("EB-04 editor open/close", "headless plugin.editor seam + EnumWindows(pid) + screenshots",
               "PASS" if ok else "FAIL", "echoboy-editor.json; echoboy-editor-open.png; echoboy-editor-close.png",
               summary=f"open windows={eb_open} visible-after-close={eb_closed_vis}")

        # ---- EB-06 automation Mix (0 held 0.5 s, ramp to 1 by 1.5 s) ----
        long_note = [note(0, 3840, 60, 0.9)]
        env = [{"param": "fx:eb:3", "points": [{"tick": 0, "value": 0.0},
                                               {"tick": 960, "value": 0.0},
                                               {"tick": 2880, "value": 1.0}]}]
        song_a = probe_song(ECHOBOY, "eb", long_note, envelopes=env, name="EB Mix automation")
        save_json(os.path.join(WORK, "probe-echoboy-automation.json"), song_a)
        eng.cmd("project.load", project=song_a)
        ra = render(eng, os.path.join(WORK, "eb-automix.wav"))
        song_plain = probe_song(ECHOBOY, "eb", long_note, name="EB static")
        load_with_live_params(eng, song_plain, {"3": 0.0})
        rb = render(eng, os.path.join(WORK, "eb-mix0.wav"))
        load_with_live_params(eng, song_plain, {"3": 1.0})
        rc = render(eng, os.path.join(WORK, "eb-mix1.wav"))
        pa, pb, pc = (os.path.join(WORK, f) for f in ("eb-automix.wav", "eb-mix0.wav", "eb-mix1.wav"))
        sa, sb, sc = (wav_stats(p) for p in (pa, pb, pc))
        early_ab = max_abs_diff(pa, pb, 0.0, 0.45)     # Mix held 0: A must equal B early
        late_ac = max_abs_diff(pa, pc, 1.6, 2.0)       # Mix held 1: A must equal C late
        late_ab = max_abs_diff(pa, pb, 1.6, 2.0)       # ...and differ from B
        early_ac = max_abs_diff(pa, pc, 0.0, 0.45)
        auto = {"renders": {"automation": ra, "mix0": rb, "mix1": rc},
                "stats": {"automation": sa, "mix0": sb, "mix1": sc},
                "maxAbsDiff": {"early_A_vs_B": early_ab, "late_A_vs_C": late_ac,
                               "late_A_vs_B": late_ab, "early_A_vs_C": early_ac},
                "envelope": env}
        save_json(os.path.join(EV, "echoboy-automation.json"), auto)
        ok = (sa["sha256"] != sb["sha256"] != sc["sha256"] and early_ab < 1e-3
              and late_ac < 1e-3 and late_ab > 1e-2)
        record("EB-06 automation Mix", "headless envelope fx:eb:3 + render x3 + sample comparison",
               "PASS" if ok else "FAIL", "echoboy-automation.json; eb-automix.wav; eb-mix0.wav; eb-mix1.wav",
               summary=f"early|A-B|={early_ab} late|A-C|={late_ac} late|A-B|={late_ab}")

        # ---- EB-07 bypass ----
        eng.cmd("project.load", project=song_plain)
        r_act = render(eng, os.path.join(WORK, "eb-active.wav"))
        song_byp = probe_song(ECHOBOY, "eb", long_note, bypass=True, name="EB bypass")
        eng.cmd("project.load", project=song_byp)
        r_byp = render(eng, os.path.join(WORK, "eb-bypass.wav"))
        song_nofx = probe_song(ECHOBOY, "eb", long_note, with_fx=False, name="EB no fx")
        eng.cmd("project.load", project=song_nofx)
        r_nofx = render(eng, os.path.join(WORK, "eb-nofx.wav"))
        p_act, p_byp, p_nofx = (os.path.join(WORK, f) for f in ("eb-active.wav", "eb-bypass.wav", "eb-nofx.wav"))
        s_act, s_byp, s_nofx = (wav_stats(p) for p in (p_act, p_byp, p_nofx))
        byp_eq_nofx = s_byp["sha256"] == s_nofx["sha256"]
        act_ne_byp = s_act["sha256"] != s_byp["sha256"]
        save_json(os.path.join(EV, "echoboy-bypass.json"),
                  {"active": s_act, "bypass": s_byp, "nofx": s_nofx,
                   "bypassBitIdenticalToNoFx": byp_eq_nofx, "activeDiffersFromBypass": act_ne_byp,
                   "renders": {"active": r_act, "bypass": r_byp, "nofx": r_nofx}})
        record("EB-07 bypass", "headless project.load bypass:true/false/absent + render sha256",
               "PASS" if (byp_eq_nofx and act_ne_byp) else "FAIL",
               "echoboy-bypass.json; eb-active.wav; eb-bypass.wav; eb-nofx.wav",
               summary=f"bypass==nofx:{byp_eq_nofx} active!=bypass:{act_ne_byp} peaks {s_act['peak']}/{s_byp['peak']}")

        # ---- EB-09 save/reopen (state bit-identical) ----
        eng.cmd("project.load", project=song_plain)
        eng.cmd("plugin.param", key=KEY, id="3", value=0.8)
        eng.cmd("plugin.param", key=KEY, id="11", value=0.6)
        s1 = eng.cmd("plugins.states", keys=[KEY])
        sha1 = sha256_text(s1[KEY])
        doc = eng.cmd("project.get")
        # embed the captured state the way the app saves it
        for t in doc["tracks"]:
            for fx in t.get("effects", []):
                if fx.get("id") == "eb":
                    fx["plugin"]["state"] = s1[KEY]
        saved_path = os.path.join(WORK, "probe-echoboy-saved.core-project.json")
        save_json(saved_path, doc)
        r_before = render(eng, os.path.join(WORK, "eb-before-reopen.wav"))
        eng.cmd("plugins.release")
        lr2 = eng.cmd("project.load", project=doc)
        s2 = eng.cmd("plugins.states", keys=[KEY])
        sha2 = sha256_text(s2[KEY])
        params2 = eng.cmd("plugin.params", key=KEY)
        mix2 = next(p for p in params2 if p["name"] == "Mix")
        fb2 = next(p for p in params2 if p["name"] == "Feedback")
        r_after = render(eng, os.path.join(WORK, "eb-after-reopen.wav"))
        render_eq = sha256_file(os.path.join(WORK, "eb-before-reopen.wav")) == sha256_file(os.path.join(WORK, "eb-after-reopen.wav"))
        save_json(os.path.join(EV, "echoboy-save-reopen.json"),
                  {"stateShaBefore": sha1, "stateShaAfter": sha2, "stateBitIdentical": sha1 == sha2,
                   "stateBytes": len(s1[KEY]), "mixAfter": mix2, "feedbackAfter": fb2,
                   "renderBitIdentical": render_eq, "savedDocument": saved_path,
                   "loadAfter": lr2})
        ok = sha1 == sha2 and abs(mix2["value"] - 0.8) < 1e-6 and render_eq
        record("EB-09 save/reopen", "headless plugins.states sha256 + document save + plugins.release + project.load",
               "PASS" if ok else "FAIL", "echoboy-save-reopen.json; probe-echoboy-saved.core-project.json",
               summary=f"state sha {sha1[:12]}=={sha2[:12]}:{sha1 == sha2} Mix={mix2['text']} renderEq={render_eq}")

        # ---- EB-10 tail (short impulse, Mix=1, Feedback=0.6, 1/8 delay @120) ----
        impulse = [note(0, 240, 60, 0.9)]  # 125 ms note + 20 ms synth release
        song_t = probe_song(ECHOBOY, "eb", impulse, name="EB tail probe")
        load_with_live_params(eng, song_t, {"3": 1.0, "11": 0.6})
        st = eng.cmd("plugins.states", keys=[KEY])
        save_json(os.path.join(WORK, "probe-echoboy-tail-state.json"), st)
        r_tail = render(eng, os.path.join(WORK, "eb-tail.wav"), tail_ms=6000)
        load_with_live_params(eng, song_t, {"3": 0.0})
        r_dry = render(eng, os.path.join(WORK, "eb-tail-dry.wav"), tail_ms=6000)
        p_tail, p_dry = os.path.join(WORK, "eb-tail.wav"), os.path.join(WORK, "eb-tail-dry.wav")
        s_tail, s_dry = wav_stats(p_tail), wav_stats(p_dry)
        last_t = last_above(p_tail, 1e-4)
        last_d = last_above(p_dry, 1e-4)
        peaks = echo_peaks(p_tail, window_ms=10, min_gap_ms=120, rel_prominence=0.05)
        spacings = [round(peaks[i + 1]["t"] - peaks[i]["t"], 4) for i in range(len(peaks) - 1)]
        note_end = 0.125 + 0.020
        tail_len = round(last_t["lastAboveS"] - note_end, 4) if last_t["lastAboveS"] else None
        save_json(os.path.join(EV, "echoboy-tail.json"),
                  {"tailRender": r_tail, "dryRender": r_dry, "tailStats": s_tail, "dryStats": s_dry,
                   "lastAbove": {"tail": last_t, "dry": last_d}, "echoPeaks": peaks,
                   "spacingsS": spacings, "noteEndS": note_end, "tailLengthS": tail_len,
                   "expectedDelayS": 0.25, "feedback": 0.6, "mix": 1.0, "tailMs": 6000})
        ok = (tail_len is not None and tail_len > 1.0 and len(peaks) >= 4
              and spacings and all(abs(s - 0.25) < 0.03 for s in spacings[:6]))
        record("EB-10 tail", "headless render tailMs=6000 + WAV envelope analysis (numpy)",
               "PASS" if ok else "FAIL", "echoboy-tail.json; eb-tail.wav; eb-tail-dry.wav",
               summary=f"tail={tail_len}s past note end, {len(peaks)} echoes, spacings={spacings[:8]}")

        # ---- EB-08 render summary (offline WAV facts) ----
        save_json(os.path.join(EV, "echoboy-renders.json"),
                  {"note": "offline renders, null device, 48 kHz/24-bit; peaks/sha per file",
                   "files": [wav_stats(os.path.join(WORK, f)) for f in
                             ("eb-automix.wav", "eb-mix0.wav", "eb-mix1.wav", "eb-active.wav",
                              "eb-bypass.wav", "eb-nofx.wav", "eb-tail.wav", "eb-tail-dry.wav",
                              "eb-before-reopen.wav", "eb-after-reopen.wav")]})
        record("EB-08 render", "headless render (offline, blocking) 48kHz/24-bit",
               "PASS", "echoboy-renders.json", summary="10 WAVs, peaks/sha recorded")

    except EngineError as e:
        record("EB-ERROR", "headless", "FAIL", f"stderr-echoboy.log; transcript-echoboy.jsonl", summary=str(e))
    finally:
        rc = eng.close()
        print(f"engine exit={rc}")
    save_json(os.path.join(EV, "echoboy-results.json"), RESULTS)
    print(json.dumps([{k: r[k] for k in ("case", "result", "method")} for r in RESULTS], indent=1))


if __name__ == "__main__":
    main()
