#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include "as.h"

int byte_instructions=0;

int get_opcode(char *code)
{
	if(!strcasecmp(code, "push") || !strcasecmp(code, "psh")) {
		return 0;
	} else if(!strcasecmp(code, "pop")) {
		return 8;
	} else if(!strcasecmp(code, "dup")) {
		return 10;
	} else if(!strcasecmp(code, "br") || !strcasecmp(code, "branch")) {
		return 14;
	} else if(!strcasecmp(code, "and")) {
		return 1;
	} else if(!strcasecmp(code, "add")) {
		return 6;
	} else if(!strcasecmp(code, "cmp")) {
		return 7;
	} else if(!strcasecmp(code, "swap") || !strcasecmp(code, "swp")) {
		return 9;
	} else if(!strcasecmp(code, "not")) {
		return 2;
	} else if(!strcasecmp(code, "or")) {
		return 3;
	} else if(!strcasecmp(code, "ppc")) {
		return 11;
	} else if(!strcasecmp(code, "mul")) {
		return 4;
	} else if(!strcasecmp(code, "div")) {
		return 5;
	} else if(!strcasecmp(code, "get")) {
		return 12;
	} else if(!strcasecmp(code, "put")) {
		return 13;
	} else
		return -1;
}

uint8_t convert(char *outname, int line, char *tok)
{
	errno=0;
	char *end=0;
	int conv = strtol(tok, &end, 0);
	if((end - tok) != (int)strlen(tok)) {
		conv = get_opcode(tok);
		if(conv == -1)
			fprintf(stderr, "warning: %s:%d: invalid opcode: %s\n", outname, line, tok);
	} else {
		if(conv > 15)
			fprintf(stderr, "warning: %s:%d: immediate value truncated to 4 bits: %d\n", outname, line, conv);
	}
	return (uint8_t)conv;
}

int pass4(char *source, char *output, int little_endian)
{
	FILE *in = fopen(source, "r");
	FILE *out = fopen(output, "w");
	if(!in) {
		fprintf(stderr, "pass4: failed to open input '%s': %s\n", source, strerror(errno));
		return 1;
	}
	if(!out) {
		fprintf(stderr, "pass4: failed to open output '%s': %s\n", output, strerror(errno));
		return 1;
	}
	char buffer[1024];
	int i=0;
	uint8_t bytebuffer = 0;
	int token=0;
	while(fgets(buffer, 1024, in))
	{
		i++;
		if(buffer[0] == '\n') continue;
		char *tmp = strchr(buffer, '\n');
		*tmp = 0;
		
		char *tok = buffer;
		while(tok) {
			tmp = strchr(tok, ' ');
			if(tmp) *(tmp++) = 0;
			uint8_t val = convert(source, i, tok);
			if(byte_instructions) {
				fwrite(&val, sizeof(uint8_t), 1, out);
			} else {
				if(!(token % 2)) bytebuffer = 0;
				bytebuffer |= ((little_endian ? !(token%2) : (token%2)) ? (val << 4) : (val));
				
				if(token % 2)
					fwrite(&bytebuffer, sizeof(uint8_t), 1, out);
			}
			token++;
			tok=tmp;
		}
	}
	fwrite(&bytebuffer, sizeof(uint8_t), 1, out);
	fclose(in);
	fclose(out);
	return 0;
}

struct symbol {
	char name[128];
	int pc;
	struct symbol *next;
} *syms=0;

void add_label(char *name, int pc, FILE *symfile)
{
	struct symbol *s = malloc(sizeof(struct symbol));
	strcpy(s->name, name);
	s->pc = pc;
	struct symbol *o = syms;
	s->next = o;
	syms = s;
	fprintf(symfile, "%s: %d\n", name, pc);
}

int get_label(char *name)
{
	struct symbol *s = syms;
	while(s)
	{
		if(!strcmp(s->name, name))
			return s->pc;
		s=s->next;
	}
	return -1;
}

int pass3(char *source, char *output)
{
	FILE *in = fopen(source, "r");
	FILE *out = fopen(output, "w");
	if(!in) {
		fprintf(stderr, "pass3: failed to open input '%s': %s\n", source, strerror(errno));
		return 1;
	}
	if(!out) {
		fprintf(stderr, "pass3: failed to open output '%s': %s\n", output, strerror(errno));
		return 1;
	}
	
	char buffer[1024];
	int line=0;
	while(fgets(buffer, 1024, in))
	{
		line++;
		if(buffer[0] == '\n') continue;
		char *tmp = strchr(buffer, '\n');
		if(tmp)
			*tmp = 0;
		
		char *tok = buffer;
		while(tok) {
			tmp = strchr(tok, ' ');
			if(tmp) *(tmp++) = 0;
			
			char *col;
			if((col = strchr(tok, ':'))) {
				int digit = *(col+1) - '0';
				*col=0;
				int __pc = get_label(tok);
				uint16_t pc = (uint16_t)__pc;
				if(__pc == -1) {
					fprintf(stderr, "%s:%d: undefined reference to '%s'\n", source, line, tok);
					return 1;
				}
				//printf("LABEL: %s: %d -> %d", tok, digit, pc);
				pc = (pc >> (digit * 4)) & 0xF;
				fprintf(out, "%d", pc);
			} else
				fprintf(out, "%s", tok);
			
			if((tok=tmp))
				fprintf(out, " ");
		}
		fprintf(out, "\n");
	}
	
	fclose(out);
	fclose(in);
	return 0;
}

int pass2(char *source, char *output, char *symfile)
{
	FILE *in = fopen(source, "r");
	FILE *out = fopen(output, "w");
	FILE *sf = fopen(symfile, "w");
	if(!in) {
		fprintf(stderr, "pass2: failed to open input '%s': %s\n", source, strerror(errno));
		return 1;
	}
	if(!out) {
		fprintf(stderr, "pass2: failed to open output '%s': %s\n", output, strerror(errno));
		return 1;
	}
	if(!sf) {
		fprintf(stderr, "pass2: failed to open symbol file '%s': %s\n", symfile, strerror(errno));
		return 1;
	}
	
	char buffer[1024];
	int line=0;
	int pc_counter=0;
	while(fgets(buffer, 1024, in))
	{
		line++;
		
		if(buffer[0] == '\n') continue;
		char *tmp;
		
		if((tmp = strchr(buffer, ':')) && !isdigit(*(tmp+1))) {
			/* label definition */
			*tmp=0;
			add_label(buffer, pc_counter, sf);
		} else {
			/* count number of opcodes */
			int count=0;
			for(int i=0, flag=0;buffer[i];i++) {
				if(!isspace(buffer[i])) flag=1;
				if(flag && buffer[i] == ' ' && !isspace(buffer[i+1])) count++;
			}
			int num_spaces = count;
			pc_counter += (num_spaces+1);
			fputs(buffer, out);
		}
	}
	
	fclose(sf);
	fclose(in);
	fclose(out);
	return 0;
}

int is_empty(const char *s) {
  while (*s != '\0') {
    if (!isspace(*s))
      return 0;
    s++;
  }
  return 1;
}

void push_convert(FILE *out, int value)
{
	uint8_t tmp1 = (value >> 12) & 0xF;
	uint8_t tmp2 = (value >> 8) & 0xF;
	uint8_t tmp3 = (value >> 4) & 0xF;
	uint8_t tmp4 = (value) & 0xF;
	
	fprintf(out, "push %d\npush 8\ndup\nmul\ndup\nmul\nmul\n"
	             "push %d\npush 15\npush 1\nadd\ndup\nmul\nmul\n"
				 "push %d\npush 4\ndup\nmul\nmul\n"
				 "push %d\n"
				 "or\nor\nor\n", tmp1, tmp2, tmp3, tmp4);
}

void push_label(FILE *out, char *label)
{
	fprintf(out, "push %s:3\npush 8\ndup\nmul\ndup\nmul\nmul\n"
	             "push %s:2\npush 15\npush 1\nadd\ndup\nmul\nmul\n"
				 "push %s:1\npush 4\ndup\nmul\nmul\n"
				 "push %s:0\n"
				 "or\nor\nor\n", label, label, label, label);
}

char *find_first_non_whitespace(char *s)
{
	while(s && *s && isspace(*s)) s++;
	return s;
}

int pass1(char *source, char *output)
{
	FILE *in = fopen(source, "r");
	FILE *out = fopen(output, "w");
	if(!in) {
		fprintf(stderr, "pass1: failed to open input '%s': %s\n", source, strerror(errno));
		return 1;
	}
	if(!out) {
		fprintf(stderr, "pass1: failed to open output '%s': %s\n", output, strerror(errno));
		return 1;
	}
	
	char __buffer[1024];
	int line=0;
	char *buffer;
	while(fgets(__buffer, 1024, in))
	{
		buffer = find_first_non_whitespace(__buffer);
		line++;
		char *c = strchr(buffer, ';');
		if(c) *c=0;
		if(!is_empty(buffer)) {
			if(!strncmp(buffer, "psh", 3) || !strncmp(buffer, "push", 4)) {
				char *imm = strchr(buffer, ' ');
				if(!imm) {
					fprintf(stderr, "warning: %s:%d: push requires an immediate operand\n", source, line);
					return 1;
				}
				imm++;
				char *nl = strchr(imm, '\n');
				if(nl) *nl=0;
				/* check if imm is greater than 4 bits */
				char *end=0;
				int conv = strtol(imm, &end, 0);
				//printf("push: %d: %d: %d\n", conv, strlen(imm), (end - imm) != strlen(imm));
				if(conv > 15) {
					push_convert(out, conv);
				} else if((end - imm) != (int)(strlen(imm))) {
					push_label(out, imm);
				} else {
					if(nl) *nl='\n';
					fputs(buffer, out);
				}
				
			} else
				fputs(buffer, out);
		}
	}
	
	fclose(in);
	fclose(out);
	return 0;
}

void usage()
{
	fprintf(stderr, "bas: usage: bas [-k] <source-file> <binary-file>\n");
	fprintf(stderr, "assembles an assembly language program directly into a binary file for BASIL\n");
	fprintf(stderr, "  -k: keep intermediate files\n");
	fprintf(stderr, "  -b: make each instruction 8 bites long\n");
	exit(0);
}

int main(int argc, char **argv)
{
	opterr=0;
	int c;
	int keep=0;
	while((c = getopt(argc, argv, "khb")) != EOF) {
		switch(c) {
			case 'k':
				keep=1;
				break;
			case 'b':
				byte_instructions = 1;
				break;
			case 'h':case '?':
			default:
				usage();
		}
	}
	
	char *infile = argv[optind];
	char *outfile = argv[optind+1];
	if(!infile || !outfile) usage();
	
	char out1[strlen(outfile) + 2];
	strcpy(out1, outfile);
	strcat(out1, ".1");
	
	char out2[strlen(outfile) + 2];
	strcpy(out2,outfile);
	strcat(out2, ".2");
	
	char out3[strlen(outfile) + 2];
	strcpy(out3, outfile);
	strcat(out3, ".3");
	
	char outs[strlen(outfile) + 4];
	strcpy(outs, outfile);
	strcat(outs, ".sym");
	
	if(pass1(infile, out1)) return 1;
	if(pass2(out1, out2, outs)) return 1;
	if(pass3(out2, out3)) return 1;
	if(pass4(out3, outfile, 1)) return 1;
	
	if(!keep) {
		unlink(out1);
		unlink(out2);
		unlink(out3);
		unlink(outs);
	}
	
	return 0;
}
