# Platform-specific options

CC=arm-none-eabi-gcc
CPP=arm-none-eabi-gcc

CFLAGS=-Os -g3 -Wall -pedantic -c -mthumb -mcpu=cortex-m0 -DARM  -DUSE_STDPERIPH_DRIVER -D__CHECK_DEVICE_DEFINES -DUSE_FULL_ASSERT -fmessage-length=0 -fdata-sections -ffunction-sections -DK_ADDR=uint32_t -DK_WORD=uint32_t
CPPFLAGS=-Os -g3 -Wall -pedantic -c -mthumb -mcpu=cortex-m0 -DARM  -DUSE_STDPERIPH_DRIVER -D__CHECK_DEVICE_DEFINES -DUSE_FULL_ASSERT -fmessage-length=0 -fdata-sections -ffunction-sections -DK_ADDR=uint32_t -DK_WORD=uint32_t

LINK=arm-none-eabi-gcc
LFLAGS= -Wl,--start-group -Wl,-lm  -Wl,--end-group -Wl,--gc-sections -mthumb -mcpu=cortex-m0
LFLAGS_DBG= -Wl,--start-group -Wl,-lm  -Wl,--end-group -mthumb -mcpu=cortex-m0

AR=arm-none-eabi-ar
ARFLAGS=rcs

OBJCOPY=arm-none-eabi-objcopy
OBJCOPY_FLAGS=-O ihex -R .eeprom -R .fuse -R .lock -R .signature
OBJCOPY_DBG_FLAGS=--only-section=.logger -O binary --set-section-flags .logger=alloc --change-section-address .logger=0

CFLAGS+=-I/usr/lib/avr/include/
CPPFLAGS+=-I/usr/lib/avr/include/
