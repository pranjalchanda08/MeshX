@echo off
setlocal enabledelayedexpansion

for %%I in ("%~dp0..\..") do set "REPO_ROOT=%%~fI"

:: Read .env.local if present
if exist "%~dp0.env.local" (
    for /f "usebackq tokens=1,* delims==" %%A in ("%~dp0.env.local") do (
        set "KEY=%%A"
        set "VAL=%%B"
        if not "!KEY!"=="" (
            if not "!KEY:~0,1!"=="#" (
                set "!KEY!=!VAL!"
            )
        )
    )
)

if defined MESHX_EXPORT_SCRIPT (
    call "%REPO_ROOT%\tools\bin\env.bat" "!MESHX_EXPORT_SCRIPT!" >&2
) else (
    call "%REPO_ROOT%\tools\bin\env.bat" >&2
)

python "%~dp0meshx_mcp_server.py"
