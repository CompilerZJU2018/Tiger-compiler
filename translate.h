#ifndef _TRRANSLATE_H
#define _TRRANSLATE_H

#include "frame.h"
#include "tree.h"
#include "absyn.h"

typedef struct Tr_exp_ *Tr_exp;
typedef struct Tr_expList_ *Tr_expList;

typedef struct Tr_access_ *Tr_access;
typedef struct Tr_level_  *Tr_level;
typedef struct Tr_accessList_ *Tr_accessList;
struct Tr_access_ {
    Tr_level level;
    F_access access;
};
struct Tr_level_ {
    Tr_level parent;
	Temp_label name;
	F_frame frame;
	Tr_accessList formals;
};
struct Tr_accessList_ {
    Tr_access head;
    Tr_accessList tail;
};

typedef struct patchList_ *patchList;
struct patchList_ {
    Temp_label *head;
    patchList tail;
};
static patchList PatchList(Temp_label *head, patchList tail);
static void doPatch(patchList tList, Temp_label label);
static patchList joinPatch(patchList first, patchList second);
static T_expList trans_expList(Tr_expList el);
static T_exp Tr_static_link(Tr_level level, Tr_level fun_level);

struct Cx {
    patchList trues;    //true condition
    patchList falses;   //false condition
    T_stm stm;
};

struct Tr_exp_ {
    enum {Tr_ex, Tr_nx, Tr_cx} kind;
    union{
        T_exp ex;       //expression
        T_stm nx;       //non-result statement
        struct Cx cx;   //condition statement
    } u;
};

struct Tr_expList_ {
    Tr_exp head;
    Tr_expList tail;
};
Tr_expList Tr_ExpList(Tr_exp head, Tr_expList tail);
//void Tr_expList_append(Tr_exp head, Tr_expList *tail);

static Tr_exp Tr_Ex(T_exp ex);
static Tr_exp Tr_Nx(T_stm nx);
static Tr_exp Tr_Cx(patchList trues, patchList falses, T_stm stm);

static T_exp unEx(Tr_exp e);
static T_stm unNx(Tr_exp e);
static struct Cx unCx(Tr_exp e);

Tr_accessList Tr_AccessList(Tr_access head, Tr_accessList tail);
Tr_level Tr_outermost(void);
Tr_level Tr_newLevel(Tr_level parent, Temp_label name, U_boolList formals);
Tr_accessList Tr_formals(Tr_level level);
Tr_access Tr_allocLocal(Tr_level level, bool escape);

Tr_exp Tr_simpleVar(Tr_access access, Tr_level level);
Tr_exp Tr_fieldVar(Tr_exp e, int k);
Tr_exp Tr_subscriptVar(Tr_exp e1, Tr_exp e2);
Tr_exp Tr_intExp(int i);
Tr_exp Tr_stringExp(string s); // mark
Tr_exp Tr_noExp();
Tr_exp Tr_nilExp();
Tr_exp Tr_recordExp(int i, Tr_expList el);
Tr_exp Tr_arrayExp(Tr_exp size, Tr_exp init);
Tr_exp Tr_seqExp(Tr_expList el);
Tr_exp Tr_callExp(Temp_label fun, Tr_expList el, Tr_level level, Tr_level fun_level);
Tr_exp Tr_doneExp();
Tr_exp Tr_ifExp(Tr_exp test, Tr_exp then, Tr_exp elsee);
Tr_exp Tr_forExp(Tr_exp lower_limit, Tr_exp upper_limit, Tr_exp loop, Tr_exp end, Tr_exp i);
Tr_exp Tr_whileExp(Tr_exp test, Tr_exp body, Tr_exp done);
Tr_exp Tr_breakExp(Tr_exp e);
Tr_exp Tr_assignExp(Tr_exp lvalue, Tr_exp e);
Tr_exp Tr_eqExp(A_oper op, Tr_exp left, Tr_exp right);
Tr_exp Tr_eqStringExp(A_oper op, Tr_exp left, Tr_exp right);
Tr_exp Tr_eqRef(A_oper op, Tr_exp left, Tr_exp right);
Tr_exp Tr_arithExp(A_oper op, Tr_exp left, Tr_exp right);
Tr_exp Tr_relExp(A_oper op, Tr_exp left, Tr_exp right);

void Tr_procEntryExit(Tr_level level, Tr_exp body, Tr_accessList formals);
F_fragList Tr_getResult();


#endif