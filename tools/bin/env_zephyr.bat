@echo off
:: ============================================================================
:: MeshX Zephyr Environment Setup & Automation Script (Windows Batch)
::
:: Strictly anchors all dependencies inside the workspace (third_party/ and tools/):
::   - ZEPHYR_BASE:           %PROJECT_ROOT%\third_party\zephyr
::   - ZEPHYR_SDK_INSTALL_DIR: %PROJECT_ROOT%\third_party\zephyr-sdk (or tools\zephyr-sdk)
::   - West Workspace:        %PROJECT_ROOT%\.west
::   - Hardware Modules:      %PROJECT_ROOT%\modules
::   - Local Tools:           %PROJECT_ROOT%\tools\bin
::
:: Usage:
::   call tools\bin\env_zephyr.bat [optional_sdk_source_path]
:: ============================================================================

for %%I in ("%~dp0..\..") do set "PROJECT_ROOT=%%~fI"
set "SCRIPTS_DIR=%PROJECT_ROOT%\tools\scripts"
set "TOOLS_BIN_DIR=%PROJECT_ROOT%\tools\bin"
set "THIRD_PARTY_DIR=%PROJECT_ROOT%\third_party"
set "TOOLS_DIR=%PROJECT_ROOT%\tools"

echo ============================================================
echo MeshX Zephyr Environment Setup
echo Project Root: %PROJECT_ROOT%
echo ============================================================

:: ----------------------------------------------------------------------------
:: 1. Python Discovery & PATH Configuration
:: ----------------------------------------------------------------------------
python --version >nul 2>&1
if %errorlevel% neq 0 (
    python3 --version >nul 2>&1
    if %errorlevel% neq 0 (
        :: Check standard Windows Python installation locations
        if exist "%LOCALAPPDATA%\Microsoft\WindowsApps\python.exe" (
            set "PATH=%LOCALAPPDATA%\Microsoft\WindowsApps;%PATH%"
        ) else if exist "%ProgramFiles%\Python313\python.exe" (
            set "PATH=%ProgramFiles%\Python313;%ProgramFiles%\Python313\Scripts;%PATH%"
        ) else if exist "%ProgramFiles%\Python312\python.exe" (
            set "PATH=%ProgramFiles%\Python312;%ProgramFiles%\Python312\Scripts;%PATH%"
        ) else if exist "%ProgramFiles%\Python311\python.exe" (
            set "PATH=%ProgramFiles%\Python311;%ProgramFiles%\Python311\Scripts;%PATH%"
        ) else if exist "%ProgramFiles%\Python310\python.exe" (
            set "PATH=%ProgramFiles%\Python310;%ProgramFiles%\Python310\Scripts;%PATH%"
        ) else (
            for /d %%P in ("%LOCALAPPDATA%\Programs\Python\Python*") do (
                if exist "%%P\python.exe" (
                    set "PATH=%%P;%%P\Scripts;%PATH%"
                )
            )
        )
    )
)

:: Re-verify Python
python --version >nul 2>&1
if %errorlevel% neq 0 (
    python3 --version >nul 2>&1
    if %errorlevel% neq 0 (
        echo [ERROR] Python could not be found. Please install Python 3.10+ and add it to PATH.
        exit /b 1
    )
)

:: Add tools\scripts and tools\bin (wget shim, portable tools) to PATH
set "PATH=%SCRIPTS_DIR%;%TOOLS_BIN_DIR%;%PATH%"

:: Add Python user Scripts (where pip installs tools like west, kconfiglib) to PATH
for /f "delims=" %%I in ('python -c "import site, os, sys; v='Python' + str(sys.version_info.major) + str(sys.version_info.minor); p1=os.path.join(site.getuserbase(), v, 'Scripts'); p2=os.path.join(site.getuserbase(), 'Scripts'); print(p1 if os.path.exists(p1) else (p2 if os.path.exists(p2) else ''))" 2^>nul') do (
    if "%%I" neq "" (
        set "PATH=%%I;%PATH%"
    )
)

:: Ensure .PY is in PATHEXT for Python script tools (e.g. esptool.py)
echo %PATHEXT% | findstr /I "\.PY" >nul 2>&1
if %errorlevel% neq 0 (
    set "PATHEXT=%PATHEXT%;.PY;.py"
)

:: ----------------------------------------------------------------------------
:: 2. Auto-detect CMake and Ninja
:: ----------------------------------------------------------------------------
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    if exist "%ProgramFiles%\CMake\bin\cmake.exe" (
        set "PATH=%ProgramFiles%\CMake\bin;%PATH%"
    ) else if exist "%ProgramFiles(x86)%\CMake\bin\cmake.exe" (
        set "PATH=%ProgramFiles(x86)%\CMake\bin;%PATH%"
    ) else if exist "%LOCALAPPDATA%\Microsoft\WinGet\Links\cmake.exe" (
        set "PATH=%LOCALAPPDATA%\Microsoft\WinGet\Links;%PATH%"
    )
)

where ninja >nul 2>&1
if %errorlevel% neq 0 (
    for /f "delims=" %%I in ('python -c "import ninja; print(ninja.BIN_DIR)" 2^>nul') do (
        if exist "%%I\ninja.exe" set "PATH=%%I;%PATH%"
    )
)

where ninja >nul 2>&1
if %errorlevel% neq 0 (
    if exist "%LOCALAPPDATA%\Microsoft\WinGet\Links\ninja.exe" (
        set "PATH=%LOCALAPPDATA%\Microsoft\WinGet\Links;%PATH%"
    ) else if exist "%ProgramFiles%\Ninja\ninja.exe" (
        set "PATH=%ProgramFiles%\Ninja;%PATH%"
    ) else (
        for /d %%N in ("%LOCALAPPDATA%\Microsoft\WinGet\Packages\Ninja-build.Ninja*") do (
            if exist "%%N\ninja.exe" set "PATH=%%N;%PATH%"
        )
    )
)

:: ----------------------------------------------------------------------------
:: 3. Auto-Install Required Python Packages (west, kconfiglib, pyelftools)
:: ----------------------------------------------------------------------------
where west >nul 2>&1
if %errorlevel% neq 0 (
    echo [INFO] Installing 'west' meta-tool via pip...
    python -m pip install west
)

python -c "import kconfiglib" >nul 2>&1
if %errorlevel% neq 0 (
    echo [INFO] Installing Kconfiglib ^& Zephyr dependencies...
    python -m pip install kconfiglib pyelftools
)

:: ----------------------------------------------------------------------------
:: 4. Locate Zephyr RTOS (External System Path or Workspace Fallback)
:: ----------------------------------------------------------------------------
if defined ZEPHYR_BASE (
    :: Strip surrounding quotes if present
    set "ZEPHYR_BASE=%ZEPHYR_BASE:"=%"
    if exist "%ZEPHYR_BASE%\VERSION" (
        echo [INFO] Using external ZEPHYR_BASE: %ZEPHYR_BASE%
    ) else (
        echo [WARN] ZEPHYR_BASE is set to '%ZEPHYR_BASE%', but VERSION was not found.
        set "ZEPHYR_BASE="
    )
)

if not defined ZEPHYR_BASE (
    if exist "D:\Zephyr_Project\zephyr\VERSION" (
        set "ZEPHYR_BASE=D:\Zephyr_Project\zephyr"
    ) else if exist "%THIRD_PARTY_DIR%\zephyr\VERSION" (
        set "ZEPHYR_BASE=%THIRD_PARTY_DIR%\zephyr"
    ) else if exist "%PROJECT_ROOT%\..\zephyr\VERSION" (
        for %%I in ("%PROJECT_ROOT%\..\zephyr") do set "ZEPHYR_BASE=%%~fI"
    )
)

if not defined ZEPHYR_BASE goto :no_zephyr_base
goto :zephyr_base_ok

:no_zephyr_base
echo.
echo ============================================================================
echo [ERROR] ZEPHYR_BASE is not defined!
echo ============================================================================
echo Zephyr RTOS was not detected in environment or workspace.
echo.
echo How to install Zephyr:
echo   1. Clone Zephyr RTOS to your preferred location [e.g. D:\Zephyr_Project\zephyr]:
echo        git clone --depth 1 --branch v3.7-branch https://github.com/zephyrproject-rtos/zephyr.git D:\Zephyr_Project\zephyr
echo.
echo   2. Set the environment variable in your terminal or Windows System Variables:
echo        set ZEPHYR_BASE=D:\Zephyr_Project\zephyr
echo        setx ZEPHYR_BASE "D:\Zephyr_Project\zephyr"
echo.
echo   3. Re-run this script:
echo        tools\bin\env_zephyr.bat
echo ============================================================================
echo.
exit /b 1

:zephyr_base_ok

:: Ensure root .west workspace configuration points to current ZEPHYR_BASE
if not exist "%PROJECT_ROOT%\.west" mkdir "%PROJECT_ROOT%\.west"
python -c "import os, sys; z=os.environ.get('ZEPHYR_BASE',''); r=os.environ.get('PROJECT_ROOT',''); rel=os.path.relpath(z, r).replace('\\\\','/') if z and r else z; open(os.path.join(r, '.west', 'config'), 'w').write(f'[manifest]\npath = {rel}\nfile = west.yml\n\n[zephyr]\nbase = {rel}\n')"


:: ----------------------------------------------------------------------------
:: 5. Locate Zephyr SDK Toolchain (External System Path or Workspace Fallback)
:: ----------------------------------------------------------------------------
:: Also accept ZEPHYR_SDK_INSTALL as alias for ZEPHYR_SDK_INSTALL_DIR
if not defined ZEPHYR_SDK_INSTALL_DIR (
    if defined ZEPHYR_SDK_INSTALL (
        set "ZEPHYR_SDK_INSTALL_DIR=%ZEPHYR_SDK_INSTALL%"
    )
)

if defined ZEPHYR_SDK_INSTALL_DIR (
    set "ZEPHYR_SDK_INSTALL_DIR=%ZEPHYR_SDK_INSTALL_DIR:"=%"
    if exist "%ZEPHYR_SDK_INSTALL_DIR%\sdk_version" (
        echo [INFO] Using external ZEPHYR_SDK_INSTALL_DIR: %ZEPHYR_SDK_INSTALL_DIR%
    ) else (
        echo [WARN] ZEPHYR_SDK_INSTALL_DIR is set to '%ZEPHYR_SDK_INSTALL_DIR%', but sdk_version was not found.
        set "ZEPHYR_SDK_INSTALL_DIR="
    )
)

:: If argument supplied, check if it points to an SDK
if "%~1" neq "" (
    if exist "%~f1\sdk_version" (
        set "ZEPHYR_SDK_INSTALL_DIR=%~f1"
    )
)

if not defined ZEPHYR_SDK_INSTALL_DIR (
    if exist "D:\Zephyr_Project\zephyr-sdk\sdk_version" (
        set "ZEPHYR_SDK_INSTALL_DIR=D:\Zephyr_Project\zephyr-sdk"
    ) else if exist "%THIRD_PARTY_DIR%\zephyr-sdk\sdk_version" (
        set "ZEPHYR_SDK_INSTALL_DIR=%THIRD_PARTY_DIR%\zephyr-sdk"
    ) else if exist "%TOOLS_DIR%\zephyr-sdk\sdk_version" (
        set "ZEPHYR_SDK_INSTALL_DIR=%TOOLS_DIR%\zephyr-sdk"
    ) else if exist "%PROJECT_ROOT%\..\zephyr-sdk\sdk_version" (
        for %%I in ("%PROJECT_ROOT%\..\zephyr-sdk") do set "ZEPHYR_SDK_INSTALL_DIR=%%~fI"
    )
)

:: Register Zephyr SDK with CMake if present
if defined ZEPHYR_SDK_INSTALL_DIR (
    if exist "%ZEPHYR_SDK_INSTALL_DIR%\cmake\zephyr_sdk_export.cmake" (
        where cmake >nul 2>&1
        if %errorlevel% equ 0 (
            cmake -P "%ZEPHYR_SDK_INSTALL_DIR%\cmake\zephyr_sdk_export.cmake" >nul 2>&1
        )
    )
)

:: ----------------------------------------------------------------------------
:: 6. Ensure Required Hardware Modules if workspace manifest is active
:: ----------------------------------------------------------------------------
if exist "%PROJECT_ROOT%\.west" (
    if not exist "%PROJECT_ROOT%\modules\hal\espressif\.git" (
        echo [INFO] Fetching hal_espressif module via west...
        west update hal_espressif 2>nul
    )
    if not exist "%PROJECT_ROOT%\modules\crypto\tinycrypt\.git" (
        echo [INFO] Fetching tinycrypt module via west...
        west update tinycrypt 2>nul
    )
    if not exist "%PROJECT_ROOT%\modules\hal\espressif\zephyr\blobs\lib\esp32c3\libbtdm_app.a" (
        echo [INFO] Fetching hal_espressif binary blobs via west...
        west blobs fetch hal_espressif 2>nul
    )
    if not exist "%PROJECT_ROOT%\modules\hal\rpi_pico\.git" (
        echo [INFO] Fetching hal_rpi_pico module via west...
        west update hal_rpi_pico 2>nul
    )
    if not exist "%PROJECT_ROOT%\modules\hal\cmsis\.git" (
        echo [INFO] Fetching cmsis module via west...
        west update cmsis 2>nul
    )
)

:: Sync workspace modules into ZEPHYR_BASE parent workspace if needed
for %%Z in ("%ZEPHYR_BASE%\..") do set "ZEPHYR_PARENT=%%~fI"
if exist "%ZEPHYR_BASE%" (
    for %%Z in ("%ZEPHYR_BASE%\..") do (
        if not exist "%%~fZ\.west\config" (
            if not exist "%%~fZ\.west" mkdir "%%~fZ\.west"
            (
                echo [manifest]
                echo path = zephyr
                echo file = west.yml
                echo [zephyr]
                echo base = zephyr
            ) > "%%~fZ\.west\config"
        )
        if not exist "%%~fZ\modules" (
            if exist "%PROJECT_ROOT%\modules" (
                echo [INFO] Syncing modules to %%~fZ\modules...
                xcopy /E /I /Q /Y "%PROJECT_ROOT%\modules" "%%~fZ\modules" >nul 2>&1
            )
        )
    )
)

:: ----------------------------------------------------------------------------
:: 7. Status Report
:: ----------------------------------------------------------------------------
echo.
echo [STATUS] Environment Configuration:
echo   - ZEPHYR_BASE:            %ZEPHYR_BASE%
if defined ZEPHYR_SDK_INSTALL_DIR goto :sdk_found
goto :sdk_not_found

:sdk_found
echo   - ZEPHYR_SDK_INSTALL_DIR: %ZEPHYR_SDK_INSTALL_DIR%
goto :sdk_check_done

:sdk_not_found
echo   - ZEPHYR_SDK_INSTALL_DIR: NOT FOUND
echo     ----------------------------------------------------------------------
echo     How to download and set up the Zephyr SDK:
echo       1. Download the Zephyr SDK bundle for Windows [v0.16.8 or newer]:
echo          https://github.com/zephyrproject-rtos/sdk-ng/releases
echo          e.g., zephyr-sdk-0.16.8_windows-x86_64.7z
echo.
echo       2. Extract it to your preferred directory [e.g., D:\zephyr-sdk-0.16.8]
echo.
echo       3. Register the toolchain in your environment:
echo          set ZEPHYR_SDK_INSTALL_DIR=D:\zephyr-sdk-0.16.8
echo          setx ZEPHYR_SDK_INSTALL_DIR "D:\zephyr-sdk-0.16.8"
echo.
echo       4. Run setup script inside the SDK:
echo          cd /d D:\zephyr-sdk-0.16.8 ^&^& setup.cmd
echo     ----------------------------------------------------------------------

:sdk_check_done

where west >nul 2>&1
if %errorlevel% equ 0 (
    echo   - west:                   AVAILABLE
) else (
    echo   - west:                   NOT FOUND
)

where cmake >nul 2>&1
if %errorlevel% equ 0 (
    echo   - cmake:                  AVAILABLE
) else (
    echo   - cmake:                  NOT FOUND [Install via: winget install cmake]
)

where ninja >nul 2>&1
if %errorlevel% equ 0 (
    echo   - ninja:                  AVAILABLE
) else (
    echo   - ninja:                  NOT FOUND [Install via: winget install Ninja-build.Ninja]
)

where wget >nul 2>&1
if %errorlevel% equ 0 (
    echo   - wget:                   AVAILABLE [MeshX portable shim]
) else (
    echo   - wget:                   NOT FOUND
)

echo ============================================================
echo MeshX Zephyr Environment Ready!
echo To build: meshx.py -b -H zephyr -B esp32c3_devkitm -N single_relay
echo ============================================================
echo.

:: Call base MeshX environment setup script post-completion
if exist "%TOOLS_BIN_DIR%\env.bat" (
    call "%TOOLS_BIN_DIR%\env.bat"
)

