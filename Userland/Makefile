include Makefile.inc

MODULE=0000-sampleCodeModule.bin
SAMPLE_DATA=0001-sampleDataModule.bin
SOURCES=$(wildcard *.c)
SOURCES_SHELL=$(wildcard shell/*.c)
SOURCES_USRLIB=$(wildcard usrlib/*.c)
SOURCES_GOLF=$(wildcard golf/*.c)
SOURCES_ASM=$(wildcard asm/*.asm)
OBJECTS=$(SOURCES:.c=.o)
OBJECTS_SHELL=$(SOURCES_SHELL:.c=.o)
OBJECTS_USRLIB=$(SOURCES_USRLIB:.c=.o)
OBJECTS_GOLF=$(SOURCES_GOLF:.c=.o)
OBJECTS_ASM=$(SOURCES_ASM:.asm=.o)

all: $(MODULE) sampleDataModule

$(MODULE): $(OBJECTS) $(OBJECTS_SHELL) $(OBJECTS_USRLIB) $(OBJECTS_GOLF) $(OBJECTS_ASM)
	$(LD) $(LDFLAGS) -T sampleCodeModule.ld -o $(MODULE) $(OBJECTS) $(OBJECTS_SHELL) $(OBJECTS_USRLIB) $(OBJECTS_GOLF) $(OBJECTS_ASM)
%.o: %.c
	$(GCC) $(GCCFLAGS) -I. -Ishell -Iusrlib -Igolf -c $< -o $@
%.o: %.asm
	$(ASM) $(ASMFLAGS) $< -o $@

clean:
	rm -rf asm/*.o shell/*.o usrlib/*.o golf/*.o *.o $(MODULE) $(SAMPLE_DATA)

sampleDataModule:
	printf "This is sample data." >> $(SAMPLE_DATA) && dd if=/dev/zero bs=1 count=1 >> $(SAMPLE_DATA)

.PHONY: all clean sampleDataModule