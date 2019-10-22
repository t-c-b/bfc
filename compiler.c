#include "compiler.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <elf.h>

bfc_ir* gen_ir(const char* prog, size_t prsz, size_t* irsz)
{
	bfc_ir* ir = calloc(prsz, sizeof(bfc_ir));
	size_t count = 0;
	int depth = 0;

	for(size_t i=0; i<prsz; i++){

		switch(prog[i]){
		case '+': case '-':
			ir[count].op = ADD;
			while(prog[i] == '+' || prog[i] == '-'){
				switch(prog[i]){
					case '+': ir[count].mag++; break;
					case '-': ir[count].mag--; break;
				}
				i++;
			}
			i--;
			break;

		case '>': case '<':
			ir[count].op = MOV;
			while(prog[i] == '>' || prog[i] == '<'){
				switch(prog[i]){
					case '>': ir[count].mag++; break;
					case '<': ir[count].mag--; break;
				}
				i++;
			}
			i--;
			break;

		case '.':
			ir[count].op = OUT;
			break;

		case ',':
			ir[count].op = IN;
			break;

		case '[':
			ir[count].op = LOOP_L;
			depth++;
			break;

		case ']':
			ir[count].op = LOOP_R;
			depth--;
			break;

		default:
			count--; //Don't increment count if no operation
			break;
		}
		count++;
	}
	ir = realloc(ir, count*sizeof(bfc_ir));
	*irsz = count;
	return ir;
}

//INIT
static const char MOV_ECX[1] = {0xb9};

//ADD
static const char MOV_AL_ECX[2] = {0x8a, 0x01};
static const char ADD_AL[1] = {0x04};
static const char MOV_ECX_AL[2] = {0x88, 0x01};

//MOV
static const char ADD_ECX[2] = {0x81, 0xC1};

//SYSCALL
static const char MOV_EAX[1] = {0xb8};
static const char MOV_EBX[1] = {0xbb};
static const char MOV_EDX[1] = {0xba};
static const char INT_80[2] = {0xcd, 0x80};

//I don't remember what this does sorry...
static const char EXIT[9] = {0xb3, 0x2a,
	           0x66, 0x31, 0xc0,
						 0x66, 0x40,
	           0xcd, 0x80};

// LOOP
static const char JMP[1] = {0xe9};
static const char CMP_AL_ZERO[2] = {0x3C, 0x00};
static const char JZ[2] = {0x0f, 0x84};

#define WRITE_OPCODE(op) memcpy(mc+size, op, sizeof(op)); size += sizeof(op)
#define WRITE_OPERAND(op) *((int *) (mc+size)) = op; size += 4 //was unsigned
char* gen_mc(const bfc_ir* ir, size_t irsz, size_t* prsz)
{
	int loop_start[256];
	size_t current_loop = 0;
	size_t capacity = irsz * 8 + 32;
	size_t size=0;//MOV EBX / CALL EXIT
	char* mc = malloc(capacity);
	WRITE_OPCODE(MOV_ECX);
	WRITE_OPERAND(0);//reserve space

	//Prepare for syscalls
	WRITE_OPCODE(MOV_EBX);
	WRITE_OPERAND(1);
	WRITE_OPCODE(MOV_EDX);
	WRITE_OPERAND(1);

	for(size_t i =0; i < irsz; i++){
		switch(ir[i].op){
			case ADD: 
				WRITE_OPCODE(MOV_AL_ECX);
				WRITE_OPCODE(ADD_AL);
				mc[size] = ir[i].mag;
				size++;
				WRITE_OPCODE(MOV_ECX_AL);
				break;

			case MOV: 
				WRITE_OPCODE(ADD_ECX);
				WRITE_OPERAND(ir[i].mag);
				break;

			case IN: size += 0;
				WRITE_OPCODE(MOV_EAX);
				WRITE_OPERAND(0x03);
				WRITE_OPCODE(INT_80);
				break;//MOV INC MOV

			case OUT: size += 0; 
				WRITE_OPCODE(MOV_EAX);
				WRITE_OPERAND(0x04);
				WRITE_OPCODE(INT_80);
				break;//MOV INC

			case LOOP_L:
				current_loop++;
				loop_start[current_loop] = size;
				WRITE_OPCODE(MOV_AL_ECX);
				WRITE_OPCODE(CMP_AL_ZERO);
				WRITE_OPCODE(JZ);
				WRITE_OPERAND(0); //make space for jump
				break;//MOV CMP JZ

			case LOOP_R:
				WRITE_OPCODE(JMP);
				WRITE_OPERAND(loop_start[current_loop] - (size + /*sizeof(JMP) +*/ 4));
				int off = sizeof(MOV_AL_ECX) + sizeof(CMP_AL_ZERO) ; 
				*((int *) (mc + loop_start[current_loop] +off +sizeof(JZ))) =  size - (loop_start[current_loop]+off+sizeof(JZ)+4); //old
				current_loop--;
				break;//JMP
			default: break;
		}
		if(size + 8 >= capacity){
			capacity *= 2;
			mc = realloc(mc, capacity);
		}
	}
	/*
	capacity += 16;
	mc = realloc(mc, capacity);
	*/

	WRITE_OPCODE(EXIT);

	*prsz = size;
	size += 0x08048000 + sizeof(Elf32_Ehdr) + sizeof(Elf32_Phdr);
	*((unsigned int*) (mc + sizeof(MOV_ECX))) = (unsigned int) size;
	//memcpy(mc+sizeof(MOV_ECX), &size, 4);
	return mc;
}

void write_elf(const char* prog, size_t prsz, const char* out_path)
{
	size_t origin = 0x08048000;
	size_t ehsz = sizeof(Elf32_Ehdr), phsz = sizeof(Elf32_Phdr);
  printf("entry %x\n", origin+ehsz+phsz);
	FILE* fp = fopen(out_path, "wb");
	if(!fp){
		puts("Can't open output");
	}
  
	Elf32_Ehdr ehdr = {
		.e_ident = {0x7F, 'E','L','F', ELFCLASS32, ELFDATA2LSB, EV_CURRENT, ELFOSABI_LINUX, 0},
		.e_type = ET_EXEC,
		.e_machine = EM_386,
		.e_version = EV_CURRENT,
		.e_entry = origin + ehsz + phsz,
		.e_phoff = ehsz,
		//.e_shoff = 0,
		//.e_flags = 0,
		.e_ehsize = ehsz,
		.e_phentsize = phsz,
		.e_phnum = 1,
		//.e_shentsize = 0,
		//.e_shnum = 0,
		.e_shstrndx = SHN_UNDEF
	};

	Elf32_Phdr phdr = {
		.p_type = PT_LOAD,
		//.p_offset = 0,
		.p_vaddr = origin,
		.p_paddr = origin,
		//.p_vaddr = 0, /*must be 0 for BSD compat*/
		.p_filesz = ehsz + phsz + prsz,
		.p_memsz = ehsz + phsz + prsz + 30000,
		.p_flags = PF_X | PF_W | PF_R,
		.p_align = 0x1000
	};

	void * vp = &ehdr;
	fwrite(vp, ehsz, 1, fp);
	vp = &phdr;
	fwrite(vp, phsz, 1, fp);
	fwrite(prog, prsz, 1, fp);
	fclose(fp);
}
