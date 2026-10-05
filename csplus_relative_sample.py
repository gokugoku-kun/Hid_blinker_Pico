# -*- coding: utf-8 -*-
"""CS+ IronPython 2.7 console sample using a relative import path."""

import os
import sys


# CS+の現在の作業フォルダーを基準にした相対パスです。
# CS+の作業フォルダーがプロジェクトの親フォルダーの場合の例です。
module_dir = os.path.abspath(r".\Hid_blinker_Pico")
if module_dir not in sys.path:
    sys.path.append(module_dir)

import csplus_pico


# python.exeがPATHに登録されている場合は、この指定で起動できます。
csplus_pico.configure(python_exe="python")

print("module: {0}".format(csplus_pico.__file__))
print("LED 0 ON: {0}".format(csplus_pico.on(0)))
print("LED 1 BLINK: {0}".format(csplus_pico.blink(1, 300, 300)))
print("ALL OFF: {0}".format(csplus_pico.all_off()))
