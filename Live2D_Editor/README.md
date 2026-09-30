# Live2D Editor

This is the active Windows PainterEngine LiveEditor. It supports layer and mesh authoring, Timeline frames, realtime RT30 axes, bake, preview and `.live` export. `build_msvc.bat` compiles this directory with the retained `../PainterEngine/` PC source tree. Embedded firmware does not link the PC engine or editor.

File → Import PSD reads a layered PSD or PSB, up to a 1280-pixel edge, and creates one texture and one root layer per pixel layer. Groups are expanded and are not turned into bones. Run `build_msvc.bat` from a Windows environment with MSVC installed. The `tests/` batch scripts build PC-side RT30 and editor benchmark checks. This Linux workspace has no MSVC; firmware and Host runtime tests validate playback, not the editor GUI.
