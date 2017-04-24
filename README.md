# crc32c-fastzero
Measure performance of crc32c fastzero calculations.

# To build on ppc64le architecture.

	ppc # make clean
	rm -f main libcrc32c.so libcrc32c.a
	rm -f crc32c_ppc.o crc32c_ppc_fast_zero_asm.o crc32c_ppc_asm.o main.o
	ppc # 

	ppc # make 
	gcc -c -Wall -Werror -fpic -maltivec -DHAVE_POWER8 -I include crc32c_ppc.c crc32c_ppc_fast_zero_asm.S crc32c_ppc_asm.S
	gcc -shared -o libcrc32c.so crc32c_ppc.o crc32c_ppc_fast_zero_asm.o crc32c_ppc_asm.o
	gcc -c -maltivec -DHAVE_POWER8 main.c -o main.o
	gcc -maltivec -DHAVE_POWER8 main.o -L ./ -lcrc32c -lrt -o main
	ppc # 

# To build on x86 architecture.  

	x86 # make -f Makefile.intel clean
	rm -f main libcrc32c.so libcrc32c.a
	rm -f crc32c_intel_fast.o crc32c_intel_baseline.o crc32c_intel_fast_zero_asm.o crc32c_intel_fast_asm.o main.o
	x86 #

	x86 # make -f Makefile.intel 
	gcc -c -Wall -Werror -fpic -DHAVE_GOOD_YASM_ELF64 -I include crc32c_intel_fast.c crc32c_intel_baseline.c
	yasm -f elf64 crc32c_intel_fast_asm.S
	yasm -f elf64 crc32c_intel_fast_zero_asm.S
	gcc -shared -o libcrc32c.so crc32c_intel_fast.o crc32c_intel_baseline.o crc32c_intel_fast_zero_asm.o crc32c_intel_fast_asm.o
	gcc -c -DHAVE_GOOD_YASM_ELF64 main.c -o main.o
	gcc -DHAVE_GOOD_YASM_ELF64 main.o -L ./ -lcrc32c -lrt -o main
	x86 # 

# Sample steps to create a simple data pattern.

	ppc # printf '123456789' >foo
	ppc # 

	ppc # dd if=/dev/zero of=foo.zero count=1
	1+0 records in
	1+0 records out
	512 bytes copied, 0.000140484 s, 3.6 MB/s
	ppc #

	ppc # cat foo foo.zero >foo.padded
	ppc # 

# Sample steps to calculate and verify crc values.

	# ../main -f foo
	crc=0x58E3FA20
	ppc # ../main -f foo.zero
	crc=0x00000000
	ppc # ../main -f foo.padded
	crc=0x10376763
	ppc # 

	ppc # ../main -f foo -z 512
	crc=0x10376763
	ppc # ../main -f foo -z 512 -d
	crc=0x10376763
	ppc # ../main -f foo -z 512 -m
	crc=0x10376763
	ppc # 

# Sample steps to do a performance run.  

	ppc # ./main -p -m
	0 crc=0x105EC76F
	duration 633 nsec
	1 crc=0xF26B8303
	duration 1180 nsec
	2 crc=0x3FC5F181
	duration 1277 nsec
	3 crc=0x13A29877
	duration 1181 nsec
	4 crc=0x70A27D8A
	duration 1182 nsec
	5 crc=0xA541927E
	duration 1429 nsec
	6 crc=0xE964B13D
	duration 1365 nsec
	7 crc=0xDD45AAB8
	duration 1410 nsec
	8 crc=0x8F2261D3
	duration 1431 nsec
	9 crc=0x38116FAC
	duration 1215 nsec
	---
	avg duration 1230 nsec across 10 iterations
	ppc # 

