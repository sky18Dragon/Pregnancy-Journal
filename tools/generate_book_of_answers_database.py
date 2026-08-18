#!/usr/bin/env python3
"""Generate the firmware answer table from the checked-in CSV source."""

from __future__ import annotations

import csv
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE_CSV = ROOT / "assets" / "book_of_answers" / "source" / "database.csv"
OUTPUT_CPP = (
    ROOT
    / "src"
    / "apps"
    / "book_of_answers"
    / "book_of_answers_answers.cpp"
)
EXPECTED_ANSWER_COUNT = 350
MAX_DISPLAY_LINE_LENGTH = 23
MAX_DISPLAY_LINES = 4


def normalize_display_text(text: str) -> str:
    """Convert source typography to characters supported by the 5x7 font."""

    normalized = (
        text.strip()
        .replace("’", "'")
        .replace("‘", "'")
        .replace("“", '"')
        .replace("”", '"')
        .replace("–", "-")
        .replace("—", "-")
    )
    normalized.encode("ascii")
    return normalized


def wrapped_line_count(text: str) -> int:
    """Return the greedy word-wrap line count used by the firmware renderer."""

    line_count = 1
    line_length = 0
    for word in text.split():
        separator = 1 if line_length else 0
        if line_length + separator + len(word) > MAX_DISPLAY_LINE_LENGTH:
            line_count += 1
            line_length = len(word)
        else:
            line_length += separator + len(word)
    return line_count


def load_answers() -> list[str]:
    """Load numbered answers and validate the source boundary and display fit."""

    answers: list[str] = []
    acknowledgement_seen = False
    with SOURCE_CSV.open(encoding="utf-8-sig", newline="") as source:
        for row_number, row in enumerate(csv.reader(source), start=1):
            if len(row) < 2:
                raise ValueError(f"CSV row {row_number} has fewer than two columns")
            if row[1].strip() == "ACKNOWLEDGMENTS":
                acknowledgement_seen = True
                break
            expected_id = len(answers) + 1
            if int(row[0]) != expected_id:
                raise ValueError(
                    f"CSV row {row_number} has id {row[0]}, expected {expected_id}"
                )
            answer = normalize_display_text(row[1])
            if not answer:
                raise ValueError(f"CSV row {row_number} has an empty answer")
            if any(
                len(word) > MAX_DISPLAY_LINE_LENGTH
                for word in answer.split()
            ):
                raise ValueError(
                    f"CSV row {row_number} contains a word wider than one line"
                )
            if wrapped_line_count(answer) > MAX_DISPLAY_LINES:
                raise ValueError(
                    f"CSV row {row_number} exceeds {MAX_DISPLAY_LINES} display lines"
                )
            answers.append(answer)

    if not acknowledgement_seen:
        raise ValueError("CSV acknowledgement boundary was not found")
    if len(answers) != EXPECTED_ANSWER_COUNT:
        raise ValueError(
            f"Expected {EXPECTED_ANSWER_COUNT} answers, found {len(answers)}"
        )
    return answers


def write_cpp(answers: list[str]) -> None:
    """Write the validated answer strings and the fixed crystal answer pool."""

    entries = "\n".join(
        f"    {{{json.dumps(answer, ensure_ascii=True)}}}," for answer in answers
    )
    output = f'''#include "book_of_answers_answers.h"

namespace {{

// Generated from assets/book_of_answers/source/database.csv.
// 由assets/book_of_answers/source/database.csv自动生成。
constexpr BookMessageAnswer kMessageAnswers[] = {{
{entries}
}};

constexpr const char *kCrystalAnswers[] = {{
    "YES",
    "NO",
    "UNCLEAR",
}};

}}  // namespace

size_t book_message_answer_count()
{{
    return sizeof(kMessageAnswers) / sizeof(kMessageAnswers[0]);
}}

const BookMessageAnswer &book_message_answer(size_t index)
{{
    return kMessageAnswers[index % book_message_answer_count()];
}}

size_t book_crystal_answer_count()
{{
    return sizeof(kCrystalAnswers) / sizeof(kCrystalAnswers[0]);
}}

const char *book_crystal_answer(size_t index)
{{
    return kCrystalAnswers[index % book_crystal_answer_count()];
}}
'''
    OUTPUT_CPP.write_text(output, encoding="utf-8")


def main() -> None:
    """Validate the CSV and regenerate the C++ answer table."""

    answers = load_answers()
    write_cpp(answers)
    print(f"Generated {len(answers)} answers in {OUTPUT_CPP}")


if __name__ == "__main__":
    main()
