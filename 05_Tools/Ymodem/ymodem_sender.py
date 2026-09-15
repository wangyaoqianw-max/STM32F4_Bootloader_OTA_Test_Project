"""AI-callable command-line YMODEM sender."""

import argparse
import json
from pathlib import Path
import re
import sys

import config
from port_detector import PortCriteria, PortDependencyError, PortDetector, PortSelectionError
from serial_transport import SerialTransport, TransportError, TransportOpenError
from ymodem_protocol import (
    ReceiverCancelledError,
    ReceiverTimeoutError,
    TransferFailedError,
    YModemSender,
)


class InvalidArgumentError(ValueError):
    """Raised when argparse input cannot form a valid command."""


class CommandArgumentParser(argparse.ArgumentParser):
    def error(self, message):
        raise InvalidArgumentError(message)


def _positive_int(value):
    try:
        parsed = int(value, 10)
    except ValueError as error:
        raise argparse.ArgumentTypeError("must be a positive integer") from error
    if parsed <= 0:
        raise argparse.ArgumentTypeError("must be a positive integer")
    return parsed


def _nonnegative_int(value):
    try:
        parsed = int(value, 10)
    except ValueError as error:
        raise argparse.ArgumentTypeError("must be a non-negative integer") from error
    if parsed < 0:
        raise argparse.ArgumentTypeError("must be a non-negative integer")
    return parsed


def _positive_float(value):
    try:
        parsed = float(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("must be a positive number") from error
    if parsed <= 0:
        raise argparse.ArgumentTypeError("must be a positive number")
    return parsed


def _port_id(value):
    try:
        parsed = int(value, 0)
    except ValueError as error:
        raise argparse.ArgumentTypeError("must be a decimal or 0x-prefixed USB ID") from error
    if not 0 <= parsed <= 0xFFFF:
        raise argparse.ArgumentTypeError("must be in range 0..65535")
    return parsed


def _add_port_filters(parser):
    parser.add_argument("--vid", type=_port_id, help="filter USB vendor ID, for example 0x10C4")
    parser.add_argument("--pid", type=_port_id, help="filter USB product ID, for example 0xEA60")
    parser.add_argument("--match", action="append", default=None, help="case-insensitive port text filter; repeatable")


def build_parser():
    parser = CommandArgumentParser(
        prog="ymodem_sender",
        description="Non-interactive YMODEM-1K firmware sender for Codex/GPT, PowerShell, and CMD.",
    )
    subparsers = parser.add_subparsers(dest="command")

    devices = subparsers.add_parser("devices", help="list available serial ports")
    _add_port_filters(devices)
    devices.add_argument("--json", action="store_true", help="write one machine-readable JSON result")

    send = subparsers.add_parser("send", help="send one firmware file")
    send.add_argument("file", help="firmware file, such as app.bin or an S05 .img")
    send.add_argument("--port", help="manual serial port override, for example COM7")
    send.add_argument("--baud", type=_positive_int, default=config.DEFAULT_BAUDRATE)
    send.add_argument("--timeout", type=_positive_float, default=config.DEFAULT_TIMEOUT_SECONDS)
    send.add_argument("--retries", type=_nonnegative_int, default=config.DEFAULT_MAX_RETRIES)
    _add_port_filters(send)
    send.add_argument("--json", action="store_true", help="write one machine-readable JSON result")
    return parser


def parse_args(argv=None):
    return build_parser().parse_args(argv)


def _criteria(args):
    vid = args.vid if args.vid is not None else config.DEFAULT_PORT_VID
    pid = args.pid if args.pid is not None else config.DEFAULT_PORT_PID
    matches = tuple(args.match) if args.match is not None else tuple(config.DEFAULT_PORT_MATCHES)
    return PortCriteria(vid=vid, pid=pid, matches=matches)


def _port_record(info):
    fields = (
        "device",
        "name",
        "description",
        "manufacturer",
        "product",
        "interface",
        "hwid",
        "vid",
        "pid",
        "serial_number",
        "location",
    )
    return {field: getattr(info, field, None) for field in fields}


def _result(command, code, phase, *, port=None, file_path=None, stats=None, error=None, devices=None):
    result = {
        "ok": code == config.EXIT_SUCCESS,
        "command": command,
        "exit_code": code,
        "port": port,
        "file": str(file_path) if file_path is not None else None,
        "bytes_sent": getattr(stats, "bytes_sent", 0),
        "blocks_sent": getattr(stats, "blocks_sent", 0),
        "retries": getattr(stats, "retry_count", 0),
        "phase": phase,
        "error": str(error) if error is not None else None,
    }
    if devices is not None:
        result["devices"] = devices
    return result


def _emit_result(result, json_mode):
    if json_mode:
        print(json.dumps(result, ensure_ascii=False, sort_keys=True))


def _logger(json_mode):
    stream = sys.stderr if json_mode else sys.stdout

    def log(message):
        print(message, file=stream)

    return log


def _error(result, json_mode):
    if result["error"]:
        print(f"[ERROR] {result['error']}", file=sys.stderr)
    _emit_result(result, json_mode)
    return result["exit_code"]


def _valid_port(port):
    return re.fullmatch(r"COM[1-9][0-9]*", port.upper()) is not None


def _run_devices(args):
    json_mode = args.json
    log = _logger(json_mode)
    detector = PortDetector()
    try:
        log("[PORT] Scanning serial ports...")
        all_ports = detector.list_all()
        for info in all_ports:
            log(f"[PORT] {detector.describe(info)}")
        matches = detector.scan(_criteria(args))
        result = _result(
            "devices",
            config.EXIT_SUCCESS,
            "complete",
            devices=[_port_record(info) for info in matches],
        )
        _emit_result(result, json_mode)
        return config.EXIT_SUCCESS
    except PortDependencyError as error:
        return _error(_result("devices", config.EXIT_SERIAL_OPEN_FAILED, "port_scan", error=error), json_mode)


def _run_send(args):
    json_mode = args.json
    log = _logger(json_mode)
    path = Path(args.file)
    if not path.is_file():
        return _error(_result("send", config.EXIT_INVALID_ARGUMENT, "argument", file_path=path, error=f"firmware file does not exist: {path}"), json_mode)
    if path.stat().st_size == 0:
        return _error(_result("send", config.EXIT_INVALID_ARGUMENT, "argument", file_path=path, error="firmware file must not be empty"), json_mode)

    port = None
    detector = PortDetector()
    try:
        if args.port is not None:
            if not _valid_port(args.port):
                raise InvalidArgumentError(f"invalid COM port: {args.port}")
            port = args.port.upper()
        else:
            log("[PORT] Scanning serial ports...")
            all_ports = detector.list_all()
            for info in all_ports:
                log(f"[PORT] {detector.describe(info)}")
            port = detector.select(_criteria(args)).device
        log(f"[PORT] Selected {port}")
    except InvalidArgumentError as error:
        return _error(_result("send", config.EXIT_INVALID_ARGUMENT, "argument", file_path=path, error=error), json_mode)
    except PortSelectionError as error:
        return _error(_result("send", config.EXIT_PORT_NOT_FOUND, "port_selection", file_path=path, error=error), json_mode)
    except PortDependencyError as error:
        return _error(_result("send", config.EXIT_SERIAL_OPEN_FAILED, "port_scan", file_path=path, error=error), json_mode)

    transport = SerialTransport(port, args.baud, write_timeout=args.timeout)
    try:
        transport.open()
    except TransportOpenError as error:
        return _error(_result("send", config.EXIT_SERIAL_OPEN_FAILED, "serial_open", port=port, file_path=path, error=error), json_mode)

    try:
        sender = YModemSender(transport, args.timeout, args.retries, logger=log)
        stats = sender.send(path)
        log(f"[DONE] Transfer successful bytes={stats.bytes_sent} blocks={stats.blocks_sent} retries={stats.retry_count}")
        result = _result("send", config.EXIT_SUCCESS, "complete", port=port, file_path=path, stats=stats)
        _emit_result(result, json_mode)
        return config.EXIT_SUCCESS
    except ReceiverTimeoutError as error:
        return _error(_result("send", config.EXIT_RECEIVER_TIMEOUT, "wait_receiver", port=port, file_path=path, error=error), json_mode)
    except ReceiverCancelledError as error:
        return _error(_result("send", config.EXIT_RECEIVER_CANCELLED, "transfer", port=port, file_path=path, error=error), json_mode)
    except (TransferFailedError, TransportError) as error:
        return _error(_result("send", config.EXIT_TRANSFER_FAILED, "transfer", port=port, file_path=path, error=error), json_mode)
    except ValueError as error:
        return _error(_result("send", config.EXIT_INVALID_ARGUMENT, "argument", port=port, file_path=path, error=error), json_mode)
    finally:
        transport.close()


def main(argv=None):
    try:
        args = parse_args(argv)
    except InvalidArgumentError as error:
        print(f"[ERROR] {error}", file=sys.stderr)
        return config.EXIT_INVALID_ARGUMENT
    except SystemExit as error:
        return int(error.code)

    if args.command == "devices":
        return _run_devices(args)
    if args.command == "send":
        return _run_send(args)
    print("[ERROR] a subcommand is required: devices or send", file=sys.stderr)
    return config.EXIT_INVALID_ARGUMENT


if __name__ == "__main__":
    sys.exit(main())
