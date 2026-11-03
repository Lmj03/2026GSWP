# GLFW dependency for Neon Veil

- Version: **3.4**, official source distribution, unchanged.
- Download: https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
- Archive SHA-256: `b5ec004b2712fd08e8861dc271428f048775200a2df719ccf575143ba749a3e9`
- Complete distribution: `glfw-3.4/` (include, src, deps, CMake, docs, examples, tests, license).
- License: [zlib/libpng license](glfw-3.4/LICENSE.md). Upstream notices remain intact.
- `upstream.sha256`: hashes of all 362 distribution files, including binary resources.
- `GLFW.vcxproj`: repository-owned static Windows library project. This adapter is not an upstream file.

Open `SimpleGame.sln` with Visual Studio 2022 C++ desktop tools and a Windows 10/11 SDK.
The SimpleGame project references this project, so building either the solution or the game project
builds GLFW locally for the same Debug/Release and Win32/x64 selection. No download, CMake, vcpkg,
preinstalled GLFW, prebuilt compiler-specific GLFW .lib, or GLFW DLL is needed.

The library uses `_GLFW_WIN32`, C11, no precompiled header, and /MDd (Debug) or /MD (Release), matching
the application. It does not define `GLFW_DLL`, `_GLFW_BUILD_DLL` or application header options.
Outputs are ignored under `build/<Platform>/<Configuration>`; the source and build adapter are committed.
DirectInput/XInput headers come from the Windows SDK. GLFW loads optional system runtime APIs itself;
Vulkan/EGL/OSMesa SDK installation is not required for this WGL OpenGL application.

Do not format upstream sources. `.gitattributes` disables newline conversion in `glfw-3.4/` so
the package stays byte-identical. `tools/validate_glfw_migration.py` verifies hashes and build references
without compiling or launching anything.

The legacy FreeGLUT files elsewhere in Dependencies/output folders are not referenced by the new
application or link settings. GLFW itself is statically linked. Existing GLEW remains a separate dependency:
the tracked `Dependencies/Runtime/Win32` and `x64` DLLs are copied to the selected application output.
