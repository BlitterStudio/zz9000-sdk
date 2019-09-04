COMPILE="arm-none-eabi-gcc -std=gnu99 -nostdlib -O2 -c -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -I../../lib -I../../include"
LINK="arm-none-eabi-gcc -T ../../link.ld -std=gnu99 -nostdlib -O2 -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard"
NAME=raytrace

mkdir build

#$COMPILE -S -fverbose-asm -ftree-vectorize -fno-threadsafe-statics -fno-stack-protector -fno-rtti -fno-exceptions -I. -o build/zz9ktrace.asm zz9ktrace.cpp
$COMPILE -ftree-vectorize -fno-threadsafe-statics -fno-stack-protector -fno-rtti -fno-exceptions -o build/zz9ktrace.o zz9ktrace.cpp

$COMPILE -o build/idiv.o ../../lib/div/idiv.S
$COMPILE -o build/idivmod.o ../../lib/div/idivmod.S
$COMPILE -o build/ldivmod.o ../../lib/div/ldivmod.S
$COMPILE -o build/printf.o ../../lib/printf/printf.c
$COMPILE -o build/zz9ktrace-main.o zz9ktrace-main.c

$LINK -o $NAME build/zz9ktrace-main.o build/zz9ktrace.o build/printf.o build/*div*.o -L. -lm

arm-none-eabi-objcopy -O binary $NAME $NAME.bin

rm $NAME.lha
lha a0 $NAME.lha $NAME.bin

