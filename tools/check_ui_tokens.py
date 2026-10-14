#!/usr/bin/env python3
"""Keep page code on ui_tokens: no literal font sizes or text margins.

Scans the UI layer for text-drawing calls and requires the pixel-size
argument (and a left text margin of 24) to use `ui_tokens::` names.

  tools/check_ui_tokens.py          report violations, exit 1 if any
  tools/check_ui_tokens.py --fix    rewrite mapped literals to tokens
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FILES = [ROOT / "src/ui/app_pages.cpp", ROOT / "src/ui/prototype_ui.cpp",
         ROOT / "src/ui/repository_status_page.cpp",
         *sorted((ROOT / "src/apps").glob("*/*_pages.cpp"))]

SIZE_TOKENS = {16: "kSecondaryBody", 18: "kBody", 20: "kEmphasis", 22: "kCardTitle",
               24: "kSectionHeading", 28: "kPageHeading", 52: "kDisplay",
               56: "kDisplayLarge", 70: "kCounterDisplay"}
# function -> (size argument index, x argument index or None)
CALLS = {
    "draw_text_pixels": (3, 0),
    "ui_draw_wrapped_text_pixels": (5, 1),
    "draw_centered": (4, None),
    "draw_centered_text_pixels": (3, None),
    "font_text_width_pixels": (1, None),
}
CALL_RE = re.compile(r"\b(" + "|".join(CALLS) + r")\s*\(")
INT_RE = re.compile(r"\b(\d+)U?\b")


# Layout literals that have a token: (pattern, replacement, description)
LAYOUT = [
    (re.compile(r"\b(width(?:\(\))?) - 48\b"), r"\1 - 2 * ui_tokens::kContentMargin", "content width"),
    (re.compile(r", 56\}"), ", ui_tokens::kButtonHeight}", "button height 56"),
    (re.compile(r", 48\}"), ", ui_tokens::kMinimumTouchHeight}", "touch height 48"),
    (re.compile(r"\?\s*24U?\s*:\s*28U?"), "? ui_tokens::kSectionHeading : ui_tokens::kPageHeading", "heading size pair"),
]


def split_args(src, open_index):
    """Return (args as (start, end) spans, index of the closing paren)."""
    depth, start, spans, i, quote = 0, open_index + 1, [], open_index + 1, None
    while i < len(src):
        c = src[i]
        if quote:
            if c == "\\":
                i += 1
            elif c == quote:
                quote = None
        elif c in "\"'":
            quote = c
        elif c in "([{":
            depth += 1
        elif c in ")]}":
            if depth == 0:
                spans.append((start, i))
                return spans, i
            depth -= 1
        elif c == "," and depth == 0:
            spans.append((start, i))
            start = i + 1
        i += 1
    return spans, i


def process(path, fix):
    src = path.read_text()
    problems, edits = [], []
    for m in CALL_RE.finditer(src):
        name = m.group(1)
        # skip declarations / definitions: preceded by a type name
        before = src[max(0, m.start() - 12):m.start()]
        if re.search(r"\b(void|int|bool)\s+$", before):
            continue
        spans, _ = split_args(src, m.end() - 1)
        size_i, x_i = CALLS[name]
        line = src.count("\n", 0, m.start()) + 1
        if len(spans) <= size_i:
            continue
        s0, s1 = spans[size_i]
        arg = src[s0:s1]
        if "ui_tokens::" not in arg and INT_RE.search(arg):
            new = INT_RE.sub(lambda k: ("ui_tokens::" + SIZE_TOKENS[int(k.group(1))])
                             if int(k.group(1)) in SIZE_TOKENS else k.group(0), arg)
            if INT_RE.search(new):
                problems.append(f"{path.relative_to(ROOT)}:{line}: unmapped font size in {name}: {arg.strip()}")
            elif fix:
                edits.append((s0, s1, new))
            else:
                problems.append(f"{path.relative_to(ROOT)}:{line}: literal font size in {name}: {arg.strip()}")
        if x_i is not None and len(spans) > x_i:
            x0, x1 = spans[x_i]
            if src[x0:x1].strip() == "24":
                if fix:
                    edits.append((x0, x1, " ui_tokens::kContentMargin" if src[x0] == " " else "ui_tokens::kContentMargin"))
                else:
                    problems.append(f"{path.relative_to(ROOT)}:{line}: literal margin 24 in {name}")
    for pattern, replacement, what in LAYOUT:
        for m in pattern.finditer(src):
            line = src.count("\n", 0, m.start()) + 1
            if fix:
                edits.append((m.start(), m.end(), pattern.sub(replacement, m.group(0))))
            else:
                problems.append(f"{path.relative_to(ROOT)}:{line}: literal {what}")
    if fix and edits:
        for s0, s1, new in sorted(edits, reverse=True):
            src = src[:s0] + new + src[s1:]
        path.write_text(src)
    return problems


def main():
    fix = "--fix" in sys.argv
    problems = []
    for f in FILES:
        problems += process(f, fix)
    for p in problems:
        print(p)
    if fix:
        print("rewritten; re-run without --fix to check what is left")
        return 0
    print(f"ui tokens: {len(problems)} violation(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
