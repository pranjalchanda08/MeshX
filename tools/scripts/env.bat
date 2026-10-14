@echo off
:: -----------------------------------------------------------
:: MeshX Environment Setup Script (Windows Batch)
:: Must be called with:  call tools\scripts\env.bat [extra_command]
:: Example:  call tools\scripts\env.bat D:\esp\v5.4.4\esp-idf\export.bat
:: -----------------------------------------------------------

:: Resolve canonical project root and tools\scripts paths
for %%I in ("%~dp0..\..") do set "PROJECT_ROOT=%%~fI"
set "SCRIPTS_DIR=%~dp0"
:: Strip trailing backslash from SCRIPTS_DIR if present
if "%SCRIPTS_DIR:~-1%"=="\" set "SCRIPTS_DIR=%SCRIPTS_DIR:~0,-1%"

echo Project root set to: %PROJECT_ROOT%

:: Add tools\scripts to PATH so meshx.bat and meshx.py can be invoked from anywhere
set "PATH=%SCRIPTS_DIR%;%PATH%"
echo PATH updated: added %SCRIPTS_DIR%

:: Check for python availability
where python >nul 2>&1
if %errorlevel% neq 0 (
    where python3 >nul 2>&1
    if %errorlevel% neq 0 (
        echo ERROR: python/python3 could not be found, please install it or add it to PATH.
        exit /b 1
    )
)

:: Execute additional command line argument if provided (e.g. export.bat for ESP-IDF)
if "%~1" neq "" (
    echo Executing additional command: %*
    call %*
    if errorlevel 1 (
        echo WARNING: Additional command exited with code %errorlevel%
    )
)

echo.
echo Environment setup complete.
echo You can now run: meshx.bat -h or meshx -h from any directory.
echo.