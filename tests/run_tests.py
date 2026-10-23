#!/usr/bin/env python3
"""Golden-file tests for the ProteinDSL compiler.

Runs every program in tests/cases/ and in sample_queries/ through
`proteindsl --json` and compares the JSON document with the stored
`<name>.expected.json`. The process exit code must also match the stage the
document reports (0 done, 2 parse, 3 semantic, 4 runtime).

LOAD UNIPROT is served from tests/fixtures/cache/ (see the README there), so
the suite needs no network. Each program gets a fresh copy of that directory,
because the executor deletes a cached response that holds no entries.

Usage:
    python tests/run_tests.py [path-to-proteindsl] [--update]

--update rewrites the expected files from the current output. Review the diff
before committing it: the expected files are the specification.
"""

import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CASES = ROOT / "tests" / "cases"
FIXTURE_CACHE = ROOT / "tests" / "fixtures" / "cache"
EXPECTED_DIR = ROOT / "tests" / "expected"

EXIT_CODES = {"done": 0, "parse": 2, "semantic": 3, "runtime": 4}


def programs():
    """(test name, program path) for every test case and sample query."""
    found = [(p.stem, p) for p in sorted(CASES.glob("*.dsl"))]
    found += [("sample_" + p.stem, p)
              for p in sorted((ROOT / "sample_queries").glob("*.dsl"))]
    return found


def run(binary, program):
    with tempfile.TemporaryDirectory() as tmp:
        cache = Path(tmp) / "cache"
        shutil.copytree(FIXTURE_CACHE, cache)
        proc = subprocess.run(
            [str(binary), "--json", "--cache-dir", str(cache),
             str(program.relative_to(ROOT))],
            cwd=ROOT, capture_output=True, text=True, timeout=60)
    return proc


def main(argv):
    update = "--update" in argv
    args = [a for a in argv[1:] if a != "--update"]
    binary = (ROOT / (args[0] if args else "bin/proteindsl")).resolve()
    if not binary.exists():
        sys.exit(f"error: {binary} not found; run `make` first")

    EXPECTED_DIR.mkdir(exist_ok=True)
    failures = []

    for name, program in programs():
        proc = run(binary, program)
        try:
            actual = json.loads(proc.stdout)
        except json.JSONDecodeError:
            failures.append((name, "output is not JSON", proc.stdout + proc.stderr))
            continue

        problems = []
        want_exit = EXIT_CODES.get(actual.get("stage"))
        if proc.returncode != want_exit:
            problems.append(f"exit code {proc.returncode}, expected {want_exit} "
                            f"for stage '{actual.get('stage')}'")

        expected_path = EXPECTED_DIR / (name + ".expected.json")
        if update:
            expected_path.write_text(json.dumps(actual, indent=2) + "\n",
                                     encoding="utf-8")
        elif not expected_path.exists():
            problems.append(f"no {expected_path.relative_to(ROOT)}; "
                            "run with --update to create it")
        else:
            expected = json.loads(expected_path.read_text(encoding="utf-8"))
            if actual != expected:
                problems.append("output differs from "
                                f"{expected_path.relative_to(ROOT)}:\n"
                                + json.dumps(actual, indent=2))

        if problems:
            failures.append((name, "; ".join(problems[:1]),
                             "\n".join(problems[1:]) + proc.stderr))
        print(f"{'FAIL' if problems else 'ok  '}  {name}")

    total = len(programs())
    if failures:
        print(f"\n{len(failures)} of {total} failed:")
        for name, summary, detail in failures:
            print(f"\n--- {name}: {summary}\n{detail}")
        return 1
    print(f"\nAll {total} tests passed" + (" (expected files updated)" if update else ""))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
