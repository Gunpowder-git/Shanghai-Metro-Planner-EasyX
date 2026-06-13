ShanghaiMetroEasyX VSCode package - Chinese input fixed

Usage:
1. Open this folder in VSCode.
2. Press Ctrl+Shift+B to build, or double-click build.bat.
3. Press F5 to build and run, or double-click run.bat.
4. After building, the executable is at: build\ShanghaiMetroEasyX.exe

Configured paths:
EasyX include: C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\VS\include
EasyX lib:     C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\VS\lib\x64

Input notes:
- Click a text box first, then type.
- Backspace deletes one character.
- Delete clears the current text box.
- Ctrl+A also clears the current text box.
- Enter queries the route.

This version fixes Chinese input mojibake by reading EX_CHAR messages and handling WM_IME_CHAR conversion.
