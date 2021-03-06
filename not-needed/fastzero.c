/* Copyright (C) 2017 International Business Machines Corp.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include <stdio.h>
#include <stdlib.h>
#include <altivec.h>

#include "fastzero_table.h"

/*
 * we could do better if I could get a handle on byte reflection in crc32-vpmsum
 * I have no doubt that given a day or two, I could, but I don't have that long!
 * At any rate, we're only doing 2 reflections, so it's not so bad.
 */

static inline unsigned long polynomial_multiply(unsigned int a, unsigned int b) {
	vector unsigned int va = {a, 0, 0, 0};
	vector unsigned int vb = {b, 0, 0, 0};
	vector unsigned long vt;

	__asm__("vpmsumw %0,%1,%2" : "=v"(vt) : "v"(va), "v"(vb));

	return vt[0];
}

static unsigned int reflect32(unsigned int num) {
	/*
	 * this can be improved, but for the simplicity of licensing I have
	 * written down the first correct implementation that came to mind
	 */
	int i;
	unsigned int result = 0;
	for (i=0; i<32; i++) {
		if (num & (1 << i))
			result |= 1 << (31 - i);
	}
	return result;
}

unsigned int barrett_reduction(unsigned long val);

static inline unsigned int gf_multiply(unsigned int a, unsigned int b) {
	return barrett_reduction(polynomial_multiply(a, b));
}

unsigned int append_zeros(unsigned int crc, unsigned long length) {
	unsigned long i = 0;

	while (length) {
		if (length & 1) {
			crc = gf_multiply(crc, crc_zero[i]);
		}
		i++;
		length /= 2;
	}

	return crc;
}

int main(int argc, char ** argv) {
        unsigned int crc;
	unsigned long bytes;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <original crc> <number of bytes of 0s to append>\n",
			argv[0]);
		return 1;
	}

	crc = (unsigned int)strtoul(argv[1], NULL, 16);

	/* 
	 * here we assume (for Ceph) that there is no XOR required at beginning or end
	 * c.f. the usual definition of CRC32c that does require XOR by 0xffffffff on
	 * either side.
	 *
	 * Reflection is still required.
	 */
	crc = reflect32(crc);

	bytes = strtoul(argv[2], NULL, 0);

	crc = append_zeros(crc, bytes);
	crc = reflect32(crc);

	printf("New CRC: %x\n", crc);

	return 0;
}
