"""Headless JSON-lines driver for core-engine serve (proof front, no product edits).

Protocol: engine/src/app/main.cpp serve() — request {id, cmd, args} -> {id, ok, result|error}.
Async events (state/job/ready/ended) are collected separately. Every request/response is
appended to a transcript JSONL for evidence.
"""
import json
import subprocess
import threading
import queue
import time
import hashlib
import os

EXE = r"D:\Core-build\gate-round1d\app\desktop\CoreApp_artefacts\Release\core-engine.exe"
WORK = r"D:\Core-build\vst3-host"
EV = r"D:\CORE\CORE\docs\relatorios\evidencia\2026-11-20-vst3-host-soundtoys"

SYNTH = {
    "format": "vst3",
    "id": "VST3-Core Test Synth-bc052b2-bc3b5f3e",
    "name": "Core Test Synth",
    "version": "1.0.0",
    "file": r"D:\Core-build\gate-round1d\app\selftest-plugins\CoreTestSynth_artefacts\Release\VST3\Core Test Synth.vst3",
}
ECHOBOY = {
    "format": "vst3",
    "id": "VST3-EchoBoy-d9085607-bc3ea0d0",
    "name": "EchoBoy",
    "version": "5.5.5.19885",
    "file": r"C:\Program Files\Common Files\VST3\Soundtoys\EchoBoy.vst3",
}
DECAP = {
    "format": "vst3",
    "id": "VST3-Decapitator-30e4085c-f0582f35",
    "name": "Decapitator",
    "version": "5.5.5.19885",
    "file": r"C:\Program Files\Common Files\VST3\Soundtoys\Decapitator.vst3",
}


class EngineError(RuntimeError):
    pass


class Engine:
    def __init__(self, tag):
        self.tag = tag
        self.transcript = open(os.path.join(WORK, f"transcript-{tag}.jsonl"), "w", encoding="utf-8")
        self.stderr = open(os.path.join(WORK, f"stderr-{tag}.log"), "w", encoding="utf-8")
        self.proc = subprocess.Popen(
            [EXE, "serve", "--backend", "null", "--plugin-log", os.path.join(WORK, f"plugin-log-{tag}.jsonl")],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.stderr,
            text=True, encoding="utf-8", bufsize=1, cwd=WORK)
        self._id = 0
        self._pending = {}
        self._lock = threading.Lock()
        self.events = []
        self.dead = False
        self.reader = threading.Thread(target=self._read, daemon=True)
        self.reader.start()

    def _read(self):
        for line in self.proc.stdout:
            line = line.strip()
            if not line:
                continue
            try:
                msg = json.loads(line)
            except json.JSONDecodeError:
                self.events.append({"event": "unparsed", "line": line})
                continue
            self.transcript.write(json.dumps({"dir": "in", "t": time.time(), "msg": msg}) + "\n")
            self.transcript.flush()
            if "id" in msg and isinstance(msg["id"], int):
                with self._lock:
                    q = self._pending.get(msg["id"])
                if q is not None:
                    q.put(msg)
            else:
                self.events.append(msg)
        self.dead = True

    def cmd(self, cmd, timeout=180, **args):
        if self.dead:
            raise EngineError("engine process stdout closed (crashed?)")
        with self._lock:
            self._id += 1
            mid = self._id
            q = queue.Queue()
            self._pending[mid] = q
        req = {"id": mid, "cmd": cmd, "args": args}
        self.transcript.write(json.dumps({"dir": "out", "t": time.time(), "msg": req}) + "\n")
        self.transcript.flush()
        self.proc.stdin.write(json.dumps(req) + "\n")
        self.proc.stdin.flush()
        try:
            resp = q.get(timeout=timeout)
        except queue.Empty:
            raise EngineError(f"timeout ({timeout}s) waiting for {cmd}")
        finally:
            with self._lock:
                self._pending.pop(mid, None)
        if not resp.get("ok"):
            raise EngineError(f"{cmd}: {resp.get('error')}")
        return resp.get("result")

    def try_cmd(self, cmd, timeout=180, **args):
        """Return (ok, result_or_error) without raising."""
        try:
            return True, self.cmd(cmd, timeout=timeout, **args)
        except EngineError as e:
            return False, str(e)

    def close(self):
        try:
            self.proc.stdin.close()
        except OSError:
            pass
        try:
            self.proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait(timeout=10)
        self.transcript.close()
        self.stderr.close()
        return self.proc.returncode


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def sha256_text(text):
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def note(tick, length, pitch, velocity):
    return {"tick": tick, "length": length, "pitch": pitch, "velocity": velocity}


def probe_song(fx_plugin, fx_id, notes, length_ticks=3840, bpm=120.0,
               envelopes=None, bypass=False, with_fx=True, name="vst3-host probe"):
    """core-project v2: one synth track, one plugin insert. patternTicks == lengthTicks
    so notes are absolute (no pattern repeat inside the render region)."""
    effects = []
    if with_fx:
        effects.append({"type": "plugin", "id": fx_id, "plugin": dict(fx_plugin), "bypass": bypass})
    track = {
        "id": "src", "name": "Probe source", "instrument": "plugin", "plugin": dict(SYNTH),
        "patternTicks": length_ticks, "lengthTicks": length_ticks,
        "notes": notes, "effects": effects,
    }
    if envelopes:
        track["envelopes"] = envelopes
    return {
        "format": "core-project", "version": 2, "name": name, "bpm": bpm,
        "lengthTicks": length_ticks, "masterGainDb": 0.0, "tracks": [track],
    }


def save_json(path, obj):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(obj, f, indent=1, ensure_ascii=False)
    return path


def load_wav(path):
    """Read 24/32-bit PCM or 32-float WAV -> (sr, float64 array [frames, channels] in -1..1)."""
    import wave
    import numpy as np
    with wave.open(path, "rb") as w:
        sr = w.getframerate()
        ch = w.getnchannels()
        sw = w.getsampwidth()
        n = w.getnframes()
        raw = w.readframes(n)
    if sw == 3:
        a = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        v = a[:, 0] | (a[:, 1] << 8) | (a[:, 2] << 16)
        v = np.where(v & 0x800000, v - (1 << 24), v)
        x = v.astype(np.float64) / 8388608.0
    elif sw == 4:
        # try float32 first: PCM int32 values are tiny as floats; detect via magnitude
        i = np.frombuffer(raw, dtype=np.int32)
        f = np.frombuffer(raw, dtype=np.float32)
        if np.max(np.abs(f)) <= 1.5 and np.max(np.abs(i.astype(np.float64))) > 1e6:
            x = f.astype(np.float64)
        else:
            x = i.astype(np.float64) / 2147483648.0
    elif sw == 2:
        x = np.frombuffer(raw, dtype=np.int16).astype(np.float64) / 32768.0
    else:
        raise ValueError(f"unsupported sample width {sw}")
    return sr, x.reshape(-1, ch)


def wav_stats(path):
    import numpy as np
    sr, x = load_wav(path)
    mono = x.mean(axis=1) if x.ndim > 1 else x
    peak = float(np.max(np.abs(x)))
    return {
        "path": path, "sha256": sha256_file(path), "sampleRate": sr,
        "frames": int(x.shape[0]), "channels": int(x.shape[1]) if x.ndim > 1 else 1,
        "seconds": round(x.shape[0] / sr, 4), "peak": round(peak, 6),
        "peakDb": round(20 * np.log10(max(peak, 1e-12)), 2),
        "rms": round(float(np.sqrt(np.mean(mono ** 2))), 6),
    }


def last_above(path, threshold=1e-4):
    """Time (s) of the last sample above threshold, and first after a given time."""
    import numpy as np
    sr, x = load_wav(path)
    m = np.max(np.abs(x), axis=1)
    idx = np.nonzero(m > threshold)[0]
    if len(idx) == 0:
        return {"threshold": threshold, "lastAboveS": None, "firstAboveS": None}
    return {"threshold": threshold, "lastAboveS": round(float(idx[-1]) / sr, 4),
            "firstAboveS": round(float(idx[0]) / sr, 4)}


def echo_peaks(path, window_ms=10.0, min_gap_ms=120.0, rel_prominence=0.15):
    """Envelope peak times (s): RMS in windows, smoothed, local maxima with a minimum gap."""
    import numpy as np
    sr, x = load_wav(path)
    m = np.max(np.abs(x), axis=1)
    w = max(1, int(sr * window_ms / 1000))
    n = len(m) // w
    env = np.sqrt(np.mean(m[: n * w].reshape(n, w) ** 2, axis=1))
    # smooth 3 windows
    k = np.ones(3) / 3
    sm = np.convolve(env, k, mode="same")
    peaks = []
    gap = int(min_gap_ms / window_ms)
    mx = float(np.max(sm)) if len(sm) else 0.0
    for i in range(1, len(sm) - 1):
        if sm[i] > sm[i - 1] and sm[i] >= sm[i + 1] and sm[i] > rel_prominence * mx:
            if not peaks or i - peaks[-1][0] >= gap:
                peaks.append((i, float(sm[i])))
            elif sm[i] > peaks[-1][1]:
                peaks[-1] = (i, float(sm[i]))
    return [{"t": round(i * window_ms / 1000, 4), "rms": round(v, 6)} for i, v in peaks]


def window_rms(path, start_s, end_s):
    import numpy as np
    sr, x = load_wav(path)
    seg = x[int(start_s * sr): int(end_s * sr)]
    if len(seg) == 0:
        return 0.0
    return round(float(np.sqrt(np.mean(seg ** 2))), 6)


def max_abs_diff(path_a, path_b, start_s=0.0, end_s=None):
    import numpy as np
    sra, a = load_wav(path_a)
    srb, b = load_wav(path_b)
    assert sra == srb
    n = min(len(a), len(b))
    s = int(start_s * sra)
    e = n if end_s is None else min(n, int(end_s * sra))
    if e <= s:
        return 0.0
    return round(float(np.max(np.abs(a[s:e] - b[s:e]))), 6)
