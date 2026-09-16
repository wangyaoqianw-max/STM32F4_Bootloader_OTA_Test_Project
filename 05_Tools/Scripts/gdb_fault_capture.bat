@echo off
call "%~dp0..\toolkit.bat" fault %*
exit /b %ERRORLEVEL%
