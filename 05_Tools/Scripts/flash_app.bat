@echo off
call "%~dp0..\toolkit.bat" flash %*
exit /b %ERRORLEVEL%
