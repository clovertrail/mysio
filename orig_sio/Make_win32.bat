
REM @echo off
REM Build sio_ntap using the Microsoft VC98 Package
rem
rem Root of Visual Developer Studio Common files.
set VSCommonDir=C:\PROGRA~1\MICROS~3\Common

rem
rem Root of Visual Developer Studio installed files.
rem
set MSDevDir=C:\PROGRA~1\MICROS~3\Common\msdev98

rem
rem Root of Visual C++ installed files.
rem
set MSVCDir=C:\PROGRA~1\MICROS~3\VC98

rem
rem VcOsDir is used to help create either a Windows 95 or Windows NT specific path.
rem
set VcOsDir=WIN95
if "%OS%" == "Windows_NT" set VcOsDir=WINNT

rem
echo Setting environment for using Microsoft Visual C++ tools.
rem

if "%OS%" == "Windows_NT" set PATH=%MSDevDir%\BIN;%MSVCDir%\BIN;%VSCommonDir%\TOOLS\%VcOsDir%;%VSCommonDir%\TOOLS;%PATH%
if "%OS%" == "" set PATH="%MSDevDir%\BIN";"%MSVCDir%\BIN";"%VSCommonDir%\TOOLS\%VcOsDir%";"%VSCommonDir%\TOOLS";"%windir%\SYSTEM";"%PATH%"
set INCLUDE=%MSVCDir%\ATL\INCLUDE;%MSVCDir%\INCLUDE;%MSVCDir%\MFC\INCLUDE;%INCLUDE%
set LIB=%MSVCDir%\LIB;%MSVCDir%\MFC\LIB;%LIB%

set VcOsDir=
set VSCommonDir=

REM NOW, do the actual building
cl  -o sio_ntap_win32.exe -c sio_ntap.c /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D _WIN32_WINNT=0x400 /YX /FD /c 
link /out:sio_ntap_win32.exe sio_ntap.obj kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /incremental:no /machine:I386
del *.obj *.idb *.pch
