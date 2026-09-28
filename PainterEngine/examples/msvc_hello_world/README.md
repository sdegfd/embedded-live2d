# PainterEngine MSVC Hello World

This example builds PainterEngine with the Visual Studio MSVC toolchain and opens a GUI window that renders `Hello World`.

## Files

- `main.c`: minimal `px_main` application.
- `build_msvc.bat`: locates Visual Studio, initializes MSVC, and builds the executable.
- `..\..\setup_painterengine_env.ps1`: writes `PainterEnginePath` into the current user's environment variables.

## Build

1. Run `powershell -ExecutionPolicy Bypass -File D:\live2d\PainterEngine\setup_painterengine_env.ps1`
2. Run `D:\live2d\PainterEngine\examples\msvc_hello_world\build_msvc.bat`

Output executable:

- `build\hello_world.exe`