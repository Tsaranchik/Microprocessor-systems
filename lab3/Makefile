CC = avr-gcc
OBJCOPY = avr-objcopy
MCU = atmega32
F_CPU = 16000000UL
CFLAGS = -Wall -Os -mmcu=$(MCU) -DF_CPU=$(F_CPU)

TARGET = lab3
SRC = $(TARGET).c
ELF = $(TARGET).elf
HEX = $(TARGET).hex

all: $(HEX)

build: $(ELF)

hex: $(HEX)

$(ELF): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex -R .eeprom $< $@

clean:
	rm -f $(ELF) $(HEX)

.PHONY: all build hex clean
