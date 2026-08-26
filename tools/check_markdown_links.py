#!/usr/bin/env python3

"""Validate repository-local links and images in Markdown files."""

from __future__ import annotations

import re
import sys
from pathlib import Path
from urllib.parse import unquote


PROJECT_ROOT = Path(__file__).resolve().parent.parent
MARKDOWN_LINK = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
HTML_TARGET = re.compile(r'(?:src|href)="([^"]+)"')
EXTERNAL_SCHEMES = ("http://", "https://", "mailto:", "tel:", "data:")


def markdown_files() -> list[Path]:
    """Return first-party Markdown files while skipping generated directories."""

    excluded = {".git", ".pio", ".trash", "build", "dist"}
    return sorted(
        path
        for path in PROJECT_ROOT.rglob("*.md")
        if not any(part in excluded for part in path.relative_to(PROJECT_ROOT).parts)
    )


def local_target(raw_target: str) -> str | None:
    """Extract a local filesystem path from one inline Markdown target."""

    target = raw_target.strip()
    if target.startswith("<") and target.endswith(">"):
        target = target[1:-1]
    if " " in target and not target.startswith(EXTERNAL_SCHEMES):
        target = target.split(" ", 1)[0]
    if not target or target.startswith("#") or target.startswith(EXTERNAL_SCHEMES):
        return None
    return unquote(target.split("#", 1)[0])


def main() -> int:
    """Print every broken local target and return a CI-friendly status."""

    failures: list[str] = []
    checked_links = 0

    for markdown_path in markdown_files():
        content = markdown_path.read_text(encoding="utf-8")
        raw_targets = [
            match.group(1) for match in MARKDOWN_LINK.finditer(content)
        ]
        raw_targets.extend(
            match.group(1) for match in HTML_TARGET.finditer(content)
        )
        for raw_target in raw_targets:
            target = local_target(raw_target)
            if target is None:
                continue
            checked_links += 1
            resolved = (markdown_path.parent / target).resolve()
            if not resolved.exists():
                relative_markdown = markdown_path.relative_to(PROJECT_ROOT)
                failures.append(f"{relative_markdown}: {target}")

    if failures:
        print("Broken local Markdown targets:")
        for failure in failures:
            print(f"  - {failure}")
        return 1

    print(
        f"Markdown links: PASS "
        f"({len(markdown_files())} files, {checked_links} local targets)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
