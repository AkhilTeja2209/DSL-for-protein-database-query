#!/usr/bin/env python3
"""Barebones web front end for ProteinDSL.

The DSL is still compiled and executed by bin/proteindsl -- this server only
pipes a submitted program into that binary on stdin, asks for --json, and
hands the structured result back to the page. No query logic lives here.

Run it with:

    python server/app.py            # http://127.0.0.1:8000
    python server/app.py --port 9000

Only the Python standard library is used, so there is nothing to pip install.
"""

import argparse
import json
import os
import subprocess
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

# The page is shared with the WebAssembly build: `make wasm` copies web/ into
# docs/ and adds the wasm loader, so the same HTML/CSS/JS serves both the local
# server and GitHub Pages. app.js picks whichever backend is present.
STATIC_DIR = REPO_ROOT / "web"

# A submitted program is arbitrary text from whoever can reach the page, so the
# binary is run with its file access confined to dataset/ and with a hard time
# limit. LOAD UNIPROT stays enabled -- that is the point of the demo -- but it
# can only reach UniProt, because the executor builds that URL itself.
DATASET_ROOT = REPO_ROOT / "dataset"
MAX_PROGRAM_BYTES = 8 * 1024
RUN_TIMEOUT_SECONDS = 90

# The only files this server will hand out, and the type each is sent as.
STATIC_FILES = {
    "index.html": "text/html; charset=utf-8",
    "app.css": "text/css; charset=utf-8",
    "app.js": "application/javascript; charset=utf-8",
}


def binary_path() -> Path:
    """Locate the compiled interpreter, whichever extension it has."""
    for name in ("proteindsl", "proteindsl.exe"):
        candidate = REPO_ROOT / "bin" / name
        if candidate.exists():
            return candidate
    return REPO_ROOT / "bin" / "proteindsl"


def run_program(source: str) -> dict:
    """Execute one DSL program and return the interpreter's JSON document."""
    exe = binary_path()
    if not exe.exists():
        return {
            "ok": False,
            "stage": "runtime",
            "statements": 0,
            "ast": "",
            "errors": [
                f"The interpreter has not been built yet ({exe} is missing). "
                "Run `make` in the repository root first."
            ],
            "sources": [],
            "results": [],
        }

    cmd = [
        str(exe), "-", "--json",
        "--dataset-root", str(DATASET_ROOT),
        "--cache-dir", str(REPO_ROOT / ".cache"),
    ]

    try:
        proc = subprocess.run(
            cmd,
            input=source,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            cwd=str(REPO_ROOT),
            timeout=RUN_TIMEOUT_SECONDS,
        )
    except subprocess.TimeoutExpired:
        return {
            "ok": False,
            "stage": "runtime",
            "statements": 0,
            "ast": "",
            "errors": [f"The query took longer than {RUN_TIMEOUT_SECONDS}s and was stopped."],
            "sources": [],
            "results": [],
        }

    # In --json mode every phase, including failure, prints one JSON document.
    # Anything else means the binary died in a way it does not model, so the
    # raw stderr is surfaced rather than swallowed.
    try:
        payload = json.loads(proc.stdout)
    except (json.JSONDecodeError, ValueError):
        detail = (proc.stderr or proc.stdout or "").strip()
        return {
            "ok": False,
            "stage": "runtime",
            "statements": 0,
            "ast": "",
            "errors": [
                f"The interpreter exited with code {proc.returncode} and no JSON output."
            ] + ([detail] if detail else []),
            "sources": [],
            "results": [],
        }

    payload["exitCode"] = proc.returncode
    return payload


class Handler(BaseHTTPRequestHandler):
    server_version = "ProteinDSL/1.0"

    def log_message(self, fmt, *args):  # quieter console
        sys.stderr.write("  %s - %s\n" % (self.address_string(), fmt % args))

    # -- helpers ---------------------------------------------------------

    def _send(self, code: int, body: bytes, content_type: str):
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        # The page loads only its own CSS and JS and carries no inline style or
        # script, so the strictest useful policy applies with nothing to relax.
        self.send_header("Content-Security-Policy", "default-src 'self'")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.end_headers()
        self.wfile.write(body)

    def _send_json(self, code: int, payload: dict):
        self._send(code, json.dumps(payload).encode("utf-8"), "application/json; charset=utf-8")

    # -- routes ----------------------------------------------------------

    def do_GET(self):
        path = self.path.split("?", 1)[0]
        if path == "/api/examples":
            self._send_json(200, {"examples": load_examples()})
            return

        name = "index.html" if path in ("/", "/index.html") else path.lstrip("/")
        content_type = STATIC_FILES.get(name)
        if content_type is None:
            # Only the three files the page is built from are served; there is
            # no general static handler to walk out of with a crafted path.
            self._send(404, b"Not found", "text/plain; charset=utf-8")
            return

        try:
            body = (STATIC_DIR / name).read_bytes()
        except OSError:
            self._send(500, f"{name} is missing".encode("utf-8"), "text/plain; charset=utf-8")
            return
        self._send(200, body, content_type)

    def do_POST(self):
        if self.path.split("?", 1)[0] != "/api/run":
            self._send(404, b"Not found", "text/plain; charset=utf-8")
            return

        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            length = 0

        if length > MAX_PROGRAM_BYTES:
            self._send_json(413, {
                "ok": False, "stage": "parse", "statements": 0, "ast": "",
                "errors": [f"Program is larger than the {MAX_PROGRAM_BYTES} byte limit."],
                "sources": [], "results": [],
            })
            return

        raw = self.rfile.read(length) if length else b""
        try:
            source = json.loads(raw.decode("utf-8")).get("source", "")
        except (UnicodeDecodeError, json.JSONDecodeError, AttributeError):
            self._send_json(400, {
                "ok": False, "stage": "parse", "statements": 0, "ast": "",
                "errors": ["Request body must be JSON of the form {\"source\": \"...\"}"],
                "sources": [], "results": [],
            })
            return

        self._send_json(200, run_program(source))


def load_examples() -> list:
    """Read the .dsl files in sample_queries/ so the page can offer them."""
    examples = []
    sample_dir = REPO_ROOT / "sample_queries"
    for path in sorted(sample_dir.glob("*.dsl")):
        try:
            examples.append({
                "name": path.stem.replace("_", " "),
                "source": path.read_text(encoding="utf-8"),
            })
        except OSError:
            continue
    return examples


def main() -> int:
    parser = argparse.ArgumentParser(description="Serve the ProteinDSL playground.")
    parser.add_argument("--port", type=int, default=8000)
    # Loopback by default: this runs a compiler on submitted input, so it is
    # not something to expose to a network without thinking about it first.
    parser.add_argument("--host", default="127.0.0.1")
    args = parser.parse_args()

    exe = binary_path()
    if not exe.exists():
        print(f"warning: {exe} not found -- run `make` first, or queries will fail.\n",
              file=sys.stderr)

    os.chdir(REPO_ROOT)
    httpd = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"ProteinDSL playground on http://{args.host}:{args.port}  (Ctrl+C to stop)")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nstopped")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
