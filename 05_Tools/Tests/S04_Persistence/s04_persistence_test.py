"""Run S04 reset and power-cycle persistence board tests."""

import argparse
import os
from pathlib import Path
import re
import socket
import subprocess
import sys
import tempfile
import time


SNAPSHOT_MARKER = b"[S04-PERSIST] SNAPSHOT"
BOOT_MARKER = b"[S04-PERSIST] read-only test start"
RESET_RTT_WINDOW_SECONDS = 4
RTT_RECONNECT_INITIAL_DELAY_SECONDS = 1.0
RTT_RECONNECT_MAX_DELAY_SECONDS = 4.0
RTT_STALL_TIMEOUT_SECONDS = 5.0
RTT_SYMBOL_ADDRESS_PATTERN = re.compile(rb"\$\d+\s*=\s*0x([0-9a-fA-F]+)")
PERSISTENCE_TEST_SYMBOL = "app_s04_persistence_test_run"
POWER_CYCLE_EVENT_MARKER = b"[S04-PERSIST] POWER_CYCLE_BOOT_CONFIRMED"
JLINK_LOCK_FILE_NAME = "toolkit_jlink.lock"


def parse_arguments(arguments=None):
    """Parse the tool paths and board-test parameters supplied by the BAT entrypoint."""
    parser = argparse.ArgumentParser(description="Run S04 persistence board tests")
    parser.add_argument("--mode", choices=("reset", "power-cycle"), required=True)
    parser.add_argument("--gdb", required=True)
    parser.add_argument("--server", default="")
    parser.add_argument("--rtt-logger", required=True)
    parser.add_argument("--axf", required=True)
    parser.add_argument("--gdb-script", default="")
    parser.add_argument("--parser", required=True)
    parser.add_argument("--device", required=True)
    parser.add_argument("--interface", required=True)
    parser.add_argument("--speed", type=int, required=True)
    parser.add_argument("--port", type=int, required=True)
    parser.add_argument("--channel", type=int, required=True)
    parser.add_argument("--start-timeout", type=int, required=True)
    parser.add_argument("--capture-seconds", type=int, required=True)
    parser.add_argument("--server-log", required=True)
    parser.add_argument("--output-log", required=True)
    parser.add_argument("--log-directory", required=True)
    parser.add_argument("--serial-port", required=True)
    return parser.parse_args(arguments)


def require_file(path_text, description):
    """Raise an actionable error when a required local file is absent."""
    path = Path(path_text)
    if not path.is_file():
        raise FileNotFoundError("{} not found: {}".format(description, path))
    return path


def remove_file(path):
    """Remove one generated output file when it exists."""
    try:
        Path(path).unlink()
    except FileNotFoundError:
        pass


def parse_symbol_address(output):
    """Parse the hexadecimal address printed by GDB for a symbol."""
    if isinstance(output, str):
        output = output.encode("ascii", errors="ignore")
    match = RTT_SYMBOL_ADDRESS_PATTERN.search(output)
    if match is None:
        raise RuntimeError("GDB did not return a symbol address")
    address = int(match.group(1), 16)
    if address == 0:
        raise RuntimeError("GDB returned a null symbol address")
    return address


def resolve_rtt_address(gdb, axf):
    """Resolve the RTT control-block address from the current AXF symbols."""
    try:
        result = subprocess.run(
            [
                str(gdb),
                "-q",
                "-nx",
                "-batch",
                str(axf),
                "-ex",
                "p/x &_SEGGER_RTT",
            ],
            capture_output=True,
            timeout=15,
            check=False,
        )
    except subprocess.TimeoutExpired as error:
        raise RuntimeError("Timed out resolving _SEGGER_RTT address from AXF") from error
    if result.returncode != 0:
        diagnostic = result.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(
            "Unable to resolve _SEGGER_RTT address from AXF: {}".format(diagnostic)
        )
    return parse_symbol_address(result.stdout)


def validate_persistence_test_image(gdb, axf):
    """Require the AXF to contain the temporary S04 board-test entry point."""
    try:
        result = subprocess.run(
            [
                str(gdb),
                "-q",
                "-nx",
                "-batch",
                str(axf),
                "-ex",
                "p/x &{}".format(PERSISTENCE_TEST_SYMBOL),
            ],
            capture_output=True,
            timeout=15,
            check=False,
        )
    except subprocess.TimeoutExpired as error:
        raise RuntimeError("Timed out validating the S04 persistence AXF") from error

    diagnostic = result.stdout + result.stderr
    if result.returncode != 0:
        details = diagnostic.decode("utf-8", errors="replace").strip()
        raise RuntimeError(
            "AXF is not an S04 persistence test image: {}".format(details)
        )
    try:
        parse_symbol_address(result.stdout)
    except RuntimeError as error:
        raise RuntimeError(
            "AXF is not an S04 persistence test image: missing {}".format(
                PERSISTENCE_TEST_SYMBOL
            )
        ) from error


def build_rtt_logger_arguments(
    logger,
    device,
    interface,
    speed,
    channel,
    rtt_address,
    output,
):
    """Build one RTT Logger command using a fixed control-block address."""
    return [
        str(logger),
        "-Device", device,
        "-If", interface,
        "-Speed", str(speed),
        "-RTTAddress", "0x{:08X}".format(rtt_address),
        "-RTTChannel", str(channel),
        str(output),
    ]


def classify_rtt_logger_diagnostic(diagnostic):
    """Classify the reason why one RTT Logger attempt ended."""
    if isinstance(diagnostic, str):
        diagnostic = diagnostic.encode("utf-8", errors="replace")
    if b"Could not connect to target" in diagnostic:
        return "TARGET_UNAVAILABLE"
    if b"RTT Control Block not found" in diagnostic:
        return "RTT_CONTROL_BLOCK_NOT_FOUND"
    if b"Getting RTT data from target" in diagnostic:
        return "CONNECTED"
    return "UNKNOWN"


def calculate_reconnect_delay(attempt_number):
    """Return a bounded exponential delay for the next Logger attempt."""
    if attempt_number < 1:
        raise ValueError("Reconnect attempt number must be positive")
    delay = RTT_RECONNECT_INITIAL_DELAY_SECONDS * (2 ** (attempt_number - 1))
    return min(delay, RTT_RECONNECT_MAX_DELAY_SECONDS)


def is_rtt_logger_stalled(last_data_time, current_time, timeout_seconds):
    """Return whether a live Logger has stopped producing output."""
    if timeout_seconds <= 0:
        raise ValueError("RTT stall timeout must be positive")
    return (current_time - last_data_time) >= timeout_seconds


def stop_process(process):
    """Stop a process created by this test runner."""
    if process is None or process.poll() is not None:
        return

    try:
        process.terminate()
    except ProcessLookupError:
        return
    try:
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        try:
            process.kill()
        except ProcessLookupError:
            return
        process.wait()


def acquire_jlink_lock(lock_path):
    """Prevent two local test runners from claiming the J-Link simultaneously."""
    try:
        descriptor = os.open(
            str(lock_path),
            os.O_CREAT | os.O_EXCL | os.O_WRONLY,
        )
    except FileExistsError as error:
        raise RuntimeError(
            "J-Link test lock already exists: {}".format(lock_path)
        ) from error

    try:
        with os.fdopen(descriptor, "w", encoding="ascii") as lock_file:
            lock_file.write("pid={}\n".format(os.getpid()))
    except BaseException:
        remove_file(lock_path)
        raise
    return Path(lock_path)


def release_jlink_lock(lock_path):
    """Release the local J-Link test lock when the runner exits."""
    remove_file(lock_path)


def wait_for_tcp_port(port, timeout_seconds):
    """Wait until the local GDB Server accepts connections."""
    deadline = time.monotonic() + timeout_seconds
    while time.monotonic() < deadline:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as client:
            client.settimeout(0.25)
            try:
                client.connect(("127.0.0.1", port))
                return
            except OSError:
                time.sleep(0.1)
    raise TimeoutError("GDB Server startup timeout on port {}".format(port))


def create_runtime_gdb_script(axf, source_script):
    """Prepend the AXF symbol file command without adding a GDB load command."""
    descriptor, temporary_path = tempfile.mkstemp(prefix="s04_persistence_", suffix=".gdb")
    os.close(descriptor)
    axf_command = 'file "{}"'.format(str(axf).replace("\\", "/"))
    content = axf_command + "\n" + source_script.read_text(encoding="utf-8")
    Path(temporary_path).write_text(content, encoding="utf-8")
    return Path(temporary_path)


def invoke_parser(parser, output_log, require_post_event=False):
    """Run the shared Host parser and return its PASS/FAIL/NOT_READY status code."""
    parser_arguments = [sys.executable, str(parser), "--log", str(output_log)]
    if require_post_event:
        parser_arguments.append("--require-post-event")
    result = subprocess.run(
        parser_arguments,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    if result.stdout:
        print(result.stdout, end="")
    if result.stderr:
        print(result.stderr, end="", file=sys.stderr)
    return result.returncode


def run_gdb_reset_session(options, paths, output_log, label):
    """Run one GDB reset session and release the Probe with continue& -> disconnect."""
    runtime_script = create_runtime_gdb_script(paths["axf"], paths["gdb_script"])
    server_log = paths["server_log"].open("ab")
    server = None
    try:
        server_arguments = [
            str(paths["server"]),
            "-device", options.device,
            "-if", options.interface,
            "-speed", str(options.speed),
            "-port", str(options.port),
            "-swoport", "2332",
            "-telnetport", "2333",
            "-nogui",
        ]
        print("[S04-PERSIST] Starting GDB Server for {} session.".format(label))
        server = subprocess.Popen(server_arguments, stdout=server_log, stderr=subprocess.STDOUT)
        wait_for_tcp_port(options.port, options.start_timeout)

        with output_log.open("ab") as gdb_output:
            gdb_output.write(
                "\n[S04-PERSIST] GDB session: {}\n".format(label).encode("utf-8")
            )
            print("[S04-PERSIST] GDB connected; reset -> continue& -> disconnect.")
            try:
                gdb_result = subprocess.run(
                    [str(paths["gdb"]), "-q", "-nx", "-x", str(runtime_script)],
                    stdout=gdb_output,
                    stderr=subprocess.STDOUT,
                    timeout=60,
                    check=False,
                )
            except subprocess.TimeoutExpired as error:
                raise TimeoutError("S04 {} GDB session timeout".format(label)) from error

        if gdb_result.returncode != 0:
            raise RuntimeError(
                "S04 {} GDB returned ERRORLEVEL={}".format(label, gdb_result.returncode)
            )
    finally:
        stop_process(server)
        server_log.close()
        remove_file(runtime_script)


def capture_rtt_window(options, paths, output_log, label, duration_seconds, rtt_address):
    """Capture one RTT window after a GDB session has released the Probe."""
    chunk = paths["log_directory"] / "S04_reset_{}_rtt.log".format(label)
    diagnostic = paths["log_directory"] / "S04_reset_{}_rtt_logger.log".format(label)
    remove_file(chunk)
    remove_file(diagnostic)
    diagnostic_file = diagnostic.open("wb")
    logger = None
    try:
        logger_arguments = build_rtt_logger_arguments(
            paths["rtt_logger"],
            options.device,
            options.interface,
            options.speed,
            options.channel,
            rtt_address,
            chunk,
        )
        print("[S04-PERSIST] RTT capture {} window started.".format(label))
        logger = subprocess.Popen(
            logger_arguments,
            stdout=diagnostic_file,
            stderr=subprocess.STDOUT,
        )
        time.sleep(duration_seconds)
    finally:
        stop_process(logger)
        diagnostic_file.close()

    content = chunk.read_bytes() if chunk.is_file() else b""
    if SNAPSHOT_MARKER not in content:
        raise RuntimeError("S04 {} RTT window contains no snapshot".format(label))
    with output_log.open("ab") as merged:
        merged.write("\n[S04-PERSIST] RTT window: {}\n".format(label).encode("utf-8"))
        merged.write(content)
        if not content.endswith(b"\n"):
            merged.write(b"\n")


def run_reset_test(options, paths):
    """Run startup, baseline RTT, reset, and post-reset persistence validation."""
    if options.port < 1 or options.port > 65535:
        raise ValueError("GDB port is outside the valid range: {}".format(options.port))
    if options.speed < 1:
        raise ValueError("J-Link speed must be positive: {}".format(options.speed))
    if options.start_timeout < 1:
        raise ValueError("GDB Server startup timeout must be positive")

    script_content = paths["gdb_script"].read_text(encoding="utf-8")
    if "continue&" not in script_content:
        raise ValueError("S04 reset GDB script must contain continue&")
    if "disconnect" not in script_content:
        raise ValueError("S04 reset GDB script must contain disconnect")
    if "\nload" in script_content or script_content.lstrip().startswith("load"):
        raise ValueError("S04 reset GDB script must not contain load")

    validate_persistence_test_image(paths["gdb"], paths["axf"])
    rtt_address = resolve_rtt_address(paths["gdb"], paths["axf"])
    remove_file(paths["server_log"])
    remove_file(paths["output_log"])
    run_gdb_reset_session(options, paths, paths["output_log"], "startup")
    capture_rtt_window(
        options,
        paths,
        paths["output_log"],
        "baseline",
        RESET_RTT_WINDOW_SECONDS,
        rtt_address,
    )
    run_gdb_reset_session(options, paths, paths["output_log"], "reset")
    capture_rtt_window(
        options,
        paths,
        paths["output_log"],
        "post_reset",
        RESET_RTT_WINDOW_SECONDS,
        rtt_address,
    )

    output = paths["output_log"].read_bytes()
    if output.count(SNAPSHOT_MARKER) < 2:
        raise RuntimeError("S04 reset capture contains fewer than two snapshots")
    if output.count(b"continue-and-disconnect complete") < 2:
        raise RuntimeError("S04 reset capture contains incomplete disconnect evidence")

    return invoke_parser(paths["parser"], paths["output_log"])


def start_rtt_logger(options, paths, chunk_number, rtt_address):
    """Start one RTT Logger chunk and keep its diagnostic output separate."""
    chunk = paths["log_directory"] / (
        "S04_power_cycle_persistence_chunk_{:03d}.log".format(chunk_number)
    )
    diagnostic = paths["log_directory"] / (
        "S04_power_cycle_persistence_logger_{:03d}.log".format(chunk_number)
    )
    remove_file(chunk)
    remove_file(diagnostic)
    diagnostic_file = diagnostic.open("wb")
    logger_arguments = build_rtt_logger_arguments(
        paths["rtt_logger"],
        options.device,
        options.interface,
        options.speed,
        options.channel,
        rtt_address,
        chunk,
    )
    try:
        process = subprocess.Popen(
            logger_arguments,
            stdout=diagnostic_file,
            stderr=subprocess.STDOUT,
        )
    except BaseException:
        diagnostic_file.close()
        raise
    print("[S04-PERSIST] RTT listener chunk {} started.".format(chunk_number))
    return process, diagnostic_file, chunk, diagnostic


def stop_rtt_logger(logger_state):
    """Stop one logger process and close its diagnostic file."""
    if logger_state is None:
        return
    process, diagnostic_file, _, _ = logger_state
    try:
        stop_process(process)
    finally:
        diagnostic_file.close()


def merge_chunks(chunk_files, output_log, event_chunk=None, event_offset=None):
    """Merge RTT chunks in order into the parser input log."""
    with output_log.open("wb") as merged:
        for chunk in chunk_files:
            if not chunk.is_file():
                continue
            content = chunk.read_bytes()
            if content:
                if chunk == event_chunk and event_offset is not None:
                    offset = min(event_offset, len(content))
                    merged.write(content[:offset])
                    merged.write(
                        b"\n" + POWER_CYCLE_EVENT_MARKER + b"\n"
                    )
                    merged.write(content[offset:])
                else:
                    merged.write(content)
                if not content.endswith(b"\n"):
                    merged.write(b"\n")


def run_power_cycle_test(options, paths):
    """Capture RTT before and after an operator power cycle, reconnecting on loss."""
    if options.speed < 1:
        raise ValueError("J-Link speed must be positive: {}".format(options.speed))
    if options.channel < 0 or options.channel > 15:
        raise ValueError("RTT channel is outside the valid range: {}".format(options.channel))
    if options.capture_seconds < 5:
        raise ValueError("Power-cycle capture duration must be at least five seconds")

    validate_persistence_test_image(paths["gdb"], paths["axf"])
    rtt_address = resolve_rtt_address(paths["gdb"], paths["axf"])

    remove_file(paths["output_log"])
    for old_chunk in paths["log_directory"].glob("S04_power_cycle_persistence_chunk_*.log"):
        remove_file(old_chunk)
    for old_diagnostic in paths["log_directory"].glob("S04_power_cycle_persistence_logger_*.log"):
        remove_file(old_diagnostic)

    print(
        "[S04-PERSIST] RTT owns the J-Link during this test. "
        "Serial module: {} (informational only).".format(options.serial_port)
    )
    print("[S04-PERSIST] RTT control block: 0x{:08X}.".format(rtt_address))
    print("[S04-PERSIST] Start the listener first; do not start another J-Link client.")

    capture_start = time.monotonic()
    deadline = time.monotonic() + options.capture_seconds
    logger_state = None
    chunk_files = []
    chunk_number = 0
    reconnect_attempt = 0
    ready_reported = False
    boot_detected = False
    post_event_snapshot_detected = False
    event_chunk = None
    event_offset = None
    automation_events = []
    chunk_offsets = {}
    chunk_sizes = {}
    chunk_last_data_times = {}
    try:
        while time.monotonic() < deadline:
            current_time = time.monotonic()
            if logger_state is None:
                chunk_number += 1
                logger_state = start_rtt_logger(options, paths, chunk_number, rtt_address)
                chunk_files.append(logger_state[2])
                chunk_sizes[logger_state[2]] = 0
                chunk_last_data_times[logger_state[2]] = current_time
                chunk_offsets[logger_state[2]] = 0
            elif logger_state[0].poll() is not None:
                diagnostic_path = logger_state[3]
                logger_state[1].flush()
                diagnostic = (
                    diagnostic_path.read_bytes()
                    if diagnostic_path.is_file()
                    else b""
                )
                stop_rtt_logger(logger_state)
                logger_state = None
                reconnect_attempt += 1
                reason = classify_rtt_logger_diagnostic(diagnostic)
                delay = calculate_reconnect_delay(reconnect_attempt)
                elapsed = time.monotonic() - capture_start
                print(
                    "[S04-PERSIST][INFO] RTT Logger exited at t={:.1f}s; "
                    "reason={}; retry={} in {:.1f}s.".format(
                        elapsed,
                        reason,
                        reconnect_attempt,
                        delay,
                    )
                )
                automation_events.append(
                    "[S04-PERSIST][INFO] RTT Logger exited at t={:.1f}s; "
                    "reason={}; retry={} in {:.1f}s.".format(
                        elapsed,
                        reason,
                        reconnect_attempt,
                        delay,
                    )
                )
                remaining = deadline - time.monotonic()
                if remaining > 0:
                    time.sleep(min(delay, remaining))

            elif logger_state[2].is_file():
                chunk = logger_state[2]
                current_size = chunk.stat().st_size
                previous_size = chunk_sizes.get(chunk, 0)
                if current_size > previous_size:
                    chunk_last_data_times[chunk] = current_time
                    chunk_sizes[chunk] = current_size
                    reconnect_attempt = 0
                elif is_rtt_logger_stalled(
                    chunk_last_data_times.get(chunk, current_time),
                    current_time,
                    RTT_STALL_TIMEOUT_SECONDS,
                ):
                    elapsed = current_time - capture_start
                    stop_rtt_logger(logger_state)
                    logger_state = None
                    reconnect_attempt += 1
                    delay = calculate_reconnect_delay(reconnect_attempt)
                    print(
                        "[S04-PERSIST][INFO] RTT Logger stalled at t={:.1f}s; "
                        "reason=RTT_DATA_STALLED; retry={} in {:.1f}s.".format(
                            elapsed,
                            reconnect_attempt,
                            delay,
                        )
                    )
                    automation_events.append(
                        "[S04-PERSIST][INFO] RTT Logger stalled at t={:.1f}s; "
                        "reason=RTT_DATA_STALLED; retry={} in {:.1f}s.".format(
                            elapsed,
                            reconnect_attempt,
                            delay,
                        )
                    )
                    remaining = deadline - time.monotonic()
                    if remaining > 0:
                        time.sleep(min(delay, remaining))

            if logger_state is not None and logger_state[0].poll() is None:
                logger_state[1].flush()
                diagnostic_path = logger_state[3]
                diagnostic = (
                    diagnostic_path.read_bytes()
                    if diagnostic_path.is_file()
                    else b""
                )
                if classify_rtt_logger_diagnostic(diagnostic) == "CONNECTED":
                    reconnect_attempt = 0

            if not ready_reported:
                for chunk in chunk_files:
                    if chunk.is_file() and SNAPSHOT_MARKER in chunk.read_bytes():
                        ready_reported = True
                        for known_chunk in chunk_files:
                            if known_chunk.is_file():
                                chunk_offsets[known_chunk] = known_chunk.stat().st_size
                        print("[S04-PERSIST][READY] Baseline snapshot captured.")
                        print(
                            "[S04-PERSIST][READY] Power-cycle the target now; "
                            "capture remains active."
                        )
                        break
            elif not post_event_snapshot_detected:
                for chunk in chunk_files:
                    if not chunk.is_file():
                        continue
                    content = chunk.read_bytes()
                    previous_size = chunk_offsets.get(chunk, 0)
                    new_content = content[previous_size:]
                    chunk_offsets[chunk] = len(content)
                    if not boot_detected:
                        boot_position = new_content.find(BOOT_MARKER)
                    else:
                        boot_position = -1
                    if boot_position >= 0:
                        boot_detected = True
                        event_chunk = chunk
                        event_offset = (
                            previous_size + boot_position + len(BOOT_MARKER)
                        )
                        print("[S04-PERSIST][EVENT] Test firmware boot marker captured.")
                        event_region = content[event_offset:]
                        if SNAPSHOT_MARKER in event_region:
                            post_event_snapshot_detected = True
                            break
                    elif boot_detected:
                        event_region = new_content
                        if chunk == event_chunk and event_offset is not None:
                            event_region = content[event_offset:]
                        if SNAPSHOT_MARKER in event_region:
                            post_event_snapshot_detected = True
                            break
                    chunk_offsets[chunk] = len(content)
            time.sleep(0.25)
    finally:
        stop_rtt_logger(logger_state)

    merge_chunks(
        chunk_files,
        paths["output_log"],
        event_chunk,
        event_offset,
    )
    if automation_events:
        with paths["output_log"].open("ab") as output_file:
            output_file.write(b"\n[S04-PERSIST] AUTOMATION_EVENTS\n")
            for event in automation_events:
                output_file.write((event + "\n").encode("utf-8"))
    parser_result = invoke_parser(
        paths["parser"],
        paths["output_log"],
        require_post_event=True,
    )
    if parser_result == 0 and (
        not boot_detected or not post_event_snapshot_detected
    ):
        print(
            "[S04-PERSIST][NOT_READY] No post-READY snapshot was captured "
            "after the boot marker."
        )
        return 2
    return parser_result


def main(arguments=None):
    """Run the selected board test and return its process status."""
    options = parse_arguments(arguments)
    paths = {
        "parser": require_file(options.parser, "S04 host parser"),
        "rtt_logger": require_file(options.rtt_logger, "J-Link RTT Logger"),
        "gdb": require_file(options.gdb, "ARM GDB"),
        "axf": require_file(options.axf, "S04 persistence test AXF"),
        "server_log": Path(options.server_log),
        "output_log": Path(options.output_log),
        "log_directory": Path(options.log_directory),
    }
    if options.mode == "reset":
        paths["server"] = require_file(options.server, "J-Link GDB Server")
        paths["gdb_script"] = require_file(options.gdb_script, "S04 GDB script")
    paths["log_directory"].mkdir(parents=True, exist_ok=True)

    lock_path = paths["log_directory"] / JLINK_LOCK_FILE_NAME
    lock_acquired = False
    try:
        acquire_jlink_lock(lock_path)
        lock_acquired = True
        if options.mode == "reset":
            result = run_reset_test(options, paths)
        else:
            result = run_power_cycle_test(options, paths)
        if result == 0:
            print("[S04-PERSIST][PASS] Persistence parser passed.")
        return result
    except (OSError, RuntimeError, TimeoutError, ValueError) as error:
        print("[S04-PERSIST][FAIL] {}".format(error))
        if lock_acquired:
            paths["output_log"].write_text(
                "[S04-PERSIST][FAIL] {}\n".format(error),
                encoding="utf-8",
            )
        return 1
    finally:
        if lock_acquired:
            release_jlink_lock(lock_path)


if __name__ == "__main__":
    sys.exit(main())
