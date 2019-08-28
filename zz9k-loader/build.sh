export VBCC=$HOME/code/vbcc
export INCLUDE=$HOME/code/vbcc/targets/m68k-amigaos/include
export INCLUDE2=$HOME/code/vbcc/targets/m68k-amigaos/include2
export PATH=$PATH:$VBCC/bin
export NAME=zz9k

cppcheck --template='{file}:{line}:{severity}:{message}' --enable=all -I. $NAME.c

vc +aos68k -I$INCLUDE -I$INCLUDE2 -I. -c99 -O2 -o $NAME $NAME.c -ldebug -lamiga -lauto

rm $NAME.lha
lha a0 $NAME.lha $NAME
