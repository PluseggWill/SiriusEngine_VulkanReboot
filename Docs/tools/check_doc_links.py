#!/usr/bin/env python3
"""Scan markdown/mdc for broken relative internal links."""
from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
LINK_RE = re.compile(r"\[[^\]]*\]\(([^)]+)\)")
SKIP_PREFIXES = ("http://", "https://", "mailto:", "#")


def is_internal(link: str) -> bool:
    if not link or link.startswith(SKIP_PREFIXES):
        return False
    if "://" in link:
        return False
    return True


def resolve(from_file: Path, link: str) -> Path | None:
    link = link.split("#", 1)[0].strip()
    if not link:
        return from_file
    if link.startswith("/"):
        return REPO / link.lstrip("/")
    return (from_file.parent / link).resolve()


def scan(root: Path) -> list[tuple[Path, str, str]]:
    broken: list[tuple[Path, str, str]] = []
    for path in sorted(root.rglob("*")):
        if path.suffix not in {".md", ".mdc"}:
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except OSError:
            continue
        for m in LINK_RE.finditer(text):
            raw = m.group(1).strip()
            if not is_internal(raw):
                continue
            target = resolve(path, raw)
            if target is None:
                continue
            if not target.exists():
                rel = path.relative_to(REPO)
                broken.append((rel, raw, str(m.group(0))))
    return broken


def main() -> int:
    roots = [REPO / "Docs", REPO / ".cursor"]
    all_broken: list[tuple[Path, str, str]] = []
    for root in roots:
        if root.exists():
            all_broken.extend(scan(root))
    if not all_broken:
        print("No broken internal links found.")
        return 0
    print(f"Broken links: {len(all_broken)}")
    for rel, link, ctx in all_broken:
        print(f"  {rel}: {link!r}  ({ctx})")
    return 1


if __name__ == "__main__":
    sys.exit(main())
