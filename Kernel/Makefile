include Makefile.inc

ifdef MM
    MM_FLAG := -D$(MM)
else
    MM_FLAG :=
endif

KERNEL=kernel.bin
SOURCES=$(wildcard *.c)
SOURCES_IDT=$(wildcard idt/*.c)
SOURCES_DRIVERS=$(wildcard drivers/*.c)
SOURCES_MEMORY=$(wildcard memory/*.c)
SOURCES_ASM=$(wildcard asm/*.asm)
SOURCES_ASM_IDT=$(wildcard idt/*.asm)

OBJECTS=$(SOURCES:.c=.o) $(SOURCES_IDT:.c=.o) $(SOURCES_DRIVERS:.c=.o) $(SOURCES_MEMORY:.c=.o)
OBJECTS_ASM=$(SOURCES_ASM:.asm=.o) $(SOURCES_ASM_IDT:.asm=.o)
LOADERSRC=loader.asm

LOADEROBJECT=$(LOADERSRC:.asm=.o)
STATICLIBS=

all: $(KERNEL)

$(KERNEL): $(LOADEROBJECT) $(OBJECTS) $(STATICLIBS) $(OBJECTS_ASM)
	$(LD) $(LDFLAGS) -T kernel.ld -o $(KERNEL) $(LOADEROBJECT) $(OBJECTS) $(OBJECTS_ASM) $(STATICLIBS)

%.o: %.c
	$(GCC) $(GCCFLAGS) -I./include -c $< -o $@ $(MM_FLAG)

%.o : %.asm
	$(ASM) $(ASMFLAGS) $< -o $@

$(LOADEROBJECT):
	$(ASM) $(ASMFLAGS) $(LOADERSRC) -o $(LOADEROBJECT)

clean:
	rm -rf asm/*.o *.o drivers/*.o idt/*.o *.bin

.PHONY: all clean
