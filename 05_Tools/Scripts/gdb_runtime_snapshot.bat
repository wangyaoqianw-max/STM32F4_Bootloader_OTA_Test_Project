@echo off
call "%~dp0..\toolkit.bat" snapshot %*
exit /b %ERRORLEVEL%
