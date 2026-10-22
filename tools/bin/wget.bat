@echo off
setlocal
where python >nul 2>&1
if %errorlevel% equ 0 (
    python "%~dp0..\scripts\wget_shim.py" %*
    exit /b %errorlevel%
)
where python3 >nul 2>&1
if %errorlevel% equ 0 (
    python3 "%~dp0..\scripts\wget_shim.py" %*
    exit /b %errorlevel%
)
if exist "%SystemRoot%\System32\curl.exe" (
    "%SystemRoot%\System32\curl.exe" -f -L %*
    exit /b %errorlevel%
)
curl -f -L %*
exit /b %errorlevel%
