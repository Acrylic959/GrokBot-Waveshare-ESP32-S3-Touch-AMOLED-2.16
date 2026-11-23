#!/usr/bin/env python3
"""Restore binary assets that are stored in this repo as base64 text chunks.

GitHub's contents API only accepts UTF-8 text, so firmware binaries are committed
as <original path>.b64/part-NNN.txt files under encoded-assets/. Run this script
from the repository root to recreate the original binaries in place.

    python3 tools/reconstruct-assets.py

The script verifies SHA-256 against releases/SHA256SUMS when an entry exists.
"""
from __future__ import annotations

import base64
import hashlib
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
ENCODED_ROOT = ROOT / "encoded-assets"
SUMS_FILE = ROOT / "releases" / "SHA256SUMS"


def load_checksums() -> dict[str, str]:
    sums: dict[str, str] = {}
    if not SUMS_FILE.exists():
        return sums
    for line in SUMS_FILE.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(None, 1)
        if len(parts) != 2:
            continue
        digest, name = parts
        name = name.lstrip("*").lstrip("./")
        sums[name] = digest.lower()
    return sums


def main() -> int:
    if not ENCODED_ROOT.exists():
        print("No encoded-assets/ directory; nothing to restore.", file=sys.stderr)
        return 0

    checksums = load_checksums()
    restored = 0
    for chunk_dir in sorted(ENCODED_ROOT.rglob("*.b64")):
        if not chunk_dir.is_dir():
            continue
        rel_parts = chunk_dir.relative_to(ENCODED_ROOT).parts
        rel_path = pathlib.Path(*rel_parts)
        target_rel = str(rel_path.with_suffix(""))
        chunks = sorted(chunk_dir.glob("part-*.txt"))
        if not chunks:
            print(f"skip {target_rel}: no chunks", file=sys.stderr)
            continue
        encoded = "".join(p.read_text(encoding="ascii") for p in chunks)
        data = base64.b64decode(encoded, validate=True)
        target = ROOT / target_rel
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        restored += 1

        expected = checksums.get(target_rel)
        if expected:
            actual = hashlib.sha256(data).hexdigest()
            if actual != expected:
                print(f"SHA256 MISMATCH {target_rel}: {actual} != {expected}", file=sys.stderr)
                return 1
            print(f"restored {target_rel} ({len(data)} bytes, sha256 OK)")
        else:
            print(f"restored {target_rel} ({len(data)} bytes)")

    print(f"Done: {restored} binary file(s) restored.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
