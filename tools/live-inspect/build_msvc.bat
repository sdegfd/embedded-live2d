@echo off
setlocal
set "PROJECT_DIR=%~dp0"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALLDIR=%%I"
if not defined VSINSTALLDIR exit /b 1
set "VSINSTALLDIR=%VSINSTALLDIR%\"
call "%VSINSTALLDIR%VC\Auxiliary\Build\vcvars64.bat"
if not exist "%PROJECT_DIR%build\obj" mkdir "%PROJECT_DIR%build\obj"
rem PROJECT_DIR already ends in a backslash. Keep the forward slash on /Fo.
cl /nologo /W3 /O2 /utf-8 /D_CRT_SECURE_NO_WARNINGS /Fo"%PROJECT_DIR%build\obj/" "%PROJECT_DIR%live_inspect.c" /Fe"%PROJECT_DIR%live-inspect.exe" /link /SUBSYSTEM:CONSOLE
exit /b %ERRORLEVEL%
