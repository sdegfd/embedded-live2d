@echo off
setlocal
set "PROJECT_DIR=%~dp0"
set "ENGINE_DIR=%PROJECT_DIR%..\..\PainterEngine"
if not defined OUTPUT_NAME set "OUTPUT_NAME=PainterEngine.exe"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALLDIR=%%I"
if not defined VSINSTALLDIR exit /b 1
set "VSINSTALLDIR=%VSINSTALLDIR%\"
call "%VSINSTALLDIR%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%PROJECT_DIR%build\obj" mkdir "%PROJECT_DIR%build\obj"
set "RSP=%PROJECT_DIR%build\sources.rsp"
if exist "%RSP%" del "%RSP%"
for %%F in ("%PROJECT_DIR%*.c") do >>"%RSP%" echo "%%~fF"
for %%F in ("%ENGINE_DIR%\core\*.c") do >>"%RSP%" echo "%%~fF"
for %%F in ("%ENGINE_DIR%\kernel\*.c") do >>"%RSP%" echo "%%~fF"
for %%F in ("%ENGINE_DIR%\runtime\*.c") do >>"%RSP%" echo "%%~fF"
for %%F in ("%ENGINE_DIR%\platform\windows\*.c") do >>"%RSP%" echo "%%~fF"
for %%F in ("%ENGINE_DIR%\platform\windows\*.cpp") do >>"%RSP%" echo "%%~fF"
pushd "%PROJECT_DIR%"
cl /nologo /W3 /EHsc /utf-8 /D_CRT_SECURE_NO_WARNINGS /I "%ENGINE_DIR%" /I "%ENGINE_DIR%\runtime" /I "%ENGINE_DIR%\platform\windows" /I "%PROJECT_DIR%" /Fo"%PROJECT_DIR%build\obj\" @"%RSP%" /link /OUT:"%PROJECT_DIR%%OUTPUT_NAME%" /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup d2d1.lib dsound.lib ws2_32.lib comdlg32.lib imm32.lib advapi32.lib
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
