@echo off
rem Workshop in this console. Its project is %USERPROFILE%\Zengine\terminal, made at the first
rem start and kept across packages, so a plan saved in the window's project does not start here.
rem Each start names this package's zengine-app in the project, where an exported app's run.cmd
rem finds it.
setlocal EnableExtensions DisableDelayedExpansion
chcp 65001 >nul
if not defined USERPROFILE goto noprofile
set "project=%USERPROFILE%\Zengine\terminal"
if not exist "%project%\" mkdir "%project%" || goto noproject
cd /d "%project%" || goto noproject
for %%a in ("%~dp0bin\zengine-app.exe") do (echo %%~a)> "%project%\zengine-app.path"
"%~dp0bin\zengine-workshop.exe" %*
if errorlevel 1 pause
exit /b
:noprofile
echo Workshop keeps your project in %%USERPROFILE%%\Zengine\terminal, and USERPROFILE is not set.
pause
exit /b 1
:noproject
echo Workshop could not make or enter its project folder, %USERPROFILE%\Zengine\terminal.
pause
exit /b 1
