"""PC-side HID controller for the Pico W LED firmware.

Requires: python -m pip install hidapi
"""

from __future__ import annotations

import argparse
import struct
import sys
import time
from dataclasses import dataclass
from typing import Iterable, Optional

import hid

VID = 0x1209
PID = 0x0001
PRODUCT = "Hid_blinker_Pico"
USAGE_PAGE = 0xFF00
USAGE = 0x0001
REPORT_SIZE = 64
READ_TIMEOUT_MS = 1000

CMD_SET_LED = 0x01
CMD_GET_STATUS = 0x02
CMD_ALL_OFF = 0x03
CMD_SET_ALL = 0x04

MODE_OFF = 0
MODE_ON = 1
MODE_BLINK = 2

STATUS_NAMES = {
    0: "SUCCESS",
    1: "BAD_LENGTH",
    2: "BAD_VERSION",
    3: "BAD_COMMAND",
    4: "BAD_LED",
    5: "BAD_MODE",
    6: "BAD_TIMING",
    7: "BAD_RESERVED",
}


@dataclass(frozen=True)
class DeviceInfo:
    path: object
    serial_number: str
    product_string: str
    usage_page: int
    usage: int
    interface_number: int


def _devices() -> list[dict]:
    return [
        item
        for item in hid.enumerate(VID, PID)
        if item.get("product_string") == PRODUCT
        and item.get("usage_page") == USAGE_PAGE
        and item.get("usage") == USAGE
    ]


def list_devices() -> list[DeviceInfo]:
    return [
        DeviceInfo(
            path=item["path"],
            serial_number=item.get("serial_number") or "",
            product_string=item.get("product_string") or "",
            usage_page=item.get("usage_page") or 0,
            usage=item.get("usage") or 0,
            interface_number=(
                item["interface_number"]
                if item.get("interface_number") is not None
                else -1
            ),
        )
        for item in _devices()
    ]


def _select_device(serial: Optional[str]) -> DeviceInfo:
    devices = list_devices()
    if serial is not None:
        devices = [item for item in devices if item.serial_number == serial]
    if not devices:
        raise RuntimeError(
            "Pico HID device not found. Check the UF2, USB cable, and "
            f"VID/PID {VID:04x}:{PID:04x}."
        )
    if len(devices) > 1:
        choices = ", ".join(item.serial_number or "<no serial>" for item in devices)
        raise RuntimeError(f"Multiple Pico devices found; use --serial: {choices}")
    return devices[0]


class PicoLed:
    def __init__(self, serial: Optional[str] = None, timeout_ms: int = READ_TIMEOUT_MS):
        self.device_info = _select_device(serial)
        self.timeout_ms = timeout_ms
        self._device = hid.device()
        self._device.open_path(self.device_info.path)
        self._sequence = 0

    def close(self) -> None:
        self._device.close()

    def __enter__(self) -> "PicoLed":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()

    def _next_sequence(self) -> int:
        self._sequence = (self._sequence + 1) & 0xFFFF
        return self._sequence

    def _request(
        self,
        command: int,
        led_id: int,
        mode: int = 0,
        on_ms: int = 0,
        off_ms: int = 0,
    ) -> bytes:
        sequence = self._next_sequence()
        report = bytearray(REPORT_SIZE)
        report[0] = 1
        report[1] = command
        struct.pack_into("<H", report, 2, sequence)
        report[4] = led_id
        report[5] = mode
        struct.pack_into("<II", report, 8, on_ms, off_ms)

        # HIDAPI requires a leading zero Report ID for a report with no ID.
        written = self._device.write(bytes([0]) + report)
        if written < REPORT_SIZE + 1:
            raise RuntimeError(f"short HID write: {written} bytes")

        response = self._device.read(REPORT_SIZE, self.timeout_ms)
        if len(response) != REPORT_SIZE:
            raise TimeoutError(
                f"HID response timeout/short read: received {len(response)} bytes"
            )
        data = bytes(response)
        response_sequence = struct.unpack_from("<H", data, 2)[0]
        if data[0] != 1 or data[1] != (command | 0x80) or response_sequence != sequence:
            raise RuntimeError(
                f"unexpected HID response: version={data[0]} command=0x{data[1]:02x} "
                f"sequence={response_sequence}"
            )
        status = data[4]
        if status != 0:
            raise RuntimeError(
                f"Pico rejected command: {STATUS_NAMES.get(status, f'0x{status:02x}')}"
            )
        return data

    def set_led(self, led_id: int, mode: int, on_ms: int = 0, off_ms: int = 0) -> bytes:
        if not 0 <= led_id <= 4:
            raise ValueError("led_id must be in the range 0..4")
        if mode not in (MODE_OFF, MODE_ON, MODE_BLINK):
            raise ValueError("mode must be MODE_OFF, MODE_ON, or MODE_BLINK")
        if mode == MODE_BLINK and not (10 <= on_ms <= 3_600_000 and 10 <= off_ms <= 3_600_000):
            raise ValueError("blink durations must be 10..3,600,000 ms")
        return self._request(CMD_SET_LED, led_id, mode, on_ms, off_ms)

    def on(self, led_id: int) -> bytes:
        return self.set_led(led_id, MODE_ON)

    def off(self, led_id: int) -> bytes:
        return self.set_led(led_id, MODE_OFF)

    def blink(self, led_id: int, on_ms: int = 500, off_ms: int = 500) -> bytes:
        return self.set_led(led_id, MODE_BLINK, on_ms, off_ms)

    def all_off(self) -> bytes:
        return self._request(CMD_ALL_OFF, 0xFF)

    def set_all(self, on: bool) -> bytes:
        return self._request(CMD_SET_ALL, 0xFF, MODE_ON if on else MODE_OFF)

    def status(self, led_id: int) -> dict:
        data = self._request(CMD_GET_STATUS, led_id)
        return {
            "led_id": data[5],
            "mode": data[6],
            "output_on": bool(data[7]),
            "on_ms": struct.unpack_from("<I", data, 8)[0],
            "off_ms": struct.unpack_from("<I", data, 12)[0],
            "uptime_ms": struct.unpack_from("<I", data, 16)[0],
            "led_count": data[20],
            "firmware": f"{data[21]}.{data[22]}.{data[23]}",
            "reset_reason": data[24],
        }


def _print_devices() -> int:
    devices = list_devices()
    if not devices:
        print(f"No matching HID device ({VID:04x}:{PID:04x}, {PRODUCT})")
        return 1
    for index, device in enumerate(devices):
        print(f"[{index}] serial={device.serial_number or '<none>'} interface={device.interface_number}")
    return 0


def main(argv: Optional[Iterable[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Control Pico W LEDs over USB HID")
    parser.add_argument("--serial", help="PICOLED-... serial number")
    parser.add_argument("--list", action="store_true", help="list matching Pico devices")
    sub = parser.add_subparsers(dest="command")
    sub.add_parser("on").add_argument("led", type=int)
    sub.add_parser("off").add_argument("led", type=int)
    blink = sub.add_parser("blink")
    blink.add_argument("led", type=int)
    blink.add_argument("on_ms", type=int, nargs="?", default=500)
    blink.add_argument("off_ms", type=int, nargs="?", default=500)
    sub.add_parser("all-off")
    status = sub.add_parser("status")
    status.add_argument("led", type=int)
    args = parser.parse_args(argv)

    if args.list:
        return _print_devices()
    if args.command is None:
        parser.error("choose --list or a command")

    try:
        with PicoLed(args.serial) as device:
            if args.command == "on":
                result = device.on(args.led)
            elif args.command == "off":
                result = device.off(args.led)
            elif args.command == "blink":
                result = device.blink(args.led, args.on_ms, args.off_ms)
            elif args.command == "all-off":
                result = device.all_off()
            else:
                print(device.status(args.led))
                return 0
            print(f"OK: mode={result[6]} output_on={bool(result[7])}")
    except (OSError, RuntimeError, TimeoutError, ValueError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
