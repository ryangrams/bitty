#!/usr/bin/env python3
"""Serve the native Bitty Buddy engine as a private browser playtest."""

import argparse
import atexit
from http import cookies
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import secrets
import subprocess
import threading
import time

ROOT = Path(__file__).resolve().parents[1]
FRAME_BYTES = 135 * 240 * 2
MAX_SESSIONS = 6
IDLE_SECONDS = 900


class Session:
    def __init__(self, executable):
        self.process = subprocess.Popen([str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        stderr=subprocess.DEVNULL, bufsize=0)
        self.lock = threading.Lock()
        self.last_seen = time.monotonic()
        self.previous = self.last_seen
        self.remainder = 0.0
        self.first = True

    def close(self):
        self.process.terminate()
        try:
            self.process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
        self.process.stdin.close()
        self.process.stdout.close()

    def frame(self, data):
        with self.lock:
            now = time.monotonic()
            self.last_seen = now
            elapsed = min(now - self.previous, 0.15)
            self.previous = now
            paused = bool(data.get("paused", False))
            if paused:
                self.remainder = 0
            else:
                self.remainder += elapsed * 60
            steps = min(9, int(self.remainder))
            self.remainder -= steps
            restart = self.first or bool(data.get("restart", False))
            # A press always reaches the simulation, including on high refresh displays.
            if not paused and (data.get("pressed") or data.get("step")) and steps == 0:
                steps = 1
                self.remainder = 0
            self.first = False
            def integer(name, default, minimum, maximum):
                value = data.get(name, default)
                if type(value) is not int:
                    return default
                return max(minimum, min(maximum, value))
            command = [steps, integer("lane", -1, -1, 2), integer("step", 0, -1, 1),
                       int(bool(data.get("jump"))) if not paused else 0,
                       int(bool(data.get("roll"))) if not paused else 0,
                       int(bool(data.get("pressed"))) if not paused else 0,
                       int(restart), integer("level", 0, 0, 4), integer("shirt", 0, 0, 3),
                       integer("mask", 1, 0, 3), int(bool(data.get("home")))]
            self.process.stdin.write((" ".join(map(str, command)) + "\n").encode())
            result = bytearray()
            while len(result) < FRAME_BYTES:
                chunk = self.process.stdout.read(FRAME_BYTES - len(result))
                if not chunk:
                    raise RuntimeError("Native preview stopped")
                result.extend(chunk)
            return bytes(result)


class PreviewServer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, address, executable):
        super().__init__(address, Handler)
        self.executable = executable
        self.sessions = {}
        self.sessions_lock = threading.Lock()

    def session(self, token):
        with self.sessions_lock:
            now = time.monotonic()
            for old_token, session in list(self.sessions.items()):
                if now - session.last_seen > IDLE_SECONDS:
                    session.close()
                    del self.sessions[old_token]
            if token in self.sessions:
                return token, self.sessions[token]
            if len(self.sessions) >= MAX_SESSIONS:
                raise RuntimeError("All playtest slots are busy. Try again in a few minutes.")
            token = secrets.token_hex(24)
            session = self.sessions[token] = Session(self.executable)
            return token, session

    def close_sessions(self):
        with self.sessions_lock:
            for session in self.sessions.values():
                session.close()
            self.sessions.clear()


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_args):
        pass

    def send(self, code, body, content_type, token=None):
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Referrer-Policy", "no-referrer")
        self.send_header("Cross-Origin-Resource-Policy", "same-origin")
        self.send_header("Content-Security-Policy", "default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; img-src 'self' data:; connect-src 'self'; frame-ancestors 'none'")
        if token:
            self.send_header("Set-Cookie", "buddy=" + token + "; HttpOnly; SameSite=Strict; Path=/")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/":
            self.send(200, (ROOT / "web" / "buddy-preview.html").read_bytes(), "text/html; charset=utf-8")
        elif self.path == "/health":
            self.send(200, b"Bitty Buddy native preview\n", "text/plain; charset=utf-8")
        elif self.path == "/favicon.ico":
            self.send(204, b"", "image/x-icon")
        else:
            self.send(404, b"Page not found", "text/plain; charset=utf-8")

    def do_POST(self):
        if self.path != "/frame":
            self.send(404, b"Page not found", "text/plain")
            return
        origin = self.headers.get("Origin")
        if (origin and origin != "http://" + self.headers.get("Host", "")) or self.headers.get("Sec-Fetch-Site") == "cross-site":
            self.send(403, b"Open the playtest directly", "text/plain")
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if not 1 <= length <= 4096:
                raise ValueError("Invalid request length")
            data = json.loads(self.rfile.read(length))
            if not isinstance(data, dict):
                raise ValueError("Expected controls")
            jar = cookies.SimpleCookie()
            jar.load(self.headers.get("Cookie", ""))
            token, session = self.server.session(jar["buddy"].value if "buddy" in jar else "")
            self.send(200, session.frame(data), "application/octet-stream", token)
        except (ValueError, json.JSONDecodeError):
            self.send(400, b"Invalid controls", "text/plain")
        except (BrokenPipeError, ConnectionResetError):
            return
        except RuntimeError as error:
            self.send(503, str(error).encode(), "text/plain")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bind", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=4186)
    parser.add_argument("--executable", type=Path, default=ROOT / ".local" / "buddy-preview" / "buddy-host")
    args = parser.parse_args()
    if not args.executable.is_file():
        parser.error("Compile tools/buddy_host.cpp before starting the preview")
    server = PreviewServer((args.bind, args.port), args.executable)
    atexit.register(server.close_sessions)
    print("Bitty Buddy preview listening on http://" + args.bind + ":" + str(args.port), flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
