@echo off
setlocal
python "%~dp0meshx.py" %*
exit /b %errorlevel%
