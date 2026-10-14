#!/usr/bin/env python3
"""Generate the bounded GB2312 Level-1 single-character pinyin index."""

from __future__ import annotations

import argparse
import re
import unicodedata
from collections import defaultdict
from pathlib import Path


LINE_PATTERN = re.compile(r"^U\+([0-9A-F]+):\s*([^#]+)")
FREQUENCY_PATTERN = re.compile(r"^U\+([0-9A-F]+)\t([1-5])$")
PRIORITY_TIERS = {"core": 0, "domain": 1}
CANDIDATES_PER_PAGE = 8
RANKING_VERSION = 2


def normalize_pinyin(value: str) -> str:
    value = value.strip().lower().replace("ü", "v")
    decomposed = unicodedata.normalize("NFD", value)
    normalized = "".join(
        character
        for character in decomposed
        if unicodedata.category(character) != "Mn"
    ).replace("u:", "v")
    if not normalized or any(character < "a" or character > "z" for character in normalized):
        raise ValueError(f"unsupported pinyin syllable: {value!r}")
    return normalized


def parse_readings(path: Path) -> dict[int, list[str]]:
    readings: dict[int, list[str]] = {}
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        match = LINE_PATTERN.match(line)
        if match is None:
            continue
        codepoint = int(match.group(1), 16)
        syllables: list[str] = []
        for raw_syllable in match.group(2).split(","):
            syllable = normalize_pinyin(raw_syllable)
            if syllable not in syllables:
                syllables.append(syllable)
        if not syllables:
            raise ValueError(f"no readings at {path}:{line_number}")
        readings[codepoint] = syllables
    return readings


def gb2312_level1_codepoints() -> set[int]:
    codepoints: set[int] = set()
    for lead in range(0xB0, 0xD8):
        for trail in range(0xA1, 0xFF):
            try:
                character = bytes((lead, trail)).decode("gb2312")
            except UnicodeDecodeError:
                continue
            codepoints.add(ord(character))
    if len(codepoints) != 3755:
        raise ValueError(f"unexpected GB2312 Level-1 size: {len(codepoints)}")
    return codepoints


def parse_frequency(path: Path, target: set[int]) -> dict[int, int]:
    frequency: dict[int, int] = {}
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line or line.startswith("#"):
            continue
        match = FREQUENCY_PATTERN.match(line)
        if match is None:
            raise ValueError(f"invalid frequency row at {path}:{line_number}")
        codepoint = int(match.group(1), 16)
        if codepoint not in target:
            raise ValueError(f"frequency codepoint outside target at {path}:{line_number}")
        if codepoint in frequency:
            raise ValueError(f"duplicate frequency codepoint at {path}:{line_number}")
        frequency[codepoint] = int(match.group(2))
    if len(frequency) != 3171:
        raise ValueError(f"unexpected frequency entry count: {len(frequency)}")
    return frequency


def parse_priorities(
    path: Path,
    target: set[int],
    primary: dict[int, list[str]],
    fallback: dict[int, list[str]],
) -> tuple[dict[tuple[str, int], tuple[int, int]], list[tuple[str, int]]]:
    priorities: dict[tuple[str, int], tuple[int, int]] = {}
    domain_benchmark: list[tuple[str, int]] = []
    order = 0
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) != 3 or fields[0] not in PRIORITY_TIERS:
            raise ValueError(f"invalid priority row at {path}:{line_number}")
        tier_name, term, raw_syllables = fields
        syllables = [normalize_pinyin(value) for value in raw_syllables.split()]
        if not term or len(term) != len(syllables):
            raise ValueError(f"term/syllable length mismatch at {path}:{line_number}")
        tier = PRIORITY_TIERS[tier_name]
        for character, syllable in zip(term, syllables):
            codepoint = ord(character)
            if codepoint not in target:
                raise ValueError(f"priority character outside target at {path}:{line_number}")
            readings = primary.get(codepoint, fallback.get(codepoint))
            if readings is None or syllable not in readings:
                raise ValueError(
                    f"priority reading mismatch for {character}/{syllable} "
                    f"at {path}:{line_number}; generated readings={readings}"
                )
            key = (syllable, codepoint)
            existing = priorities.get(key)
            value = (tier, order)
            if existing is None or value < existing:
                priorities[key] = value
            if tier_name == "domain":
                domain_benchmark.append(key)
            order += 1
    if not domain_benchmark:
        raise ValueError("priority file has no domain benchmark entries")
    return priorities, domain_benchmark


def wrap_values(values: list[str], indent: str = "    ", width: int = 92) -> str:
    lines: list[str] = []
    current = indent
    for value in values:
        token = value + ","
        if len(current) + len(token) + 1 > width and current != indent:
            lines.append(current.rstrip())
            current = indent
        current += token + " "
    if current != indent:
        lines.append(current.rstrip())
    return "\n".join(lines)


def generate(primary_path: Path,
             fallback_path: Path,
             common_path: Path,
             frequency_path: Path,
             priority_path: Path,
             header_path: Path,
             source_path: Path) -> None:
    primary = parse_readings(primary_path)
    fallback = parse_readings(fallback_path)
    common = set(parse_readings(common_path))
    target = gb2312_level1_codepoints()
    frequency = parse_frequency(frequency_path, target)
    priorities, domain_benchmark = parse_priorities(
        priority_path, target, primary, fallback
    )

    candidates_by_key: dict[str, set[int]] = defaultdict(set)
    missing: list[int] = []
    for codepoint in sorted(target):
        readings = primary.get(codepoint, fallback.get(codepoint))
        if readings is None:
            missing.append(codepoint)
            continue
        for reading in readings:
            candidates_by_key[reading].add(codepoint)
    if missing:
        missing_text = "".join(chr(codepoint) for codepoint in missing)
        raise ValueError(f"missing GB2312 Level-1 readings: {missing_text}")

    keys = sorted(candidates_by_key)
    key_offsets: list[int] = []
    key_offset = 0
    entries: list[tuple[int, int, int]] = []
    candidates: list[int] = []
    ordered_by_key: dict[str, list[int]] = {}
    for key in keys:
        key_offsets.append(key_offset)
        key_offset += len(key) + 1
        ordered = sorted(
            candidates_by_key[key],
            key=lambda codepoint: (
                priorities.get((key, codepoint), (2, 0))[0],
                frequency.get(codepoint, 6),
                priorities.get((key, codepoint), (2, 0))[1],
                codepoint not in common,
                codepoint,
            ),
        )
        ordered_by_key[key] = ordered
        entries.append((key_offsets[-1], len(candidates), len(ordered)))
        candidates.extend(ordered)

    if key_offset > 0xFFFF or len(candidates) > 0xFFFF:
        raise ValueError("generated offsets exceed uint16_t")

    domain_ranks = [
        ordered_by_key[syllable].index(codepoint) + 1
        for syllable, codepoint in domain_benchmark
    ]
    domain_first_page_count = sum(
        rank <= CANDIDATES_PER_PAGE for rank in domain_ranks
    )
    domain_maximum_rank = max(domain_ranks)
    if domain_first_page_count != len(domain_benchmark):
        missed = [
            f"{chr(codepoint)}/{syllable}#{ordered_by_key[syllable].index(codepoint) + 1}"
            for syllable, codepoint in domain_benchmark
            if ordered_by_key[syllable].index(codepoint) >= CANDIDATES_PER_PAGE
        ]
        raise ValueError(f"domain candidates outside first page: {' '.join(missed)}")

    header = """// Generated by tools/generate_pinyin_index.py. Do not edit.
#pragma once

#include <cstddef>
#include <cstdint>

struct PinyinGeneratedEntry {
    uint16_t key_offset;
    uint16_t candidate_offset;
    uint16_t candidate_count;
};

static_assert(sizeof(PinyinGeneratedEntry) == 6U,
              "pinyin index entry layout changed");

extern const char kPinyinGeneratedKeys[];
extern const PinyinGeneratedEntry kPinyinGeneratedEntries[];
extern const uint16_t kPinyinGeneratedCandidates[];
extern const size_t kPinyinGeneratedEntryCount;
extern const size_t kPinyinGeneratedCandidateCount;
extern const size_t kPinyinGeneratedKeyBytes;
extern const size_t kPinyinGeneratedResourceBytes;
extern const uint16_t kPinyinGeneratedMaximumCandidates;
extern const uint16_t kPinyinGeneratedRankingVersion;
extern const size_t kPinyinGeneratedDomainBenchmarkCount;
extern const size_t kPinyinGeneratedDomainFirstPageCount;
extern const uint16_t kPinyinGeneratedDomainMaximumRank;
"""

    key_literals = "\n".join(f'    "{key}\\0"' for key in keys)
    entry_values = [f"{{{key_offset}U, {candidate_offset}U, {count}U}}"
                    for key_offset, candidate_offset, count in entries]
    candidate_values = [f"0x{codepoint:04X}U" for codepoint in candidates]
    maximum_candidates = max(count for _, _, count in entries)
    resource_bytes = key_offset + len(entries) * 6 + len(candidates) * 2
    source = f"""// Generated by tools/generate_pinyin_index.py. Do not edit.
#include "pinyin_index_data.h"

const char kPinyinGeneratedKeys[] =
{key_literals};

const PinyinGeneratedEntry kPinyinGeneratedEntries[] = {{
{wrap_values(entry_values)}
}};

const uint16_t kPinyinGeneratedCandidates[] = {{
{wrap_values(candidate_values)}
}};

const size_t kPinyinGeneratedEntryCount = {len(entries)}U;
const size_t kPinyinGeneratedCandidateCount = {len(candidates)}U;
const size_t kPinyinGeneratedKeyBytes = {key_offset}U;
const size_t kPinyinGeneratedResourceBytes = {resource_bytes}U;
const uint16_t kPinyinGeneratedMaximumCandidates = {maximum_candidates}U;
const uint16_t kPinyinGeneratedRankingVersion = {RANKING_VERSION}U;
const size_t kPinyinGeneratedDomainBenchmarkCount = {len(domain_benchmark)}U;
const size_t kPinyinGeneratedDomainFirstPageCount = {domain_first_page_count}U;
const uint16_t kPinyinGeneratedDomainMaximumRank = {domain_maximum_rank}U;
"""
    header_path.parent.mkdir(parents=True, exist_ok=True)
    header_path.write_text(header, encoding="utf-8")
    source_path.write_text(source, encoding="utf-8")
    print(
        "Pinyin index generated: "
        f"characters={len(target)} keys={len(keys)} "
        f"candidate_refs={len(candidates)} key_bytes={key_offset} "
        f"resource_bytes={resource_bytes} max_candidates={maximum_candidates} "
        f"ranking_version={RANKING_VERSION} "
        f"domain_first_page={domain_first_page_count}/{len(domain_benchmark)} "
        f"domain_max_rank={domain_maximum_rank}"
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--primary",
        type=Path,
        default=Path("third_party/pinyin-data/kMandarin_8105.txt"),
    )
    parser.add_argument(
        "--fallback",
        type=Path,
        default=Path("third_party/pinyin-data/gb2312_fallback.txt"),
    )
    parser.add_argument(
        "--common",
        type=Path,
        default=Path("third_party/pinyin-data/kHanyuPinlu.txt"),
    )
    parser.add_argument(
        "--frequency",
        type=Path,
        default=Path("third_party/unicode-unihan/kFrequency-gb2312-level1.txt"),
    )
    parser.add_argument(
        "--priority",
        type=Path,
        default=Path("assets/chinese-input/candidate-priority.tsv"),
    )
    parser.add_argument(
        "--header",
        type=Path,
        default=Path("src/ui/assets/pinyin_index_data.h"),
    )
    parser.add_argument(
        "--source",
        type=Path,
        default=Path("src/ui/assets/pinyin_index_data.cpp"),
    )
    args = parser.parse_args()
    generate(
        args.primary,
        args.fallback,
        args.common,
        args.frequency,
        args.priority,
        args.header,
        args.source,
    )


if __name__ == "__main__":
    main()
