#!/usr/bin/env python3
"""Download and verify the pinned Noto Sans SC input-font source."""

from __future__ import annotations

import argparse
import hashlib
import urllib.request
from pathlib import Path


FONT_URL = (
    "https://raw.githubusercontent.com/notofonts/noto-cjk/"
    "f8d157532fbfaeda587e826d4cd5b21a49186f7c/"
    "Sans/SubsetOTF/SC/NotoSansSC-Medium.otf"
)
FONT_SHA256 = "7633f5a016d4dd95e685a69633d818aabc4644c4b08e26bd35b1b30c45ed5dda"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("tools/.cache/NotoSansSC-Medium.otf"),
    )
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    digest = hashlib.sha256()
    temporary = args.output.with_suffix(args.output.suffix + ".tmp")
    try:
        with urllib.request.urlopen(FONT_URL, timeout=60) as response, temporary.open(
            "wb"
        ) as output:
            while chunk := response.read(1024 * 1024):
                digest.update(chunk)
                output.write(chunk)
        actual = digest.hexdigest()
        if actual != FONT_SHA256:
            raise ValueError(f"font SHA-256 mismatch: {actual}")
        temporary.replace(args.output)
    finally:
        temporary.unlink(missing_ok=True)
    print(f"verified {args.output} sha256={FONT_SHA256}")


if __name__ == "__main__":
    main()
