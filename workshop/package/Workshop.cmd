@echo off
rem Workshop in a window. Its project is %USERPROFILE%\Zengine, made at the first start and kept
rem across packages; a plan that project holds (workshop-plan.json) starts instead of the shipped
rem one. Each start names this package's zengine-app in the project, where an exported app's
rem run.cmd finds it.
setlocal EnableExtensions DisableDelayedExpansion
chcp 65001 >nul
if not defined USERPROFILE goto noprofile
set "project=%USERPROFILE%\Zengine"
if not exist "%project%\" mkdir "%project%" || goto noproject
cd /d "%project%" || goto noproject
for %%a in ("%~dp0bin\zengine-app.exe") do (echo %%~a)> "%project%\zengine-app.path"
if exist "%project%\workshop-plan.json" goto projectplan
"%~dp0bin\zengine-workshop.exe" --load-plan "%~dp0bin\graphical-load-plan.json" %*
goto ended
:projectplan
"%~dp0bin\zengine-workshop.exe" %*
:ended
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
