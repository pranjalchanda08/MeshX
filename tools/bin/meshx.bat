@echo off
setlocal
python "%~dp0..\scripts\meshx.py" %*
exit /b %errorlevel%
