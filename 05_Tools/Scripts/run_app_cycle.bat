@echo off
call "%~dp0..\toolkit.bat" run %*
exit /b %ERRORLEVEL%
