COMPILE="arm-none-eabi-gcc -std=gnu99 -nostdlib -O2 -c -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -I../../lib -I../../include"
LINK="arm-none-eabi-gcc -T ../../link.ld -std=gnu99 -nostdlib -O2 -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard"
NAME=blur

cppcheck --template='{file}:{line}:{severity}:{message}' --enable=all -I. $NAME.c

mkdir -p build

$COMPILE -I. -ftree-vectorize -o build/$NAME.o $NAME.c
$COMPILE -o build/idiv.o ../../lib/div/idiv.S
$COMPILE -o build/idivmod.o ../../lib/div/idivmod.S
$COMPILE -o build/ldivmod.o ../../lib/div/ldivmod.S
$COMPILE -o build/memset.o ../../lib/memory/memset.c
$COMPILE -o build/memcpy.o ../../lib/memory/memcpy.c
$COMPILE -o build/printf.o ../../lib/printf/printf.c

$LINK -o $NAME build/$NAME.o build/memcpy.o build/memset.o build/printf.o build/*div*.o -L. -lm
arm-none-eabi-objcopy -O binary $NAME $NAME.bin

rm $NAME.lha
lha a0 $NAME.lha $NAME.bin

