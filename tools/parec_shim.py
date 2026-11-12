#!/usr/bin/env python3
"""Stand-in for parec that replays a real recording instead of the mic.

Only ever placed first on PATH for the screenshot tool's own process, so the
Recorder's normal capture path runs unchanged: this writes the same raw
s16le mono 16 kHz stream parec would. parec's own flags are accepted and
ignored — the WAV to speak comes from VOX_SCREENSHOT_SPEECH, so nothing in
the Recorder's argument list has to be parsed.
"""
import os
import sys
import time
import wave

CHUNK_MS = 20
RATE = 16000


def main():
    path = os.environ.get("VOX_SCREENSHOT_SPEECH", "")
    if not path:
        print("parec_shim: set VOX_SCREENSHOT_SPEECH to a WAV path", file=sys.stderr)
        return 2

    # pi-lens-ignore: python-path-traversal
    with wave.open(path, "rb") as wav:
        if wav.getframerate() != RATE or wav.getnchannels() != 1 or wav.getsampwidth() != 2:
            print("parec_shim: need 16 kHz mono s16 WAV", file=sys.stderr)
            return 1
        speech = wav.readframes(wav.getnframes())

    out = sys.stdout.buffer
    frame_bytes = 2 * RATE * CHUNK_MS // 1000

    def emit(block):
        for start in range(0, len(block), frame_bytes):
            out.write(block[start:start + frame_bytes])
            out.flush()
            # pi-lens-ignore: python-sleep-in-test
            time.sleep(CHUNK_MS / 1000.0)

    try:
        # A beat of silence first: the Recorder's start-up and its first
        # evaluateClocks tick must not land before audio is flowing.
        emit(bytes(frame_bytes) * 5)
        emit(speech)
        # Silence past the recorder's gap threshold: the take ends on its own.
        silence = bytes(frame_bytes)
        while True:
            emit(silence)
    except (BrokenPipeError, KeyboardInterrupt):
        return 0
    return 0


if __name__ == "__main__":
    sys.exit(main())
