#include"compiler.h"

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char** argv)
{
	if(argc != 2){
		puts("Too few arguments");
		return 1;
	}
	FILE* fp = fopen(argv[1], "r");
	if(!fp){
		puts("Unable to open file");
		return 1;
	}

	fseek(fp, 0, SEEK_END);
	size_t plen = ftell(fp);
	rewind(fp);

	char* prog = malloc(plen);
	fread(prog, 1, plen, fp);

	size_t irsz;
	bfc_ir* ir = gen_ir(prog, plen, &irsz);

	for(size_t i = 0; i<irsz; i ++){
		const char* str = "";
		#define OP_MACRO(op) case op : str = #op ; break
		switch(ir[i].op){
			OP_MACRO(ADD);
			OP_MACRO(MOV);
			OP_MACRO(IN);
			OP_MACRO(OUT);
			OP_MACRO(LOOP_L);
			OP_MACRO(LOOP_R);
		}
		#undef OP_MACRO
		printf("%s, %d\n", str, ir[i].mag);
	}
	size_t mcsz;
	char* mc = gen_mc(ir, irsz, &mcsz);

	for(size_t i=0; i<mcsz; i++){
		printf("%x ", mc[i]);
		}

	/*
	mcsz = 9;
	char mc[9] = {0xb3, 0x2a,
	           0x66, 0x31, 0xc0,
						 0x66, 0x40,
	           0xcd, 0x80};
	mcsz =8;
	char mc[8] = {0x31, 0xC0,
	              0x40,
								0xB3, 0x2A,
								0xCD, 0x80, '\0'};
	*/
	write_elf(mc,mcsz, "b.out");
	return 0;
}
