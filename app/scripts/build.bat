@echo off
set target=%1

echo TARGET = %target%
python scripts/code_gen.py %target%
set APP_PATH=%CD%
idf.py -DTARGET="%target%" set-target esp32c3 clean build
