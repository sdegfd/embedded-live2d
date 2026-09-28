@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "PROJECT_DIR=%SCRIPT_DIR:~0,-1%"

if defined PainterEnginePath (
	set "PAINTERENGINE_DIR=%PainterEnginePath%"
) else (
	for %%I in ("%PROJECT_DIR%\..\..") do set "PAINTERENGINE_DIR=%%~fI"
)

if not exist "%PAINTERENGINE_DIR%\PainterEngine.h" (
	echo [ERROR] PainterEnginePath is invalid: "%PAINTERENGINE_DIR%"
	exit /b 1
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
	echo [ERROR] vswhere.exe not found.
	echo Install Visual Studio with the Desktop development with C++ workload.
	exit /b 1
)

for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALLDIR=%%I"
if not defined VSINSTALLDIR (
	echo [ERROR] Visual Studio with MSVC tools was not found.
	exit /b 1
)

set "VCVARS64=%VSINSTALLDIR%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS64%" (
	echo [ERROR] vcvars64.bat was not found under "%VSINSTALLDIR%".
	exit /b 1
)

call "%VCVARS64%" >nul
where cl >nul 2>nul
if errorlevel 1 (
	echo [ERROR] Failed to initialize the MSVC environment.
	exit /b 1
)

set "BUILD_DIR=%PROJECT_DIR%\build"
set "OBJ_DIR=%BUILD_DIR%\obj"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"

set "SOURCE_LIST=%BUILD_DIR%\sources.rsp"
set "CL_FLAGS=/nologo /W3 /EHsc /Zi /utf-8 /D_CRT_SECURE_NO_WARNINGS /Fo"%OBJ_DIR%\\""
set "INCLUDE_FLAGS=/I "%PAINTERENGINE_DIR%" /I "%PAINTERENGINE_DIR%\runtime" /I "%PAINTERENGINE_DIR%\platform\windows" /I "%PROJECT_DIR%""
set "LINK_FLAGS=/link /OUT:"%BUILD_DIR%\hello_world.exe" /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup d2d1.lib dsound.lib ws2_32.lib comdlg32.lib imm32.lib"

if exist "%SOURCE_LIST%" del "%SOURCE_LIST%"

>>"%SOURCE_LIST%" echo "%PROJECT_DIR%\main.c"
for %%F in ("%PAINTERENGINE_DIR%\core\*.c") do >>"%SOURCE_LIST%" echo "%%~fF"
for %%F in ("%PAINTERENGINE_DIR%\kernel\*.c") do >>"%SOURCE_LIST%" echo "%%~fF"
for %%F in ("%PAINTERENGINE_DIR%\runtime\*.c") do >>"%SOURCE_LIST%" echo "%%~fF"
for %%F in ("%PAINTERENGINE_DIR%\platform\windows\*.c") do >>"%SOURCE_LIST%" echo "%%~fF"
for %%F in ("%PAINTERENGINE_DIR%\platform\windows\*.cpp") do >>"%SOURCE_LIST%" echo "%%~fF"

echo [INFO] Building hello_world.exe with MSVC...
pushd "%PROJECT_DIR%" >nul
cl %CL_FLAGS% %INCLUDE_FLAGS% @"%SOURCE_LIST%" %LINK_FLAGS%
set "BUILD_EXIT=%ERRORLEVEL%"
popd >nul

if not "%BUILD_EXIT%"=="0" (
	echo [ERROR] Build failed.
	exit /b %BUILD_EXIT%
)

echo [OK] Built "%BUILD_DIR%\hello_world.exe"
exit /b 0