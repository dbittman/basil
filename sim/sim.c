#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <libgen.h>
#include <unistd.h>
#include <signal.h>

#define STACK_SIZE 4096

#define GET_PC8(x) (x/2)
#define GET_PC4(x) (x%2)

uint32_t *stack, *sp;
uint8_t *program, *max;
uint8_t *pc_8;
int pc_4;
int counter=0;

int verbose = 0;
int very_verbose = 0;
int little_endian = 1;

struct opcode {
	char *name;
	void (*fn)(uint8_t);
};

void execute(int code);

void sigint(int s)
{
	fprintf(stderr, "\n\ninterrupted (SIGINT): pc = %d\n", counter);
	exit(0);
}

void underflow()
{
	fprintf(stderr, "underflow: pc = %d\n", counter);
	exit(2);
}

void overflow()
{
	fprintf(stderr, "overflow: pc = %d\n", counter);
	exit(2);
}

uint32_t pop()
{
	if(sp == stack) underflow();
	uint32_t x = *(--sp);
	return x;
}

void push(uint32_t p)
{
	if((sp - stack) == STACK_SIZE) overflow();
	*(sp++) = p;
}

uint32_t peek()
{
	if(sp == stack) underflow();
	return *(sp-1);
}

void op_push(uint8_t p)
{
	push(p);
}

void op_and(uint8_t p)
{
	uint32_t a = pop();
	uint32_t b = pop();
	push(a & b);
}

void op_not(uint8_t p)
{
	uint32_t a = pop();
	push(~a);
}

void op_or(uint8_t p)
{
	uint32_t a = pop();
	uint32_t b = pop();
	push(a | b);
}

void op_add(uint8_t p)
{
	int32_t a = pop();
	int32_t b = pop();
	push(a + b);
}

void op_mul(uint8_t p)
{
	int32_t a = pop();
	int32_t b = pop();
	push(a * b);
}

void op_div(uint8_t p)
{
	int32_t a = pop();
	int32_t b = pop();
	push(a / b);
}

void op_cmp(uint8_t p)
{
	push(pop() == pop());
}

void op_swap(uint8_t p)
{
	uint32_t param = pop();
	uint32_t elem = peek();
	
	if(param > 0) {
		if((sp - (param + 1)) < stack)
			underflow();
		*(sp-1) = *(sp - (param + 1));
		*(sp - (param + 1)) = elem;
	} else {
		/* BASIL 2.0: swap 0 executes the top of the stack */
		elem = pop();
		execute(elem & 0xF);
	}
}

void op_ppc(uint8_t p)
{
	push(counter);
}

void op_und(uint8_t p)
{
	fprintf(stderr, "undefined opcode: pc = %d\n", counter);
	exit(2);
}

void op_pop(uint8_t p)
{
	pop();
}

void op_dup(uint8_t p)
{
	uint32_t elem = peek();
	push(elem);
}

void op_get(uint8_t p)
{
	int in = getc(stdin);
	if(in != EOF) {
		push((uint32_t)in);
	}
	else {
		fprintf(stderr, "recieved EOF\n");
		for(;;);
	}
}

void op_put(uint8_t p)
{
	int x = pop();
	putc(x, stdout);
}

void op_br(uint8_t p)
{
	uint32_t test = pop();
	uint32_t x = pop();
	if(test)
		counter = x;
}

struct opcode optable[16] = {
	{"push", op_push},
	{"and",  op_and},
	{"not",  op_not},
	{"or",   op_or},
	{"mul",  op_mul},
	{"div",  op_div},
	{"add",  op_add},
	{"cmp",  op_cmp},
	{"pop",  op_pop},
	{"swap", op_swap},
	{"dup",  op_dup},
	{"ppc",  op_ppc},
	{"get",  op_get},
	{"put",  op_put},
	{"br",   op_br},
	{"und",  op_und},
};

void execute(int code)
{
	static int waiting_for_push_operand = 0;
	//fprintf(stderr, "execute (pc=%d, %d): %x\n", counter, waiting_for_push_operand, code);
	if(waiting_for_push_operand)
	{
		waiting_for_push_operand = 0;
		if(verbose)
			fprintf(stderr, "push %d\n", code);
		optable[0].fn(code);
	} else {
		
		if(!code)
			waiting_for_push_operand = 1;
		else {
			if(verbose)
				fprintf(stderr, "%s\n", optable[code].name);
			optable[code].fn(0);
		}
	}
}

void usage()
{
	fprintf(stderr, "usage: basilsim [-vVb] file\n");
	fprintf(stderr, "    -v: verbose (print assembly)\n");
	fprintf(stderr, "    -V: very verbose (print hex codes)\n");
	fprintf(stderr, "    -b: Big Endian\n");
	fprintf(stderr, "file is a properly formatted basil executable file\n");
	exit(0);
}

int main(int argc, char **argv)
{
	int c;
	opterr = 0;
	while((c = getopt(argc, argv, "vVbh")) != EOF)
	{
		switch(c) {
			case 'V':
				very_verbose = 1;
				break;
			case 'v':
				verbose = 1;
				break;
			case 'b':
				little_endian = 0;
				break;
			case 'h': case '?':
			default:
				usage();
		}
	}
	
	char *file = argv[optind];
	if(!file) usage();
	
	if(argc < 2) exit(1);
	
	setbuf(stdout, 0);
	signal(SIGINT, sigint);
	
	FILE *f = fopen(file, "r");
	if(!f) exit(1);
	
	struct stat st;
	if(stat(file, &st) == -1)
	{
		perror("stat");
		exit(3);
	}
	
	program = malloc(st.st_size);
	if(!program) {
		perror("malloc");
		exit(3);
	}
	pc_8 = program;
	
	sp = stack = malloc(STACK_SIZE);
	if(!stack) {
		perror("malloc");
		exit(3);
	}
	while((c = fgetc(f)) != EOF)
		*(pc_8++) = (uint8_t)c;
	
	max = pc_8;
	fclose(f);
	
	pc_8 = program;
	pc_4 = 0;
	
	while(pc_8 < max)
	{
		uint8_t oc = ((little_endian ? !pc_4 : pc_4) ? (*(pc_8) >> 4) : (*(pc_8))) & 0xF;
		if(very_verbose)
			printf("%d: %d %d: %x: %x\n", counter, pc_4, (int)(pc_8 - program), oc, *pc_8);
		counter++;
		
		execute(oc);
		
		pc_8 = program + GET_PC8(counter);
		pc_4 = GET_PC4(counter);
	}
	fprintf(stderr, "executing code outside of program space: pc = %d\n", counter);
	return 2;
}
