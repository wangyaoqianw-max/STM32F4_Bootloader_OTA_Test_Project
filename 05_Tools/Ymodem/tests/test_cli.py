import io
import json
import sys
import tempfile
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import patch


MODULE_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(MODULE_ROOT))

try:
    import ymodem_sender
    from port_detector import PortSelectionError
    from serial_transport import TransportOpenError
    from ymodem_protocol import ReceiverCancelledError, ReceiverTimeoutError, TransferFailedError
    IMPORT_ERROR = None
except ImportError as error:
    ymodem_sender = None
    PortSelectionError = None
    TransportOpenError = None
    ReceiverCancelledError = None
    ReceiverTimeoutError = None
    TransferFailedError = None
    IMPORT_ERROR = error


class FakePort:
    device = "COM9"
    name = "COM9"
    description = "USB Serial Device"
    manufacturer = "Test"
    product = "Test UART"
    interface = None
    hwid = "USB VID:PID=10C4:EA60"
    vid = 0x10C4
    pid = 0xEA60
    serial_number = "TEST"
    location = None


class FakeDetector:
    def __init__(self, error=None):
        self.error = error
        self.port = FakePort()

    def list_all(self):
        return [self.port]

    def scan(self, criteria):
        del criteria
        if self.error is not None:
            raise self.error
        return [self.port]

    def select(self, criteria):
        del criteria
        if self.error is not None:
            raise self.error
        return self.port

    @staticmethod
    def describe(port):
        return f"{port.device} - {port.description}"


class FakeTransport:
    def __init__(self, *args, **kwargs):
        del args, kwargs
        self.closed = False

    def open(self):
        return None

    def close(self):
        self.closed = True


class FakeSender:
    def __init__(self, transport, timeout, max_retries, logger):
        del transport, timeout, max_retries, logger

    def send(self, file_path):
        return SimpleNamespace(bytes_sent=Path(file_path).stat().st_size, blocks_sent=1, retry_count=0)


class CliTest(unittest.TestCase):
    def setUp(self):
        if IMPORT_ERROR is not None:
            self.fail(f"CLI module is not available: {IMPORT_ERROR}")

    def test_devices_and_send_subcommands_parse(self):
        devices = ymodem_sender.parse_args(["devices", "--json"])
        send = ymodem_sender.parse_args(["send", "app.bin", "--port", "COM7", "--baud", "115200"])
        self.assertEqual(devices.command, "devices")
        self.assertTrue(devices.json)
        self.assertEqual(send.command, "send")
        self.assertEqual(send.file, "app.bin")
        self.assertEqual(send.port, "COM7")
        self.assertEqual(send.baud, 115200)

    def test_help_returns_success_and_mentions_subcommands(self):
        stdout = io.StringIO()
        with redirect_stdout(stdout):
            code = ymodem_sender.main(["--help"])
        self.assertEqual(code, 0)
        self.assertIn("devices", stdout.getvalue())
        self.assertIn("send", stdout.getvalue())

    def test_devices_json_is_one_machine_readable_result(self):
        stdout = io.StringIO()
        stderr = io.StringIO()
        with patch.object(ymodem_sender, "PortDetector", return_value=FakeDetector()), redirect_stdout(stdout), redirect_stderr(stderr):
            code = ymodem_sender.main(["devices", "--json"])
        result = json.loads(stdout.getvalue())
        self.assertEqual(code, 0)
        self.assertTrue(result["ok"])
        self.assertEqual(result["devices"][0]["device"], "COM9")
        self.assertEqual(len(stdout.getvalue().splitlines()), 1)

    def test_send_success_json_returns_stats(self):
        with tempfile.TemporaryDirectory() as directory:
            file_path = Path(directory) / "app.bin"
            file_path.write_bytes(b"hello")
            stdout = io.StringIO()
            stderr = io.StringIO()
            with patch.object(ymodem_sender, "SerialTransport", FakeTransport), patch.object(ymodem_sender, "YModemSender", FakeSender), redirect_stdout(stdout), redirect_stderr(stderr):
                code = ymodem_sender.main(["send", str(file_path), "--port", "COM9", "--json"])
        result = json.loads(stdout.getvalue())
        self.assertEqual(code, 0)
        self.assertTrue(result["ok"])
        self.assertEqual(result["bytes_sent"], 5)
        self.assertEqual(result["port"], "COM9")
        self.assertEqual(len(stdout.getvalue().splitlines()), 1)

    def test_invalid_arguments_return_one(self):
        self.assertEqual(ymodem_sender.main(["send"]), 1)
        self.assertEqual(ymodem_sender.main(["send", "missing.bin", "--port", "BAD"]), 1)

    def test_error_classes_map_to_stable_exit_codes(self):
        error_cases = (
            (PortSelectionError("ambiguous"), 2, "auto"),
            (TransportOpenError("busy"), 3, "open"),
            (ReceiverTimeoutError("timeout"), 4, "send"),
            (TransferFailedError("failed"), 5, "send"),
            (ReceiverCancelledError("cancelled"), 6, "send"),
        )
        with tempfile.TemporaryDirectory() as directory:
            file_path = Path(directory) / "app.bin"
            file_path.write_bytes(b"hello")
            for error, expected_code, phase in error_cases:
                with self.subTest(expected_code=expected_code):
                    stdout = io.StringIO()
                    stderr = io.StringIO()
                    detector = FakeDetector(error if phase == "auto" else None)
                    if expected_code == 3:
                        class FailingTransport(FakeTransport):
                            def open(self):
                                raise error

                        transport_class = FailingTransport
                        sender_factory = FakeSender
                    else:
                        transport_class = FakeTransport
                        sender_factory = unittest.mock.Mock(side_effect=error)
                    with patch.object(ymodem_sender, "PortDetector", return_value=detector), patch.object(ymodem_sender, "SerialTransport", transport_class), patch.object(ymodem_sender, "YModemSender", sender_factory), redirect_stdout(stdout), redirect_stderr(stderr):
                        arguments = ["send", str(file_path), "--port", "COM9", "--json"] if phase != "auto" else ["send", str(file_path), "--json"]
                        code = ymodem_sender.main(arguments)
                    result = json.loads(stdout.getvalue())
                    self.assertEqual(code, expected_code)
                    self.assertFalse(result["ok"])


if __name__ == "__main__":
    unittest.main()
