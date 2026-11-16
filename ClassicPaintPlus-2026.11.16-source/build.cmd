@echo off
setlocal
if /i "%~1"=="--tests" goto args_ok
if not "%~1"=="" goto usage
:args_ok
if not "%~2"=="" goto usage
set "CPP_ARCH=%VSCMD_ARG_TGT_ARCH%"
if /i "%CPP_ARCH%"=="x64" goto arch_ok
if /i "%CPP_ARCH%"=="x86" goto arch_ok
echo Open an x64 or x86 Native Tools Command Prompt for Visual Studio first.
exit /b 2
:arch_ok
set "CPP_TESTS=OFF"
if /i "%~1"=="--tests" set "CPP_TESTS=ON"
cmake -S "%~dp0standalone" -B "%~dp0build\%CPP_ARCH%" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="%~dp0build\%CPP_ARCH%\bin" -DPAINT_BUILD_PICKER_TESTS=%CPP_TESTS%
if errorlevel 1 exit /b 1
cmake --build "%~dp0build\%CPP_ARCH%"
if errorlevel 1 exit /b 1
echo Output: %~dp0build\%CPP_ARCH%\bin\ClassicPaintPlus.exe
exit /b 0
:usage
echo Usage: build.cmd [--tests]
exit /b 2
