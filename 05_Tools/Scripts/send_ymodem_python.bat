@echo off
call "%~dp0..\toolkit.bat" ymodem python %*
exit /b %ERRORLEVEL%
