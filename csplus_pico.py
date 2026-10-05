# -*- coding: utf-8 -*-
"""IronPython 2.7 bridge for CS+ Python Console.

This file intentionally does not import hidapi. CS+ calls the normal CPython
3 interpreter as a child process, and pico_led.py owns the HID connection.
Edit PYTHON_EXE for the CPython installation on the CS+ PC.
"""

import os
import subprocess
import sys


PYTHON_EXE = r"C:\Program Files\Python39\python.exe"
PICO_LED_SCRIPT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "pico_led.py")
SERIAL = None
# Windows: start CPython without opening a console window.
CREATE_NO_WINDOW = 0x08000000


def configure(python_exe=None, pico_led_script=None, serial=None):
    """Set paths/options used by subsequent calls from CS+.

    Example from the CS+ console:
        import csplus_pico
        csplus_pico.configure(r"C:\Program Files\Python39\python.exe")
    """
    global PYTHON_EXE, PICO_LED_SCRIPT, SERIAL
    if python_exe is not None:
        PYTHON_EXE = python_exe
    if pico_led_script is not None:
        PICO_LED_SCRIPT = pico_led_script
    if serial is not None:
        SERIAL = serial


def _run(command, *arguments):
    args = [PYTHON_EXE, PICO_LED_SCRIPT]
    if SERIAL:
        args.extend(["--serial", SERIAL])
    args.append(command)
    args.extend([str(argument) for argument in arguments])

    try:
        process = subprocess.Popen(
            args,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            universal_newlines=True,
            shell=False,
            creationflags=CREATE_NO_WINDOW,
        )
        output = process.communicate()[0]
    except Exception as exc:
        print("[Pico] CPython起動失敗: {0}".format(exc))
        return False, ""

    if output:
        print(output.rstrip())
    if process.returncode != 0:
        print("[Pico] 操作失敗: returncode={0}".format(process.returncode))
        return False, output
    return True, output


def on(led_id):
    return _run("on", led_id)[0]


def off(led_id):
    return _run("off", led_id)[0]


def blink(led_id, on_ms=500, off_ms=500):
    return _run("blink", led_id, on_ms, off_ms)[0]


def all_off():
    return _run("all-off")[0]


def status(led_id):
    return _run("status", led_id)[0]


def list_devices():
    return _run("--list")[0]
