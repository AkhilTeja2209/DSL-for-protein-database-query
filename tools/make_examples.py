#!/usr/bin/env python3
"""Generate the playground's examples.json from sample_queries/.

The page offers each .dsl file in sample_queries/ as a one-click example.
Generating the list at build time rather than hardcoding it in app.js keeps
the two from drifting apart.

Usage: python tools/make_examples.py <output-path>
"""

import json
import sys
from pathlib import Path

SAMPLES = Path(__file__).resolve().parent.parent / "sample_queries"


def main(argv: list) -> int:
    if len(argv) != 2:
        sys.exit(f"usage: {argv[0]} <output-path>")

    examples = [
        {"name": path.stem.replace("_", " "),
         "source": path.read_text(encoding="utf-8")}
        for path in sorted(SAMPLES.glob("*.dsl"))
    ]
    if not examples:
        sys.exit(f"error: no .dsl files found in {SAMPLES}")

    out = Path(argv[1])
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps({"examples": examples}, indent=1), encoding="utf-8")
    print(f"  [pages] wrote {out} ({len(examples)} examples)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
