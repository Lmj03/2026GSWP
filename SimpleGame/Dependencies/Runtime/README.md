# Existing GLEW runtime files

These are unchanged copies of the runtime binaries already tracked by this repository:

- `Win32/glew32.dll`: copied from `Debug/glew32.dll`, PE machine I386.
- `x64/glew32.dll`: copied from `x64/Debug/glew32.dll`, PE machine AMD64.

The game project copies the matching file to `$(OutDir)` after a successful build. GLEW headers and
import libraries remain in `Dependencies/glew.h`, `Dependencies/glew32.lib` and `Dependencies/x64/glew32.lib`.
GLFW is compiled from source into a static library and has no additional runtime DLL.
GLEW redistribution notices are included in the existing `Dependencies/glew.h` copyright/license section.
