@echo off

echo Build script for Steem SSE - BCC build

rem local:

cd c:\data\prg\st\windows-build\bcc
set NASMPATH=D:\Console\nasm\
set BCCROOT=D:\Console\bcc\
set BCCPATH=D:\Console\bcc\bin\
set COPYPATH=G:\emu\st\bin\steem\
set ROOT=c:\data\prg\st\
rem set ROOT=G:\Downloads\steemsse-code\steemsse\
rem the rest should be the same for all systems:

set PROGRAMNAME=SteemBeta BCC.exe
rem manually create obj directory if necessary
set OUT=obj
del "%OUT%\*.exe"

rem delete objects if you want to remake them (rare)
if NOT EXIST obj\asm_draw.obj (
@echo ON
"%NASMPATH%nasm" -o obj\asm_draw.obj -fobj -dWIN32 -w+macro-params -w+macro-selfref -w+orphan-labels -i%ROOT%\steem\asm\ %ROOT%\steem\asm\asm_draw.asm
)
if NOT EXIST obj\asm_osd_draw.obj (
@echo ON
"%NASMPATH%nasm" -o obj\asm_osd_draw.obj -fobj -dWIN32 -w+macro-params -w+macro-selfref -w+orphan-labels -i%ROOT%\steem\asm\ %ROOT%\steem\asm\asm_osd_draw.asm
)
if NOT EXIST obj\asm_portio.obj (
@echo ON
"%NASMPATH%nasm" -o obj\asm_portio.obj -fobj -dWIN32 %ROOT%\include\asm\asm_portio.asm
)
@echo OFF

echo -----------------------------------------------
echo Building 3rd party code using Borland C/C++ 5.5
echo -----------------------------------------------
"%BCCPATH%make.exe" -fmakefile.txt -DDONT_ALLOW_DEBUG -DSTEVEN_SEAGAL 3rdparty
echo ------------------------------------------
echo Building Steem SSE using Borland C/C++ 5.5
echo ------------------------------------------
"%BCCPATH%make.exe" -fmakefile.txt -B -DDONT_ALLOW_DEBUG -DBCC_BUILD -DSTEVEN_SEAGAL

ren "%OUT%\Steem.exe" "%PROGRAMNAME%"
copy "%OUT%\%PROGRAMNAME%" "%COPYPATH%"

pause
:END_BUILD