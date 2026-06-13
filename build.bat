@echo off
chcp 65001 >nul
setlocal

rem ===== VSCode + MSVC + EasyX build script =====
rem User-provided EasyX locations:
set "VSROOT=C:\Program Files\Microsoft Visual Studio\18\Community"
set "EASYX_INC=%VSROOT%\VC\Auxiliary\VS\include"
set "EASYX_LIB=%VSROOT%\VC\Auxiliary\VS\lib\x64"
set "VSCMD=%VSROOT%\Common7\Tools\VsDevCmd.bat"

if not exist "%VSCMD%" (
    echo Cannot find VsDevCmd.bat:
    echo %VSCMD%
    echo Please check VSROOT in build.bat.
    pause
    exit /b 1
)

if not exist "%EASYX_INC%\graphics.h" (
    echo Cannot find EasyX header graphics.h:
    echo %EASYX_INC%\graphics.h
    echo Please check EASYX_INC in build.bat.
    pause
    exit /b 1
)

if not exist "%EASYX_LIB%\EasyXw.lib" (
    echo Cannot find EasyXw.lib:
    echo %EASYX_LIB%\EasyXw.lib
    echo This project uses Unicode Chinese text, so EasyXw.lib is recommended.
    echo Please check EASYX_LIB in build.bat.
    pause
    exit /b 1
)

call "%VSCMD%" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 (
    echo Failed to initialize MSVC x64 environment.
    pause
    exit /b 1
)

if not exist build mkdir build

cl /nologo /std:c++17 /EHsc /utf-8 /Zi /DUNICODE /D_UNICODE ^
    /I"%EASYX_INC%" ^
    "%~dp0ShanghaiMetroEasyX.cpp" ^
    /Fe:"%~dp0build\ShanghaiMetroEasyX.exe" ^
    /link /LIBPATH:"%EASYX_LIB%" EasyXw.lib winmm.lib msimg32.lib user32.lib gdi32.lib shell32.lib ole32.lib uuid.lib

if errorlevel 1 (
    echo.
    echo Build failed.
    pause
    exit /b 1
)

echo.
echo Build succeeded: %~dp0build\ShanghaiMetroEasyX.exe
