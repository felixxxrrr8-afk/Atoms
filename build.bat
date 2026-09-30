@echo off
rem Build atoms.exe with MSVC (any installed Visual Studio / Build Tools), no external libraries
rem   build.bat        - release build
rem   build.bat asan   - debug build with AddressSanitizer (atoms_asan.exe, for error checking)
rem   build.bat setup  - release build and the one-file installer AtomsSetup.exe
rem                      (program, manuals, license and the OpenMP runtime inside; see installer\setup.cpp)
cd /d "%~dp0"
set "ROOT=%CD%"
set "VSDIR="
for /d %%v in ("%ProgramFiles(x86)%\Microsoft Visual Studio\*" "%ProgramFiles%\Microsoft Visual Studio\*") do (
  for /d %%e in ("%%~v\*") do if exist "%%~e\VC\Auxiliary\Build\vcvars64.bat" set "VSDIR=%%~e"
)
if not defined VSDIR goto novs
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
rc /nologo /fo res\atoms.res res\atoms.rc
if errorlevel 1 goto failed
set "LIBS=user32.lib gdi32.lib opengl32.lib comdlg32.lib shell32.lib"
if /i "%1"=="asan" (
  cl /nologo /Od /Zi /fsanitize=address /openmp /utf-8 /EHsc /std:c++17 /W4 main.cpp res\atoms.res /Fe:atoms_asan.exe /link /SUBSYSTEM:WINDOWS %LIBS%
) else (
  cl /nologo /O2 /openmp /utf-8 /EHsc /std:c++17 /fp:fast /W3 main.cpp res\atoms.res /Fe:atoms.exe /link /SUBSYSTEM:WINDOWS %LIBS%
)
if errorlevel 1 goto failed
del main.obj res\atoms.res 2>nul
if /i "%1"=="setup" (
  call :setup
  if errorlevel 1 goto failed
)
echo OK
exit /b 0

rem ---- installer: files are staged in .build\setup; uninstall.exe is built first and goes inside AtomsSetup.exe
:setup
set "OUT=%ROOT%\.build\setup"
if not exist "%OUT%" mkdir "%OUT%"
set "OMP="
for /d %%r in ("%VSDIR%\VC\Redist\MSVC\*") do for /d %%o in ("%%~r\x64\Microsoft.VC*.OpenMP") do if exist "%%~o\vcomp140.dll" set "OMP=%%~o\vcomp140.dll"
if not defined OMP (echo vcomp140.dll not found in "%VSDIR%\VC\Redist" & exit /b 1)
copy /y atoms.exe "%OUT%\atoms.exe" >nul
copy /y "%OMP%" "%OUT%\vcomp140.dll" >nul
copy /y docs\MANUAL.html "%OUT%\manual_en.html" >nul
rem the Russian manual has a Cyrillic name: take the other html in docs
for %%f in (docs\*.html) do if /i not "%%~nxf"=="MANUAL.html" copy /y "%%f" "%OUT%\manual_ru.html" >nul
copy /y LICENSE "%OUT%\LICENSE.txt" >nul
copy /y installer\setup.rc "%OUT%" >nul
copy /y installer\setup.manifest "%OUT%" >nul
copy /y res\atoms.ico "%OUT%" >nul
set "VER="
for /f "tokens=3 delims=, " %%a in ('findstr ProductVersion res\atoms.rc') do set "VER=%%~a"
if not defined VER (echo ProductVersion not found in res\atoms.rc & exit /b 1)
> "%OUT%\ver.h" echo #define APP_VER L"%VER%"
>> "%OUT%\ver.h" echo #define APP_VER_A "%VER%"
>> "%OUT%\ver.h" echo #define APP_VER_NUM %VER:.=,%,0,0
set "SLIBS=user32.lib gdi32.lib shell32.lib ole32.lib advapi32.lib comctl32.lib shcore.lib uuid.lib"
set "SFLAGS=/nologo /O1 /utf-8 /EHsc /std:c++17 /W3 /I."
pushd "%OUT%"
rc /nologo /d UNINSTALLER /fo uninstall.res setup.rc || goto setupfail
cl %SFLAGS% /DUNINSTALLER "%ROOT%\installer\setup.cpp" uninstall.res /Fo:uninstall.obj /Fe:uninstall.exe /link /SUBSYSTEM:WINDOWS %SLIBS% || goto setupfail
rc /nologo /fo setup.res setup.rc || goto setupfail
cl %SFLAGS% "%ROOT%\installer\setup.cpp" setup.res /Fo:setup.obj /Fe:AtomsSetup.exe /link /SUBSYSTEM:WINDOWS %SLIBS% || goto setupfail
popd
copy /y "%OUT%\AtomsSetup.exe" AtomsSetup.exe >nul
exit /b 0
:setupfail
popd
exit /b 1

:novs
echo Visual Studio C++ tools not found
:failed
echo BUILD FAILED
if "%2"=="" pause
exit /b 1
