:; echo "This script is only for use on Windows NT platforms within Visual Studio Native Tools Command Line"; echo "On Unix systems, no preconfiguration is required."; exit 1

@ECHO OFF
SETLOCAL

IF NOT DEFINED VSCMD_VER (
	ECHO You must run this within Visual Studio Native Tools Command Line
	EXIT /B 1
)

WHERE GNUMAKE.EXE >nul 2>nul
IF ERRORLEVEL 1 (
	ECHO GNUMAKE.EXE was not found
	REM You can compile GNU Make within Visual Studio Native Tools Command
	REM Line natively by executing build_w32.bat within the root folder of
	REM GNU Make source code.
	REM You can download GNU Make source code at
	REM   http://ftpmirror.gnu.org/make/
	EXIT /B 1
)

set sdlincdir=ext\inc
set sdllibdir=ext\lib

rem Extract SDL2 headers and pre-compiled libraries
mkdir %sdlincdir%
mkdir %sdllibdir%
expand ext\sdl2_2-0-12_inc.cab -F:* %sdlincdir%
expand ext\sdl2_2-0-12_%VSCMD_ARG_TGT_ARCH%.cab -F:* %sdllibdir%
if errorlevel 1 (
	echo Could not expand tools\sdl2_2-0-12_%VSCMD_ARG_TGT_ARCH%.cab 
	exit /B
)

cls
echo Done. Execute gnumake.exe to start build.

ENDLOCAL
@ECHO ON
