@echo off
:: -----------------------------------------------------------
:: MeshX Build Script (Windows Batch)
:: Usage: build.bat <target> [product] [clean]
::   <target>    - Target BSP/MCU (e.g., weact_c3, xiao_c3)
::   <product>   - Product name (default: 4_relay_panel)
::   [clean]     - Optional: clean build directory and CMake cache
:: Usage examples:
::   build.bat weact_c3
:: build.bat weact_c3 4_relay_panel
:: build.bat xiao_c3 clean
:: -----------------------------------------------------------

:: Validate target argument
if "%~1" equ "" (
    echo ERROR: No target specified.
    echo.
    echo Usage: build.bat <target> [product] [clean]
    echo.
    echo Example: build.bat weact_c3
    echo Example: build.bat weact_c3 4_relay_panel
    echo Example: build.bat xiao_c3 clean
    exit /b 1
)

set "TARGET=%~1"
echo Target: %TARGET%

:: Set product name (default: 4_relay_panel for weact_c3)
if "%~2" equ "" (
    set "PRODUCT=4_relay_panel"
) else (
    set "PRODUCT=%~2"
)
echo Product: %PRODUCT%

:: Call environment setup (sets PROJECT_ROOT, PATH, installs PyYAML, IDF_PATH)
call "%~dp0env.bat"

:: Run code generation with product profile
echo.
echo Running code generation...
python3 tools/scripts/code_gen.py %PRODUCT% --root components/meshx --profile port/bsp\%TARGET%\prod_profile.yml

:: Build parameters
set "BUILD_PARAM="

:: Check if "clean" third argument was provided
if "%~3" equ "clean" (
    echo.
    echo Cleaning build directory and CMake cache...
    rmdir /s /q build
    :: Remove old CMake cache to prevent path mismatch
    if exist .git\cmake_install.cmake del .git\cmake_install.cmake 2>nul
    set "BUILD_PARAM=-DIDF_TARGET=esp32c3 clean"
)

echo.
echo Running idf.py build with: idf.py -DTARGET=%TARGET% %BUILD_PARAM% build%
idf.py -DTARGET=%TARGET% %BUILD_PARAM% build

echo.
echo Build complete.
pause