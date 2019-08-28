COMPILE="arm-none-eabi-gcc -std=gnu99 -nostdlib -O2 -c -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -I../../lib -I../../include"
LINK="arm-none-eabi-gcc -T ../../link.ld -std=gnu99 -nostdlib -O2 -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard"
NAME=nanojpeg

mkdir -p build

$COMPILE -o build/$NAME.o $NAME.c
$COMPILE -o build/crt.o ../../lib/div/crt.S
$COMPILE -o build/idiv.o ../../lib/div/idiv.S
$COMPILE -o build/idivmod.o ../../lib/div/idivmod.S
$COMPILE -o build/ldivmod.o ../../lib/div/ldivmod.S
$COMPILE -o build/memset.o ../../lib/memory/memset.c
$COMPILE -o build/memcpy.o ../../lib/memory/memcpy.c
$COMPILE -o build/printf.o ../../lib/printf/printf.c
$COMPILE -o build/zz9k_main.o zz9k_main.c
#$COMPILE -o build/math_sinf.o math_sinf.c

$LINK -o $NAME build/zz9k_main.o build/nanojpeg.o build/mem*.o build/printf.o build/*div*.o build/crt.o -L../../lib/memory -lmemory_freelist 
arm-none-eabi-objcopy -O binary $NAME $NAME.bin

rm $NAME.lha
lha a0 $NAME.lha $NAME.bin


