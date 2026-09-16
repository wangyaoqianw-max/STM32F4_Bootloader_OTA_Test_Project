from pathlib import Path
import sys
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import mock_open
from unittest.mock import patch


SCRIPT_ROOT = Path(__file__).resolve().parents[3] / "05_Tools" / "Scripts"
sys.path.insert(0, str(SCRIPT_ROOT))

from s04_persistence_test import build_rtt_logger_arguments
from s04_persistence_test import calculate_reconnect_delay
from s04_persistence_test import classify_rtt_logger_diagnostic
from s04_persistence_test import is_rtt_logger_stalled
from s04_persistence_test import merge_chunks
from s04_persistence_test import parse_symbol_address
from s04_persistence_test import POWER_CYCLE_EVENT_MARKER
from s04_persistence_test import validate_persistence_test_image

from s04_persistence_log import compare_snapshots
from s04_persistence_log import evaluate_snapshots
from s04_persistence_log import parse_snapshot_line
from s04_persistence_log import main


BASELINE_LINE = (
    "[S04-PERSIST] SNAPSHOT image_validation=2 image_size=55820 "
    "image_crc=0x3ED1C72E metadata_a_valid=1 metadata_b_valid=0 "
    "selected_copy=1 sequence=7 active_slot=255 confirmed_slot=1 "
    "slot_a_state=0 slot_b_state=1 confirmed_version=1.1.0"
)


class S04PersistenceLogTest(unittest.TestCase):
    def test_parse_snapshot_line_returns_persistent_fields(self):
        snapshot = parse_snapshot_line(BASELINE_LINE)

        self.assertEqual(snapshot["image_validation"], 2)
        self.assertEqual(snapshot["image_size"], 55820)
        self.assertEqual(snapshot["image_crc"], 0x3ED1C72E)
        self.assertEqual(snapshot["selected_copy"], 1)
        self.assertEqual(snapshot["sequence"], 7)
        self.assertEqual(snapshot["slot_b_state"], 1)
        self.assertEqual(snapshot["confirmed_version"], (1, 1, 0))

    def test_equal_snapshots_are_persistence_pass(self):
        result = evaluate_snapshots([BASELINE_LINE, BASELINE_LINE])

        self.assertEqual(result["status"], "PASS")
        self.assertEqual(result["before"]["sequence"], 7)
        self.assertEqual(result["after"]["sequence"], 7)

    def test_changed_sequence_is_persistence_fail(self):
        changed_line = BASELINE_LINE.replace("sequence=7", "sequence=8")

        result = compare_snapshots(
            parse_snapshot_line(BASELINE_LINE),
            parse_snapshot_line(changed_line),
        )

        self.assertEqual(result["status"], "FAIL")
        self.assertIn("sequence", result["reason"])

    def test_missing_post_event_snapshot_is_not_ready(self):
        result = evaluate_snapshots([BASELINE_LINE])

        self.assertEqual(result["status"], "NOT_READY")

    def test_power_cycle_requires_snapshot_after_confirmed_boot(self):
        event_marker = "[S04-PERSIST] POWER_CYCLE_BOOT_CONFIRMED"

        result = evaluate_snapshots(
            [BASELINE_LINE, BASELINE_LINE, event_marker],
            require_post_event=True,
        )

        self.assertEqual(result["status"], "NOT_READY")

    def test_power_cycle_compares_snapshot_after_confirmed_boot(self):
        event_marker = "[S04-PERSIST] POWER_CYCLE_BOOT_CONFIRMED"

        result = evaluate_snapshots(
            [BASELINE_LINE, event_marker, BASELINE_LINE],
            require_post_event=True,
        )

        self.assertEqual(result["status"], "PASS")

    def test_merge_chunks_inserts_power_cycle_event_boundary(self):
        with TemporaryDirectory() as temporary_directory:
            chunk = Path(temporary_directory) / "chunk.log"
            output = Path(temporary_directory) / "output.log"
            boot_line = "[S04-PERSIST] read-only test start"
            content = "{}\n{}\n{}\n".format(
                BASELINE_LINE,
                boot_line,
                BASELINE_LINE,
            ).encode("utf-8")
            chunk.write_bytes(content)

            merge_chunks(
                [chunk],
                output,
                chunk,
                content.index(boot_line.encode("utf-8")) + len(boot_line),
            )

            merged = output.read_text(encoding="utf-8")
            self.assertIn(POWER_CYCLE_EVENT_MARKER.decode("utf-8"), merged)
            self.assertEqual(
                evaluate_snapshots(
                    merged.splitlines(),
                    require_post_event=True,
                )["status"],
                "PASS",
            )

    def test_invalid_snapshot_enum_is_rejected(self):
        invalid_line = BASELINE_LINE.replace("selected_copy=1", "selected_copy=3")

        with self.assertRaises(ValueError):
            parse_snapshot_line(invalid_line)

    @patch("builtins.open", new_callable=mock_open, read_data=BASELINE_LINE + "\n" + BASELINE_LINE)
    def test_cli_returns_zero_for_persistence_pass(self, mocked_open):
        result = main(["--log", "s04.log"])

        self.assertEqual(result, 0)
        mocked_open.assert_called_once_with(
            "s04.log",
            "r",
            encoding="utf-8",
            errors="replace",
        )

    def test_parse_symbol_address_returns_rtt_control_block_address(self):
        address = parse_symbol_address(b"$1 = 0x2000a820\r\n")

        self.assertEqual(address, 0x2000A820)

    def test_logger_arguments_use_fixed_rtt_address(self):
        arguments = build_rtt_logger_arguments(
            "JLinkRTTLogger.exe",
            "STM32F411CE",
            "SWD",
            4000,
            0,
            0x2000A820,
            "capture.log",
        )

        self.assertIn("-RTTAddress", arguments)
        self.assertIn("0x2000A820", arguments)

    def test_logger_diagnostic_classifies_rtt_control_block_failure(self):
        state = classify_rtt_logger_diagnostic(
            b"Searching for RTT Control Block...RTT Control Block not found."
        )

        self.assertEqual(state, "RTT_CONTROL_BLOCK_NOT_FOUND")

    def test_reconnect_delay_increases_with_bounded_backoff(self):
        first = calculate_reconnect_delay(1)
        second = calculate_reconnect_delay(2)
        later = calculate_reconnect_delay(10)

        self.assertLess(first, second)
        self.assertLessEqual(later, 4.0)

    def test_active_logger_without_new_data_is_detected_as_stalled(self):
        self.assertTrue(is_rtt_logger_stalled(10.0, 15.1, 5.0))
        self.assertFalse(is_rtt_logger_stalled(10.0, 14.9, 5.0))

    @patch("s04_persistence_test.subprocess.run")
    def test_persistence_axf_must_contain_board_test_symbol(self, mocked_run):
        mocked_run.return_value.returncode = 1
        mocked_run.return_value.stdout = b"No symbol app_s04_persistence_test_run"
        mocked_run.return_value.stderr = b""

        with self.assertRaises(RuntimeError):
            validate_persistence_test_image("arm-none-eabi-gdb.exe", "OTA_APP.axf")


if __name__ == "__main__":
    unittest.main()
