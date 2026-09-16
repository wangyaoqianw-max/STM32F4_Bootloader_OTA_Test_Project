@echo off
call "%~dp0..\toolkit.bat" ymodem tera %*
exit /b %ERRORLEVEL%
