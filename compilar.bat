@echo off
cd /d "%~dp0"

rem donde va el dll
set "DESTINO=G:\Modding\FALLOUT 4 NG\Data\F4SE\Plugins"

rem sin esto a veces crashea el compilador
del /s /q build\release-msvc-msvc\*.iobj build\release-msvc-msvc\*.ipdb >nul 2>&1

cmake --build --preset release-msvc-msvc || (echo. & echo ===== ERROR ===== & pause & exit /b 1)

rem al juego
copy /y "build\release-msvc-msvc\RelWithDebInfo\SmoothHeadtracking.dll" "%DESTINO%\" >nul
copy /y "build\release-msvc-msvc\RelWithDebInfo\SmoothHeadtracking.pdb" "%DESTINO%\" >nul

echo. & echo Done
pause