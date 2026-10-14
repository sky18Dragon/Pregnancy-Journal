#!/usr/bin/env python3

import importlib.util
import shutil
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "validate_sd_backup", PROJECT_ROOT / "tools" / "validate_sd_backup.py"
)
assert SPEC and SPEC.loader
VALIDATOR = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = VALIDATOR
SPEC.loader.exec_module(VALIDATOR)

DATASET_A = "0123456789abcdef0123456789abcdef"
DATASET_B = "fedcba9876543210fedcba9876543210"
RECORD_ID = "00000000000000000000000000000001"


def crc32(data: bytes, value: int = 0) -> int:
    return zlib.crc32(data, value) & 0xFFFFFFFF


def manifest(dataset: str, generation: int) -> bytes:
    payload = (
        f'{{"magic":"PJMF","schema":1,"dataset_id":"{dataset}",'
        f'"generation":{generation}}}'
    ).encode("ascii")
    return payload[:-1] + f',"crc32":"{crc32(payload):08X}"}}\n'.encode("ascii")


def fixed_name(name: str) -> bytes:
    return name.encode("ascii").ljust(12, b"\0")


def transaction_payload(object_data: bytes) -> bytes:
    mutation = bytearray(64)
    mutation[0] = 0
    mutation[1] = 7
    struct.pack_into("<IIII", mutation, 4, 0, 1, len(object_data), crc32(object_data))
    mutation[20:52] = RECORD_ID.encode("ascii")
    mutation[52:64] = fixed_name("OBJ00001.BIN")
    header = bytearray(16)
    header[:4] = b"PJTX"
    struct.pack_into("<HHH", header, 4, 1, 16, 80)
    header[10] = 1
    return bytes(header + mutation)


def checkpoint_payload(snapshot_transaction_id: int, index_data: bytes) -> bytes:
    payload = bytearray(64)
    payload[:4] = b"PJCP"
    struct.pack_into("<HHQIIII", payload, 4, 1, 64, snapshot_transaction_id, 1, 1, len(index_data), crc32(index_data))
    payload[32:44] = fixed_name("INDEX001.BIN")
    return bytes(payload)


def index_snapshot(dataset: str, transaction_id: int, object_data: bytes) -> bytes:
    entry = bytearray(64)
    entry[0] = 7
    struct.pack_into("<III", entry, 4, 1, len(object_data), crc32(object_data))
    entry[16:48] = RECORD_ID.encode("ascii")
    entry[48:60] = fixed_name("OBJ00001.BIN")
    header = bytearray(80)
    header[:4] = b"PJIX"
    struct.pack_into("<HHIII", header, 4, 1, 80, 144, 1, 1)
    struct.pack_into("<Q", header, 24, transaction_id)
    header[32:64] = dataset.encode("ascii")
    struct.pack_into("<I", header, 64, crc32(entry))
    struct.pack_into("<I", header, 68, crc32(entry, crc32(header[:68])))
    return bytes(header + entry)


def frame(dataset: str, record_type: int, transaction_id: int, payload: bytes) -> bytes:
    header = bytearray(72)
    header[:4] = b"PJTL"
    struct.pack_into("<HHII", header, 4, 1, 72, 72 + len(payload), len(payload))
    header[16] = record_type
    struct.pack_into("<Q", header, 24, transaction_id)
    header[32:64] = dataset.encode("ascii")
    struct.pack_into("<I", header, 64, crc32(payload))
    struct.pack_into("<I", header, 68, crc32(payload, crc32(header[:68])))
    return bytes(header) + payload


class BackupValidatorTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name) / "PREGJNL"
        (self.root / "MANIFEST").mkdir(parents=True)
        (self.root / "MANIFEST" / "MNF000A.JSN").write_bytes(manifest(DATASET_A, 1))

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def add_valid_journal(self) -> None:
        object_data = b"record-object"
        index_data = index_snapshot(DATASET_A, 1, object_data)
        (self.root / "RECORDS").mkdir()
        (self.root / "INDEX").mkdir()
        (self.root / "JOURNAL").mkdir()
        (self.root / "RECORDS" / "OBJ00001.BIN").write_bytes(object_data)
        (self.root / "INDEX" / "INDEX001.BIN").write_bytes(index_data)
        journal = frame(DATASET_A, 1, 1, transaction_payload(object_data))
        journal += frame(DATASET_A, 3, 2, checkpoint_payload(1, index_data))
        (self.root / "JOURNAL" / "TXN00001.LOG").write_bytes(journal)

    def test_empty_initialized_dataset_is_valid_but_degraded(self) -> None:
        report = VALIDATOR.validate_backup(self.root)
        self.assertTrue(report.valid, report.errors)
        self.assertEqual(report.manifest_state, "degraded")
        self.assertEqual(report.journal_record_count, 0)

    def test_complete_backup_validates_references(self) -> None:
        self.add_valid_journal()
        (self.root / "MANIFEST" / "MNF000B.JSN").write_bytes(manifest(DATASET_A, 2))
        report = VALIDATOR.validate_backup(self.root.parent)
        self.assertTrue(report.valid, report.errors)
        self.assertEqual(report.generation, 2)
        self.assertEqual(report.journal_record_count, 2)
        self.assertEqual(report.referenced_object_count, 1)
        self.assertEqual(report.checkpoint_count, 1)

    def test_unknown_manifest_schema_is_rejected(self) -> None:
        path = self.root / "MANIFEST" / "MNF000A.JSN"
        path.write_bytes(path.read_bytes().replace(b'"schema":1', b'"schema":2'))
        report = VALIDATOR.validate_backup(self.root)
        self.assertFalse(report.valid)
        self.assertIn("unsupported", report.errors[0])

    def test_cross_dataset_manifests_are_rejected(self) -> None:
        (self.root / "MANIFEST" / "MNF000B.JSN").write_bytes(manifest(DATASET_B, 2))
        report = VALIDATOR.validate_backup(self.root)
        self.assertFalse(report.valid)
        self.assertIn("different dataset", report.errors[0])

    def test_truncated_tail_and_segment_gap_are_rejected(self) -> None:
        self.add_valid_journal()
        segment = self.root / "JOURNAL" / "TXN00001.LOG"
        segment.write_bytes(segment.read_bytes()[:-1])
        report = VALIDATOR.validate_backup(self.root)
        self.assertFalse(report.valid)
        self.assertIn("truncated journal tail", report.errors[0])

        shutil.rmtree(self.root / "JOURNAL")
        (self.root / "JOURNAL").mkdir()
        (self.root / "JOURNAL" / "TXN00002.LOG").write_bytes(b"x")
        report = VALIDATOR.validate_backup(self.root)
        self.assertFalse(report.valid)
        self.assertIn("gap", report.errors[0])

    def test_corrupt_referenced_object_is_rejected(self) -> None:
        self.add_valid_journal()
        (self.root / "RECORDS" / "OBJ00001.BIN").write_bytes(b"changed-object")
        report = VALIDATOR.validate_backup(self.root)
        self.assertFalse(report.valid)
        self.assertRegex(report.errors[0], "length mismatch|CRC32 mismatch")

    def test_corrupt_checkpoint_index_schema_is_rejected(self) -> None:
        self.add_valid_journal()
        path = self.root / "INDEX" / "INDEX001.BIN"
        data = bytearray(path.read_bytes())
        struct.pack_into("<H", data, 4, 2)
        path.write_bytes(data)
        journal = self.root / "JOURNAL" / "TXN00001.LOG"
        journal_data = journal.read_bytes()
        index_crc = crc32(data)
        checkpoint_offset = len(frame(DATASET_A, 1, 1, transaction_payload(b"record-object")))
        checkpoint = bytearray(journal_data[checkpoint_offset:])
        struct.pack_into("<I", checkpoint, 72 + 28, index_crc)
        payload = bytes(checkpoint[72:])
        struct.pack_into("<I", checkpoint, 64, crc32(payload))
        struct.pack_into("<I", checkpoint, 68, crc32(payload, crc32(checkpoint[:68])))
        journal.write_bytes(journal_data[:checkpoint_offset] + checkpoint)
        report = VALIDATOR.validate_backup(self.root)
        self.assertFalse(report.valid)
        self.assertIn("unsupported checkpoint index schema", report.errors[0])

    def test_unknown_journal_entry_is_rejected(self) -> None:
        (self.root / "JOURNAL").mkdir()
        (self.root / "JOURNAL" / "README.TXT").write_text("unexpected")
        report = VALIDATOR.validate_backup(self.root)
        self.assertFalse(report.valid)
        self.assertIn("unexpected JOURNAL entry", report.errors[0])


if __name__ == "__main__":
    unittest.main()
