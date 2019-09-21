@ECHO OFF
REM Windows specific vbcc build script
REM This implies that you have the following:
REM 1) vbcc already installed and in your path (check https://blitterstudio.com/) in D:\vbcc
REM 2) The NDK 3.9 included in your "aos68k" config
REM 3) The P96 DevKit extracted in D:\vbcc\P96
REM 
REM Please adapt the script accordingly if your environment is different
REM 
vc +aos68k -I"D:\vbcc\P96\Include" -c99 -O2 -o .\zz9k .\zz9k.c -lamiga -lauto

del zz9k.lha
lha a zz9k.lha zz9k
