# Makefile

NASM      := nasm
NASMFLAGS := -O2

SRC := m24fix.asm
LST := m24fix.lst
BIN := m24fix.com

all: $(BIN)

clean:
	rm -f $(BIN) $(LST)

m24fix.com: m24fix.asm
	$(NASM) $(NASMFLAGS) -f bin -o m24fix.com -l m24fix.lst m24fix.asm
