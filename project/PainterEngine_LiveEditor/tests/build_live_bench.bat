@echo off
setlocal
set "PROJECT_DIR=%~dp0"
set "ENGINE_DIR=%PROJECT_DIR%..\..\..\PainterEngine"
set "OUTDIR=%PROJECT_DIR%bench_build"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALLDIR=%%I"
if not defined VSINSTALLDIR exit /b 1
set "VSINSTALLDIR=%VSINSTALLDIR%\"
call "%VSINSTALLDIR%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%OUTDIR%" mkdir "%OUTDIR%"
set "RSP=%OUTDIR%\sources.rsp"
if exist "%RSP%" del "%RSP%"
>>"%RSP%" echo "%PROJECT_DIR%live_bench.c"
for %%F in ("%ENGINE_DIR%\core\*.c") do >>"%RSP%" echo "%%~fF"
for %%F in ("%ENGINE_DIR%\kernel\*.c") do >>"%RSP%" echo "%%~fF"
pushd "%OUTDIR%"
cl /nologo /O2 /W3 /EHsc /utf-8 /D_CRT_SECURE_NO_WARNINGS /I "%ENGINE_DIR%" @"%RSP%" /link /OUT:"%OUTDIR%\live_bench.exe" /SUBSYSTEM:CONSOLE
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
