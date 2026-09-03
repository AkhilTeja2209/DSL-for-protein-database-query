#!/usr/bin/env python3
"""Assemble docs/ (the GitHub Pages site) from the shared page in web/.

`make wasm` runs this after emcc. It does the two things emcc does not:

  * copies web/index.html to docs/, inserting the <script> tag that loads the
    WebAssembly module. app.js keys off whether createProteinDSL exists, so
    that one tag is the whole difference between the Pages build and the
    locally served one.
  * writes docs/examples.json from sample_queries/, since Pages has no
    /api/examples endpoint to ask.

Standard library only, like server/app.py.
"""

import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
WEB = REPO_ROOT / "web"
DOCS = REPO_ROOT / "docs"

WASM_TAG = '<script src="proteindsl.js" defer></script>'
APP_TAG = '<script src="app.js" defer></script>'


def build_index() -> None:
    html = (WEB / "index.html").read_text(encoding="utf-8")

    # app.js reads `createProteinDSL`, so the loader must run first. `defer`
    # preserves document order, which keeps that guarantee.
    marker = '<script src="/app.js" defer></script>'
    if marker not in html:
        sys.exit(f"error: expected {marker!r} in web/index.html")
    html = html.replace(marker, f"{WASM_TAG}\n{APP_TAG}")

    # Pages serves the site from a subpath, so absolute asset URLs would miss.
    html = html.replace('href="/app.css"', 'href="app.css"')

    DOCS.mkdir(exist_ok=True)
    (DOCS / "index.html").write_text(html, encoding="utf-8")


def build_examples() -> None:
    examples = []
    for path in sorted((REPO_ROOT / "sample_queries").glob("*.dsl")):
        examples.append({
            "name": path.stem.replace("_", " "),
            "source": path.read_text(encoding="utf-8"),
        })
    (DOCS / "examples.json").write_text(
        json.dumps({"examples": examples}, indent=1), encoding="utf-8")


def main() -> int:
    if not (WEB / "index.html").exists():
        sys.exit("error: web/index.html not found")
    build_index()
    build_examples()
    print(f"  [pages] wrote {DOCS.name}/index.html and {DOCS.name}/examples.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
