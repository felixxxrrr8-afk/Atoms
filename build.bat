@echo off
rem Build atoms.exe with MSVC (any installed Visual Studio / Build Tools), no external libraries
rem   build.bat        - release build
rem   build.bat asan   - debug build with AddressSanitizer (atoms_asan.exe, for error checking)
cd /d "%~dp0"
set "VSDIR="
for /d %%v in ("%ProgramFiles(x86)%\Microsoft Visual Studio\*" "%ProgramFiles%\Microsoft Visual Studio\*") do (
  for /d %%e in ("%%~v\*") do if exist "%%~e\VC\Auxiliary\Build\vcvars64.bat" set "VSDIR=%%~e"
)
if not defined VSDIR goto novs
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
rc /nologo atoms.rc
if errorlevel 1 goto failed
set "LIBS=user32.lib gdi32.lib opengl32.lib comdlg32.lib shell32.lib"
if /i "%1"=="asan" (
  cl /nologo /Od /Zi /fsanitize=address /openmp /utf-8 /EHsc /std:c++17 /W4 main.cpp atoms.res /Fe:atoms_asan.exe /link /SUBSYSTEM:WINDOWS %LIBS%
) else (
  cl /nologo /O2 /openmp /utf-8 /EHsc /std:c++17 /fp:fast /W3 main.cpp atoms.res /Fe:atoms.exe /link /SUBSYSTEM:WINDOWS %LIBS%
)
if errorlevel 1 goto failed
del main.obj atoms.res 2>nul
echo OK
exit /b 0
:novs
echo Visual Studio C++ tools not found
:failed
echo BUILD FAILED
if "%2"=="" pause
exit /b 1
