@echo off
call "%~dp0..\toolkit.bat" build %*
exit /b %ERRORLEVEL%
