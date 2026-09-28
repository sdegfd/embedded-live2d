@echo off
setlocal
set "PROJECT_DIR=%~dp0"
set "ENGINE_DIR=D:\live2d\PainterEngine"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALLDIR=%%I"
if not defined VSINSTALLDIR exit /b 1
set "VSINSTALLDIR=%VSINSTALLDIR%\"
call "%VSINSTALLDIR%\VC\Auxiliary\Build\vcvars64.bat"
cl /nologo /W4 /TC /utf-8 /I "%ENGINE_DIR%" "%PROJECT_DIR%rt30_tests.c" "%ENGINE_DIR%\kernel\PX_LiveDeviceFormat.c" "%ENGINE_DIR%\kernel\PX_LiveRealtime.c" "%ENGINE_DIR%\core\PX_MemoryPool.c" "%ENGINE_DIR%\core\PX_Typedef.c" "%ENGINE_DIR%\core\PX_Log.c" /Fe:"%PROJECT_DIR%rt30_tests.exe" /Fo"%PROJECT_DIR%bench_build\\"
exit /b %ERRORLEVEL%
