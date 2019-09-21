@ECHO OFF
REM Windows specific vbcc build script
REM This implies that you have the following:
REM 1) vbcc already installed and in your path (check https://blitterstudio.com/) in D:\vbcc
REM 2) The NDK 3.9 included in your "aos68k" config
REM 3) The CyberGraphX DevKit extracted in D:\vbcc\cybergraphics
REM 
REM Please adapt the script accordingly if your environment is different
REM 
vc +aos68k -I"D:\vbcc\cybergraphics\include" -o .\zz9k .\zz9k.c -lamiga -lauto
