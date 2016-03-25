ARCH	:= x86_64

#  CC		:= gcc
CC		:= clang
LD		:= ld
QEMU	:= qemu-system-x86_64
OBJCOPY	:= objcopy

CFLAGS		:= -Wall
EFI_CFLAGS	:= -fno-stack-protector -fpic -fshort-wchar -mno-red-zone -DEFI_FUNCTION_WRAPER $(CFLAGS)

LIB_PATH	:= /usr/lib
EFI_PATH:= /usr/include/efi
EFI_INCLUDES := -I $(EFI_PATH) -I $(EFI_PATH)/$(ARCH)
EFI_LDS		:= $(LIB_PATH)/elf_x86_64_efi.lds
CRT0_EFI	:= $(LIB_PATH)/crt0-efi-x86_64.o

#  OVMF	:= run/OVMF.fd
OVMF	:= OVMF.fd
TARGET 	:= BOOTX64.efi

HDA		:= run/hda-contents
EFI_BOOT:= $(HDA)/EFI/BOOT/

C_OBJS	:= 
OBJS_T	:= main.o $(C_OBJS)
MAIN_SO	:= src/main.so 

OBJS 	:= $(foreach file, $(OBJS_T), src/$(file))
.PHONY:all
all: $(TARGET)

$(TARGET): $(MAIN_SO)
	mkdir -p $(HDA)/EFI/BOOT
	$(OBJCOPY) 					\
		-j .text                \
		-j .sdata               \
		-j .data                \
		-j .dynamic             \
		-j .dynsym              \
		-j .rel                 \
		-j .rela                \
		-j .reloc               \
		--target=efi-app-x86_64 \
		$< $@
	
$(MAIN_SO): $(OBJS) $(CRT0_EFI)
	$(LD) $^ 			\
		-nostdlib		\
		-znocombreloc	\
		-T $(EFI_LDS)	\
		-shared			\
		-Bsymbolic		\
		-L $(LIB_PATH)	\
		-l:libgnuefi.a	\
		-l:libefi.a		\
		-o $@

src/main.o: src/main.c Makefile
	$(CC) $< $(EFI_CFLAGS) $(EFI_INCLUDES) -c -o $@

.PHONY:run
run: run/$(OVMF) $(TARGET) $(EFI_BOOT)
#      mkdir -p $(HDA)/EFI/BOOT
	cp $(TARGET) $(HDA)/EFI/BOOT/
	$(QEMU) -L ./run -bios $(OVMF) -hda fat:$(HDA)

$(EFI_BOOT):
	mkdir -p $(HDA)/EFI/BOOT

run/$(OVMF):
	mkdir -p run
	wget https://sourceforge.net/projects/edk2/files/OVMF/OVMF-X64-r15214.zip 
	unzip OVMF-X64-r15214.zip OVMF.fd
	mv OVMF.fd $@

.PHONY:clean
clean:
	rm -f *.zip $(TARGET) $(OBJS) $(MAIN_SO)
