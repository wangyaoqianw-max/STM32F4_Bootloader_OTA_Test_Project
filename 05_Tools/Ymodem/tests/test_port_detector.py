import sys
from pathlib import Path
import unittest


MODULE_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(MODULE_ROOT))

try:
    from port_detector import PortCriteria, PortDetector, PortSelectionError
    IMPORT_ERROR = None
except ImportError as error:
    PortCriteria = None
    PortDetector = None
    PortSelectionError = None
    IMPORT_ERROR = error


class FakePort:
    def __init__(self, device, description, vid=None, pid=None, manufacturer=None, product=None, interface=None, hwid=""):
        self.device = device
        self.name = device
        self.description = description
        self.vid = vid
        self.pid = pid
        self.manufacturer = manufacturer
        self.product = product
        self.interface = interface
        self.hwid = hwid


class PortDetectorTest(unittest.TestCase):
    def setUp(self):
        if IMPORT_ERROR is not None:
            self.fail(f"port detector module is not available: {IMPORT_ERROR}")
        self.ports = [
            FakePort("COM3", "Bluetooth Link"),
            FakePort(
                "COM9",
                "USB Serial Device",
                vid=0x10C4,
                pid=0xEA60,
                manufacturer="Silicon Labs",
                product="CP210x",
                hwid="USB VID:PID=10C4:EA60",
            ),
        ]
        self.detector = PortDetector(lambda: self.ports)

    def test_vid_pid_selects_target_port(self):
        candidates = self.detector.scan(PortCriteria(vid=0x10C4, pid=0xEA60))
        self.assertEqual([port.device for port in candidates], ["COM9"])
        self.assertEqual(self.detector.select(PortCriteria(vid=0x10C4, pid=0xEA60)).device, "COM9")

    def test_text_match_checks_description_and_hwid(self):
        candidates = self.detector.scan(PortCriteria(matches=("silicon",)))
        self.assertEqual([port.device for port in candidates], ["COM9"])
        candidates = self.detector.scan(PortCriteria(matches=("ea60",)))
        self.assertEqual([port.device for port in candidates], ["COM9"])

    def test_multiple_candidates_are_rejected(self):
        with self.assertRaises(PortSelectionError) as context:
            self.detector.select(PortCriteria())
        self.assertIn("COM3", str(context.exception))
        self.assertIn("COM9", str(context.exception))

    def test_no_candidates_are_rejected(self):
        detector = PortDetector(lambda: [])
        with self.assertRaises(PortSelectionError) as context:
            detector.select(PortCriteria(vid=1, pid=2))
        self.assertIn("no matching serial port", str(context.exception).lower())


if __name__ == "__main__":
    unittest.main()
