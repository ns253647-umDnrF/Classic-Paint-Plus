# Building Classic Paint Plus

## Requirements

- Windows with Visual Studio 2022/2026 or Visual Studio Build Tools.
- Desktop development with C++, MSVC x86/x64 tools, matching **ATL** components,
  and Windows 10/11 SDK (both x86 and x64 libraries for both targets).
- CMake 3.20 or newer, available on PATH. NMake is provided by the C++ tools.
- Python 3.8 or newer for generating the Unicode resource file at build time.

The source archive contains the codec source. Building does not fetch external
dependencies or require Git, NASM or a package manager. Python is needed only
to build the program, not to run the resulting EXE. Resource sources stay UTF-8;
the generated UTF-16 input prevents older rc.exe versions from corrupting text.

## Build

Open the **x64 Native Tools Command Prompt** for a 64-bit build, or the
**x86 Native Tools Command Prompt** for a 32-bit build. Change to this directory:

```bat
build.cmd
```

Output: `build\x64\bin\ClassicPaintPlus.exe` or
`build\x86\bin\ClassicPaintPlus.exe`. Copy both HTML manuals and the license
notices together with the executable when distributing it. The script uses
Release mode, the NMake generator and the static MSVC runtime. Separate build
directories keep architectures apart.

Equivalent configuration, from the matching developer prompt:

```bat
cmake -S standalone -B build\x64 -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="%CD%\build\x64\bin"
cmake --build build\x64
```

Use `x86` in the paths from the x86 developer prompt. `standalone/CMakeLists.txt`
is the Windows build entry point; the root CMakeLists.txt belongs to the original
ReactOS source tree. Do not configure the root directly.

## Integration tests

```bat
build.cmd --tests
build\x64\bin\picker_test.exe --isolated --text-edit-regressions-only
build\x64\bin\picker_test.exe --isolated --objects-only
```

Use `build\x86` for x86. `--isolated` runs the GUI checks on a private desktop;
tests use a temporary INI and do not modify your application settings.
Some broader codec tests expect generated image fixtures used by the development
workspace; the focused tests above run directly from this source archive.

## Release details

Version macros are in `version.h`; they supply both the Windows version resource
and the About dialog. The published 2026.11.16 EXEs were built with MSVC
14.51.36231, ATL 14.32.31326 and static CRT/codecs, then tested on Windows 10 x64.
Other supported compiler/SDK installations can build equivalent functionality;
byte-identical output is not promised across toolchains.

All runtime DLL imports in the Windows packages are Windows system components.
The built-in MSVC runtime in the published packages targets Windows 10/11.
The x86 package is not a Windows XP build.
