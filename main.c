#include <stdio.h>
#include <stdlib.h>
#include "util.h"
#include "errormsg.h"
#include "parse.h"
#include "prabsyn.h"
#include "semant.h"
#include "escape.h"
#include "frame.h"
#include "printtree.h"
#include "string.h"

void print_tree(F_fragList fl, string output) {
	FILE *fp; 
	fp = fopen(output, "w");
	for (; fl; fl = fl->tail) {
		F_frag f = fl->head;
		if (f->kind == F_stringFrag)
			printf("string: %s\n", f->u.stringg.str);
		else
			printStmList(fp, T_StmList(f->u.proc.body, NULL));
	}
	fclose(fp);
}

int main(int argc, char **argv) {
	if (argc!=2) {
		fprintf(stderr,"usage: a.out filename\n"); 
		exit(1);
	}
	A_exp abstree = parse(argv[1]);
	string output = strcat(argv[1], ".out");
	Esc_findEscape(abstree);
	print_tree(SEM_transProg(abstree), output);
	return 0;
}
