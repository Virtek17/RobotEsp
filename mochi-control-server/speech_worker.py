"""Persistent local Vosk worker; WAVs are passed in memory, never saved."""
import base64
import io
import json
import sys
import wave
from pathlib import Path

from vosk import KaldiRecognizer, Model, SetLogLevel

sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8")
SetLogLevel(-1)
model = Model(str(Path(__file__).parent / "models" / "vosk-model-small-ru-0.22"))
recognizer = KaldiRecognizer(model, 16000)
print(json.dumps({"ready": True}), flush=True)
for line in sys.stdin:
    try:
        request = json.loads(line)
        data = base64.b64decode(request["wav"], validate=True)
        with wave.open(io.BytesIO(data), "rb") as audio:
            if (audio.getnchannels(), audio.getsampwidth(), audio.getframerate()) != (1, 2, 16000):
                raise ValueError("Expected PCM WAV mono 16-bit 16000 Hz")
            if audio.getnframes() > 64000:
                raise ValueError("Recording exceeds 4 seconds")
            recognizer.Reset()
            fragments = []
            while block := audio.readframes(4000):
                if recognizer.AcceptWaveform(block):
                    fragments.append(json.loads(recognizer.Result()).get("text", ""))
            fragments.append(json.loads(recognizer.FinalResult()).get("text", ""))
        print(json.dumps({"text": " ".join(filter(None, fragments))}, ensure_ascii=False), flush=True)
    except Exception as exc:
        print(json.dumps({"error": str(exc)}, ensure_ascii=False), flush=True)
