"""Parse and compare S04 persistence snapshots."""

import argparse
import sys

SNAPSHOT_MARKER = "[S04-PERSIST] SNAPSHOT"
POWER_CYCLE_EVENT_MARKER = "[S04-PERSIST] POWER_CYCLE_BOOT_CONFIRMED"
IMAGE_VALID = 2
SLOT_B_VALID = 1
IMAGE_VALIDATION_MAX = 10

_REQUIRED_FIELDS = (
    "image_validation",
    "image_size",
    "image_crc",
    "metadata_a_valid",
    "metadata_b_valid",
    "selected_copy",
    "sequence",
    "active_slot",
    "confirmed_slot",
    "slot_a_state",
    "slot_b_state",
    "confirmed_version",
)


def _parse_integer(value):
    return int(value, 0)


def _parse_uint32(value, field):
    parsed = _parse_integer(value)
    if parsed < 0 or parsed > 0xFFFFFFFF:
        raise ValueError("{} must be an unsigned 32-bit value".format(field))
    return parsed


def parse_snapshot_line(line):
    """Return one snapshot dictionary, or None for a non-snapshot line."""
    if SNAPSHOT_MARKER not in line:
        return None

    fields = {}
    for token in line.split():
        if "=" in token:
            key, value = token.split("=", 1)
            fields[key] = value

    if any(field not in fields for field in _REQUIRED_FIELDS):
        raise ValueError("snapshot is missing a required field")

    version_parts = fields["confirmed_version"].split(".")
    version = tuple(int(part, 10) for part in version_parts)
    if len(version) != 3:
        raise ValueError("confirmed_version must contain major.minor.patch")
    if any(part < 0 or part > 0xFFFF for part in version):
        raise ValueError("confirmed_version parts must be unsigned 16-bit values")

    snapshot = {
        "image_validation": _parse_uint32(fields["image_validation"], "image_validation"),
        "image_size": _parse_uint32(fields["image_size"], "image_size"),
        "image_crc": _parse_uint32(fields["image_crc"], "image_crc"),
        "metadata_a_valid": _parse_uint32(fields["metadata_a_valid"], "metadata_a_valid"),
        "metadata_b_valid": _parse_uint32(fields["metadata_b_valid"], "metadata_b_valid"),
        "selected_copy": _parse_uint32(fields["selected_copy"], "selected_copy"),
        "sequence": _parse_uint32(fields["sequence"], "sequence"),
        "active_slot": _parse_uint32(fields["active_slot"], "active_slot"),
        "confirmed_slot": _parse_uint32(fields["confirmed_slot"], "confirmed_slot"),
        "slot_a_state": _parse_uint32(fields["slot_a_state"], "slot_a_state"),
        "slot_b_state": _parse_uint32(fields["slot_b_state"], "slot_b_state"),
        "confirmed_version": version,
    }
    if snapshot["image_validation"] > IMAGE_VALIDATION_MAX:
        raise ValueError("image_validation is outside the firmware enum range")
    if snapshot["metadata_a_valid"] not in (0, 1):
        raise ValueError("metadata_a_valid must be 0 or 1")
    if snapshot["metadata_b_valid"] not in (0, 1):
        raise ValueError("metadata_b_valid must be 0 or 1")
    if snapshot["selected_copy"] not in (0, 1, 2):
        raise ValueError("selected_copy must be 0, 1, or 2")
    if snapshot["active_slot"] not in (0, 1, 0xFF):
        raise ValueError("active_slot must be 0, 1, or 255")
    if snapshot["confirmed_slot"] not in (0, 1, 0xFF):
        raise ValueError("confirmed_slot must be 0, 1, or 255")
    if snapshot["slot_a_state"] not in (0, 1, 2):
        raise ValueError("slot_a_state must be 0, 1, or 2")
    if snapshot["slot_b_state"] not in (0, 1, 2):
        raise ValueError("slot_b_state must be 0, 1, or 2")
    return snapshot


def compare_snapshots(before, after):
    """Compare all persistence fields and return a machine-readable result."""
    if before is None or after is None:
        return {
            "status": "NOT_READY",
            "reason": "before and after snapshots are both required",
            "before": before,
            "after": after,
        }

    mismatches = [
        field for field in before if before[field] != after[field]
    ]
    if mismatches:
        return {
            "status": "FAIL",
            "reason": "changed fields: " + ", ".join(mismatches),
            "before": before,
            "after": after,
        }

    if before["image_validation"] != IMAGE_VALID:
        return {
            "status": "FAIL",
            "reason": "baseline image is not VALID",
            "before": before,
            "after": after,
        }

    if before["slot_b_state"] != SLOT_B_VALID:
        return {
            "status": "FAIL",
            "reason": "baseline Slot B state is not VALID",
            "before": before,
            "after": after,
        }

    if before["selected_copy"] == 0:
        return {
            "status": "FAIL",
            "reason": "no committed Metadata copy was selected",
            "before": before,
            "after": after,
        }

    return {
        "status": "PASS",
        "reason": "all persistent fields are unchanged",
        "before": before,
        "after": after,
    }


def evaluate_snapshots(lines, require_post_event=False):
    """Compare snapshots, optionally requiring one after the power-cycle event."""
    snapshots = []
    lines = list(lines)
    for index, line in enumerate(lines):
        if SNAPSHOT_MARKER not in line:
            continue
        try:
            snapshot = parse_snapshot_line(line)
        except ValueError as error:
            return {
                "status": "FAIL",
                "reason": "invalid snapshot: {}".format(error),
                "before": snapshots[0][1] if snapshots else None,
                "after": None,
            }
        if snapshot is not None:
            snapshots.append((index, snapshot))

    if len(snapshots) < 2:
        return {
            "status": "NOT_READY",
            "reason": "at least two snapshots are required",
            "before": snapshots[0][1] if snapshots else None,
            "after": None,
        }

    if not require_post_event:
        return compare_snapshots(snapshots[0][1], snapshots[-1][1])

    event_indices = [
        index for index, line in enumerate(lines)
        if POWER_CYCLE_EVENT_MARKER in line
    ]
    if not event_indices:
        return {
            "status": "NOT_READY",
            "reason": "power-cycle event marker is required",
            "before": snapshots[0][1],
            "after": None,
        }

    event_index = event_indices[-1]
    before_candidates = [entry for entry in snapshots if entry[0] < event_index]
    after_candidates = [entry for entry in snapshots if entry[0] > event_index]
    if not before_candidates or not after_candidates:
        return {
            "status": "NOT_READY",
            "reason": "a post-event snapshot is required",
            "before": before_candidates[0][1] if before_candidates else None,
            "after": None,
        }

    return compare_snapshots(before_candidates[0][1], after_candidates[0][1])


def main(arguments=None):
    """Evaluate a captured S04 persistence log and return a process status."""
    parser = argparse.ArgumentParser(description="Evaluate S04 persistence snapshots")
    parser.add_argument("--log", required=True, help="UTF-8 log file to evaluate")
    parser.add_argument(
        "--require-post-event",
        action="store_true",
        help="require a snapshot after the controller power-cycle event marker",
    )
    options = parser.parse_args(arguments)

    try:
        with open(options.log, "r", encoding="utf-8", errors="replace") as log_file:
            result = evaluate_snapshots(
                log_file,
                require_post_event=options.require_post_event,
            )
    except (OSError, UnicodeError) as error:
        print("[S04-PERSIST][FAIL] cannot read log: {}".format(error))
        return 1

    print("[S04-PERSIST][{}] {}".format(result["status"], result["reason"]))
    if result["status"] == "PASS":
        return 0
    if result["status"] == "NOT_READY":
        return 2
    return 1


if __name__ == "__main__":
    sys.exit(main())
