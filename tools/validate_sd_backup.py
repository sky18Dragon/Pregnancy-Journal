#!/usr/bin/env python3
"""Read-only validator for a reTerminal Sticky PREGJNL backup.

The validator deliberately performs no repair.  It verifies the selected
manifest, journal framing and the record/index objects referenced by committed
payloads.  A zero exit status means the copied directory is internally
consistent and belongs to exactly one dataset.
"""

from __future__ import annotations

import argparse
import json
import re
import struct
import sys
import zlib
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Iterable


MANIFEST_LIMIT = 192
JOURNAL_HEADER_BYTES = 72
JOURNAL_MAX_PAYLOAD = 1024
JOURNAL_SEGMENT_MAX_BYTES = 256 * 1024
DATASET_RE = re.compile(r"[0-9a-f]{32}\Z")
RECORD_ID_RE = re.compile(r"[0-9a-f]{32}\Z")
SEGMENT_RE = re.compile(r"TXN([0-9]{5})\.LOG\Z")
FAT83_RE = re.compile(r"[A-Z0-9_]{1,8}(?:\.[A-Z0-9_]{1,3})?\Z")
MANIFEST_RE = re.compile(
    rb'\{"magic":"PJMF","schema":1,"dataset_id":"([0-9a-f]{32})",'
    rb'"generation":([1-9][0-9]*),"crc32":"([0-9A-F]{8})"\}\n\Z'
)
UINT64_MAX = (1 << 64) - 1
INDEX_HEADER_BYTES = 80
INDEX_ENTRY_BYTES = 64
INDEX_ENTRY_LIMIT = 1024


class ValidationFailure(Exception):
    """An expected invalid-backup condition."""


@dataclass
class ValidationReport:
    root: str
    valid: bool = False
    dataset_id: str | None = None
    generation: int | None = None
    selected_slot: str | None = None
    manifest_state: str | None = None
    segment_count: int = 0
    journal_record_count: int = 0
    referenced_object_count: int = 0
    checkpoint_count: int = 0
    warnings: list[str] = field(default_factory=list)
    errors: list[str] = field(default_factory=list)


def _crc32(data: bytes, value: int = 0) -> int:
    return zlib.crc32(data, value) & 0xFFFFFFFF


def _application_root(path: Path) -> Path:
    candidate = path.resolve()
    if candidate.name == "PREGJNL":
        return candidate
    nested = candidate / "PREGJNL"
    if nested.is_dir():
        return nested
    raise ValidationFailure(
        "expected the PREGJNL directory or a card/backup root containing it"
    )


def _read_regular_file(path: Path, maximum: int | None = None) -> bytes:
    if path.is_symlink() or not path.is_file():
        raise ValidationFailure(f"not a regular file: {path}")
    size = path.stat().st_size
    if maximum is not None and size > maximum:
        raise ValidationFailure(f"file exceeds {maximum} bytes: {path}")
    try:
        return path.read_bytes()
    except OSError as error:
        raise ValidationFailure(f"cannot read {path}: {error}") from error


def _parse_manifest(path: Path, slot: str) -> tuple[str, int] | None:
    if not path.exists():
        return None
    data = _read_regular_file(path, MANIFEST_LIMIT - 1)
    match = MANIFEST_RE.fullmatch(data)
    if match is None:
        raise ValidationFailure(f"manifest slot {slot} is non-canonical or unsupported")
    dataset_id = match.group(1).decode("ascii")
    generation_text = match.group(2).decode("ascii")
    generation = int(generation_text)
    if generation > UINT64_MAX:
        raise ValidationFailure(f"manifest slot {slot} generation overflows uint64")
    expected_slot = "A" if generation & 1 else "B"
    if slot != expected_slot:
        raise ValidationFailure(
            f"manifest generation {generation} belongs in slot {expected_slot}, not {slot}"
        )
    payload = (
        f'{{"magic":"PJMF","schema":1,"dataset_id":"{dataset_id}",'
        f'"generation":{generation}}}'
    ).encode("ascii")
    expected_crc = f"{_crc32(payload):08X}".encode("ascii")
    if match.group(3) != expected_crc:
        raise ValidationFailure(f"manifest slot {slot} CRC32 mismatch")
    return dataset_id, generation


def _select_manifest(root: Path, report: ValidationReport) -> tuple[str, int]:
    manifest_dir = root / "MANIFEST"
    if manifest_dir.is_symlink() or not manifest_dir.is_dir():
        raise ValidationFailure("MANIFEST directory is missing or not a directory")
    a = _parse_manifest(manifest_dir / "MNF000A.JSN", "A")
    b = _parse_manifest(manifest_dir / "MNF000B.JSN", "B")
    if a is None and b is None:
        raise ValidationFailure("both manifest slots are missing")
    if a is not None and b is not None:
        if a[0] != b[0]:
            raise ValidationFailure("manifest slots describe different dataset IDs")
        if abs(a[1] - b[1]) != 1:
            raise ValidationFailure("manifest generations are not adjacent")
        report.manifest_state = "ready"
    else:
        report.manifest_state = "degraded"
        report.warnings.append("only one valid manifest slot is present")
    selected_slot, selected = max(
        (("A", a), ("B", b)), key=lambda item: item[1][1] if item[1] else -1
    )
    assert selected is not None
    report.dataset_id = selected[0]
    report.generation = selected[1]
    report.selected_slot = selected_slot
    return selected


def _decode_fixed_name(raw: bytes, label: str) -> str:
    nul = raw.find(b"\0")
    if nul >= 0:
        if any(raw[nul:]):
            raise ValidationFailure(f"{label} has non-zero bytes after its terminator")
        raw = raw[:nul]
    try:
        name = raw.decode("ascii")
    except UnicodeDecodeError as error:
        raise ValidationFailure(f"{label} is not ASCII") from error
    if name and FAT83_RE.fullmatch(name) is None:
        raise ValidationFailure(f"{label} is not an uppercase 8.3 name: {name!r}")
    return name


def _verify_referenced_file(path: Path, size: int, checksum: int, label: str) -> bytes:
    data = _read_regular_file(path)
    if len(data) != size:
        raise ValidationFailure(
            f"{label} length mismatch for {path.name}: expected {size}, got {len(data)}"
        )
    if _crc32(data) != checksum:
        raise ValidationFailure(f"{label} CRC32 mismatch for {path.name}")
    return data


def _validate_index_snapshot(
    data: bytes, dataset_id: str, transaction_id: int, live_count: int
) -> None:
    if len(data) < INDEX_HEADER_BYTES:
        raise ValidationFailure("checkpoint index is truncated")
    if data[:4] != b"PJIX":
        raise ValidationFailure("checkpoint index magic mismatch")
    schema, header_bytes, total_bytes, entry_count, encoded_live = struct.unpack_from(
        "<HHIII", data, 4
    )
    if schema != 1:
        raise ValidationFailure(f"unsupported checkpoint index schema {schema}")
    expected_size = INDEX_HEADER_BYTES + entry_count * INDEX_ENTRY_BYTES
    if (
        header_bytes != INDEX_HEADER_BYTES
        or entry_count > INDEX_ENTRY_LIMIT
        or total_bytes != expected_size
        or len(data) != expected_size
        or any(data[20:24])
        or any(data[72:80])
    ):
        raise ValidationFailure("checkpoint index header or length is invalid")
    encoded_transaction = struct.unpack_from("<Q", data, 24)[0]
    try:
        encoded_dataset = data[32:64].decode("ascii")
    except UnicodeDecodeError as error:
        raise ValidationFailure("checkpoint index dataset ID is not ASCII") from error
    if encoded_dataset != dataset_id or encoded_transaction != transaction_id:
        raise ValidationFailure("checkpoint index identity does not match checkpoint")
    entries = data[INDEX_HEADER_BYTES:]
    entries_crc, file_crc = struct.unpack_from("<II", data, 64)
    if _crc32(entries) != entries_crc:
        raise ValidationFailure("checkpoint index entries CRC32 mismatch")
    if _crc32(entries, _crc32(data[:68])) != file_crc:
        raise ValidationFailure("checkpoint index file CRC32 mismatch")

    decoded_live = 0
    previous_key: tuple[int, str] | None = None
    for offset in range(0, len(entries), INDEX_ENTRY_BYTES):
        entry = entries[offset : offset + INDEX_ENTRY_BYTES]
        repository, tombstone = entry[0], entry[1]
        if repository not in range(1, 11) or tombstone not in (0, 1):
            raise ValidationFailure("checkpoint index entry type is invalid")
        if any(entry[2:4]) or any(entry[60:64]):
            raise ValidationFailure("checkpoint index entry reserved bytes are non-zero")
        revision, object_bytes, object_crc = struct.unpack_from("<III", entry, 4)
        try:
            record_id = entry[16:48].decode("ascii")
        except UnicodeDecodeError as error:
            raise ValidationFailure("checkpoint index record ID is not ASCII") from error
        object_name = _decode_fixed_name(entry[48:60], "index object file")
        key = (repository, record_id)
        if (
            revision == 0
            or RECORD_ID_RE.fullmatch(record_id) is None
            or (previous_key is not None and key <= previous_key)
        ):
            raise ValidationFailure("checkpoint index entry key/revision is invalid")
        previous_key = key
        if tombstone:
            if object_bytes != 0 or object_crc != 0 or object_name:
                raise ValidationFailure("checkpoint index tombstone references an object")
        else:
            if object_bytes < 1 or object_bytes > 1024 or not object_name:
                raise ValidationFailure("checkpoint index live entry is invalid")
            decoded_live += 1
    if encoded_live != decoded_live or encoded_live != live_count:
        raise ValidationFailure("checkpoint index live count mismatch")


def _validate_transaction_payload(
    root: Path, payload: bytes, record_type: int, report: ValidationReport
) -> None:
    if len(payload) < 16:
        raise ValidationFailure("transaction payload is truncated")
    if payload[:4] != b"PJTX":
        raise ValidationFailure("transaction payload magic mismatch")
    schema, header_bytes, total_bytes = struct.unpack_from("<HHH", payload, 4)
    if schema != 1:
        raise ValidationFailure(f"unsupported transaction payload schema {schema}")
    count = payload[10]
    if header_bytes != 16 or count < 1 or count > 4 or any(payload[11:16]):
        raise ValidationFailure("transaction payload header is invalid")
    expected = 16 + count * 64
    if total_bytes != expected or len(payload) != expected:
        raise ValidationFailure("transaction payload length is invalid")

    has_upsert = False
    has_delete = False
    for index in range(count):
        mutation = payload[16 + index * 64 : 16 + (index + 1) * 64]
        mutation_type = mutation[0]
        repository = mutation[1]
        if mutation_type not in (0, 1) or repository not in range(1, 11):
            raise ValidationFailure("transaction mutation type/repository is invalid")
        if any(mutation[2:4]):
            raise ValidationFailure("transaction mutation reserved bytes are non-zero")
        expected_revision, committed_revision, object_bytes, object_crc = (
            struct.unpack_from("<IIII", mutation, 4)
        )
        if expected_revision == 0xFFFFFFFF or committed_revision != expected_revision + 1:
            raise ValidationFailure("transaction mutation revision is invalid")
        try:
            record_id = mutation[20:52].decode("ascii")
        except UnicodeDecodeError as error:
            raise ValidationFailure("transaction record ID is not ASCII") from error
        if RECORD_ID_RE.fullmatch(record_id) is None:
            raise ValidationFailure("transaction record ID is invalid")
        object_name = _decode_fixed_name(mutation[52:64], "object file")
        if mutation_type == 1:
            has_delete = True
            if object_bytes != 0 or object_crc != 0 or object_name:
                raise ValidationFailure("delete mutation contains an object reference")
        else:
            has_upsert = True
            if object_bytes == 0 or not object_name:
                raise ValidationFailure("upsert mutation has no object reference")
            _verify_referenced_file(
                root / "RECORDS" / object_name,
                object_bytes,
                object_crc,
                "record object",
            )
            report.referenced_object_count += 1

    if record_type == 1 and not has_upsert:
        raise ValidationFailure("commit payload contains no upsert")
    if record_type == 2 and (not has_delete or has_upsert):
        raise ValidationFailure("tombstone payload must contain only deletions")


def _validate_checkpoint_payload(
    root: Path,
    payload: bytes,
    transaction_id: int,
    previous_transaction_id: int,
    segment_number: int,
    report: ValidationReport,
) -> None:
    if len(payload) != 64 or payload[:4] != b"PJCP":
        raise ValidationFailure("checkpoint payload is truncated or has bad magic")
    schema, payload_bytes = struct.unpack_from("<HH", payload, 4)
    if schema != 1:
        raise ValidationFailure(f"unsupported checkpoint payload schema {schema}")
    if payload_bytes != 64 or any(payload[44:64]):
        raise ValidationFailure("checkpoint payload header/reserved bytes are invalid")
    last_transaction_id = struct.unpack_from("<Q", payload, 8)[0]
    encoded_segment, _live_count, index_bytes, index_crc = struct.unpack_from(
        "<IIII", payload, 16
    )
    index_name = _decode_fixed_name(payload[32:44], "index file")
    if (
        last_transaction_id != previous_transaction_id
        or last_transaction_id >= transaction_id
        or encoded_segment != segment_number
        or index_bytes == 0
        or not index_name
    ):
        raise ValidationFailure("checkpoint identity or index reference is invalid")
    index_data = _verify_referenced_file(
        root / "INDEX" / index_name, index_bytes, index_crc, "checkpoint index"
    )
    assert report.dataset_id is not None
    _validate_index_snapshot(
        index_data, report.dataset_id, last_transaction_id, _live_count
    )
    report.checkpoint_count += 1


def _segment_paths(journal_dir: Path) -> list[tuple[int, Path]]:
    if not journal_dir.exists():
        return []
    if journal_dir.is_symlink() or not journal_dir.is_dir():
        raise ValidationFailure("JOURNAL is not a regular directory")
    segments: list[tuple[int, Path]] = []
    for entry in journal_dir.iterdir():
        match = SEGMENT_RE.fullmatch(entry.name)
        if match is None:
            raise ValidationFailure(f"unexpected JOURNAL entry: {entry.name}")
        number = int(match.group(1))
        if number < 1:
            raise ValidationFailure(f"invalid journal segment number: {entry.name}")
        segments.append((number, entry))
    segments.sort()
    if [number for number, _ in segments] != list(range(1, len(segments) + 1)):
        raise ValidationFailure("journal segment sequence contains a gap")
    return segments


def _validate_journal(root: Path, dataset_id: str, report: ValidationReport) -> None:
    segments = _segment_paths(root / "JOURNAL")
    report.segment_count = len(segments)
    previous_transaction = 0
    dataset_bytes = dataset_id.encode("ascii")
    for segment_number, path in segments:
        data = _read_regular_file(path, JOURNAL_SEGMENT_MAX_BYTES)
        if not data:
            raise ValidationFailure(f"journal segment is empty: {path.name}")
        offset = 0
        while offset < len(data):
            remaining = len(data) - offset
            if remaining < JOURNAL_HEADER_BYTES:
                raise ValidationFailure(f"truncated journal tail in {path.name} at {offset}")
            header = data[offset : offset + JOURNAL_HEADER_BYTES]
            if header[:4] != b"PJTL":
                raise ValidationFailure(f"journal magic mismatch in {path.name} at {offset}")
            schema, header_bytes = struct.unpack_from("<HH", header, 4)
            if schema != 1:
                raise ValidationFailure(f"unsupported journal schema {schema}")
            total_bytes, payload_bytes = struct.unpack_from("<II", header, 8)
            if (
                header_bytes != JOURNAL_HEADER_BYTES
                or payload_bytes > JOURNAL_MAX_PAYLOAD
                or total_bytes != JOURNAL_HEADER_BYTES + payload_bytes
            ):
                raise ValidationFailure("journal frame length is invalid")
            if remaining < total_bytes:
                raise ValidationFailure(f"truncated journal tail in {path.name} at {offset}")
            record_type = header[16]
            if record_type not in (1, 2, 3):
                raise ValidationFailure(f"unsupported journal record type {record_type}")
            if any(header[17:24]):
                raise ValidationFailure("journal reserved bytes are non-zero")
            transaction_id = struct.unpack_from("<Q", header, 24)[0]
            if transaction_id == 0 or transaction_id <= previous_transaction:
                raise ValidationFailure("journal transaction IDs are not strictly increasing")
            if header[32:64] != dataset_bytes:
                raise ValidationFailure("journal frame belongs to another dataset")
            payload = data[
                offset + JOURNAL_HEADER_BYTES : offset + total_bytes
            ]
            payload_crc, frame_crc = struct.unpack_from("<II", header, 64)
            if _crc32(payload) != payload_crc:
                raise ValidationFailure("journal payload CRC32 mismatch")
            if _crc32(payload, _crc32(header[:68])) != frame_crc:
                raise ValidationFailure("journal frame CRC32 mismatch")
            if record_type in (1, 2):
                _validate_transaction_payload(root, payload, record_type, report)
            else:
                _validate_checkpoint_payload(
                    root,
                    payload,
                    transaction_id,
                    previous_transaction,
                    segment_number,
                    report,
                )
            previous_transaction = transaction_id
            report.journal_record_count += 1
            offset += total_bytes


def validate_backup(path: Path | str) -> ValidationReport:
    report = ValidationReport(root=str(path))
    try:
        root = _application_root(Path(path))
        report.root = str(root)
        dataset_id, _generation = _select_manifest(root, report)
        _validate_journal(root, dataset_id, report)
        report.valid = True
    except (OSError, ValidationFailure) as error:
        report.errors.append(str(error))
    return report


def _human_lines(report: ValidationReport) -> Iterable[str]:
    if report.valid:
        yield "VALID: backup is internally consistent"
        yield f"dataset_id: {report.dataset_id}"
        yield f"manifest: generation {report.generation}, slot {report.selected_slot}, {report.manifest_state}"
        yield f"journal: {report.segment_count} segment(s), {report.journal_record_count} record(s)"
        yield f"references: {report.referenced_object_count} object(s), {report.checkpoint_count} checkpoint(s)"
    else:
        yield "INVALID: backup must not be restored as a trusted dataset"
    for warning in report.warnings:
        yield f"WARNING: {warning}"
    for error in report.errors:
        yield f"ERROR: {error}"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Validate a copied PREGJNL directory without modifying it."
    )
    parser.add_argument("path", type=Path, help="PREGJNL or its parent backup directory")
    parser.add_argument("--json", action="store_true", help="emit machine-readable JSON")
    args = parser.parse_args(argv)
    report = validate_backup(args.path)
    if args.json:
        print(json.dumps(asdict(report), ensure_ascii=False, indent=2))
    else:
        print("\n".join(_human_lines(report)))
    return 0 if report.valid else 1


if __name__ == "__main__":
    sys.exit(main())
