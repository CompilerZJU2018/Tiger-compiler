#ifndef _FRAME_H
#define _FRAME_H

#include <stdio.h>
#include "util.h"
#include "temp.h"
#include "tree.h"


typedef struct F_frame_ *F_frame;
typedef struct F_access_ *F_access;

typedef struct F_accessList_ *F_accessList;
struct F_accessList_ {F_access head; F_accessList tail;};
F_accessList F_AccessList(F_access head, F_accessList tail);


F_frame F_newFrame(Temp_label name, U_boolList formals);

Temp_label F_name(F_frame f);
F_accessList F_formals(F_frame f);
F_access F_allocaLocal(F_frame f, bool escape);

typedef struct F_frag_ *F_frag;
struct F_frag_ {
    enum {F_stringFrag, F_procFrag} kind;
    union{
        struct {Temp_label label; string str;} stringg;
        struct {T_stm body; F_frame frame;} proc;
    }u;
};

F_frag F_StringFrag(Temp_label label, string str);
F_frag F_ProcFrag(T_stm body, F_frame frame);

typedef struct F_fragList_ *F_fragList;
struct F_fragList_ {F_frag head; F_fragList tail;};
F_fragList F_FragList(F_frag head, F_fragList tail);

extern const int F_WORD_SIZE;
Temp_temp F_FP(void);
T_exp F_Exp(F_access acc, T_exp framePtr);
Temp_temp F_RV(void);

T_exp F_externalCall(string s, T_expList args);

#endif