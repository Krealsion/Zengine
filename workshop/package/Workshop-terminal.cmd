@echo off
rem Workshop in this console (--tui). Its project is %USERPROFILE%\Zengine, the window's, made at
rem the first start and kept across packages; a plan that project holds starts here too, in its
rem terminal window. Each start names this package's zengine-app in the project, where an exported
rem app's run.cmd finds it.
setlocal EnableExtensions DisableDelayedExpansion
chcp 65001 >nul
if not defined USERPROFILE goto noprofile
set "project=%USERPROFILE%\Zengine"
if not exist "%project%\" mkdir "%project%" || goto noproject
cd /d "%project%" || goto noproject
for %%a in ("%~dp0bin\zengine-app.exe") do (echo %%~a)> "%project%\zengine-app.path"
"%~dp0bin\zengine-workshop.exe" --tui %*
if errorlevel 1 pause
exit /b
:noprofile
echo Workshop keeps your project in %%USERPROFILE%%\Zengine, and USERPROFILE is not set.
pause
exit /b 1
:noproject
echo Workshop could not make or enter its project folder, %USERPROFILE%\Zengine.
pause
exit /b 1
