"""Automatic serial-port discovery with deterministic candidate selection."""

from dataclasses import dataclass


class PortSelectionError(Exception):
    """No unique serial port matched the requested criteria."""


class PortDependencyError(Exception):
    """pyserial is unavailable for port discovery."""


@dataclass(frozen=True)
class PortCriteria:
    vid: int | None = None
    pid: int | None = None
    matches: tuple[str, ...] = ()


def _default_provider():
    try:
        from serial.tools import list_ports
    except ImportError as error:
        raise PortDependencyError("pyserial is required for serial-port discovery") from error
    return list(list_ports.comports())


class PortDetector:
    def __init__(self, provider=None):
        self.provider = provider or _default_provider

    @staticmethod
    def describe(info) -> str:
        device = getattr(info, "device", None) or getattr(info, "name", "unknown")
        description = getattr(info, "description", None) or "unknown"
        vid = getattr(info, "vid", None)
        pid = getattr(info, "pid", None)
        identity = ""
        if vid is not None and pid is not None:
            identity = f" VID:PID={vid:04X}:{pid:04X}"
        return f"{device} - {description}{identity}"

    @staticmethod
    def _matches(info, criteria: PortCriteria) -> bool:
        if criteria.vid is not None and getattr(info, "vid", None) != criteria.vid:
            return False
        if criteria.pid is not None and getattr(info, "pid", None) != criteria.pid:
            return False
        if criteria.matches:
            fields = (
                getattr(info, "device", None),
                getattr(info, "name", None),
                getattr(info, "description", None),
                getattr(info, "manufacturer", None),
                getattr(info, "product", None),
                getattr(info, "interface", None),
                getattr(info, "hwid", None),
            )
            haystack = " ".join(str(field) for field in fields if field).casefold()
            if not any(pattern.casefold() in haystack for pattern in criteria.matches):
                return False
        return True

    def scan(self, criteria: PortCriteria) -> list:
        return [info for info in self.provider() if self._matches(info, criteria)]

    def select(self, criteria: PortCriteria):
        all_ports = list(self.provider())
        candidates = [info for info in all_ports if self._matches(info, criteria)]
        if len(candidates) == 1:
            return candidates[0]
        if not candidates:
            observed = "; ".join(self.describe(info) for info in all_ports) or "none"
            raise PortSelectionError(f"no matching serial port; observed: {observed}")
        choices = "; ".join(self.describe(info) for info in candidates)
        raise PortSelectionError(f"multiple matching serial ports; choose explicitly: {choices}")
