#include "stdio.h"
#include "util.h"
#include "temp.h"
#include "tree.h"
#include "frame.h"

const int F_WORD_SIZE = 4;
static const int F_MAX_REG = 4;

struct F_access_
    {
        enum {inFrame, inReg} kind;
        union {
            int offset;
            Temp_temp reg;
        }u;
    };

struct F_frame_
    {
	    Temp_label name;
	    F_accessList formals, locals;
	    int lc;

    };

static F_access InFrame(int offset){
    F_access new_access = (F_access)checked_malloc(sizeof(struct F_access_));
    new_access->kind = inFrame;
    new_access->u.offset = offset;
    return new_access;
}
static F_access InReg(Temp_temp reg){
    F_access new_access = (F_access)checked_malloc(sizeof(struct F_access_));
    new_access->kind = inReg;
    new_access->u.reg = reg;
    return new_access;
}

F_accessList F_AccessList(F_access head, F_accessList tail) {
	F_accessList l = checked_malloc(sizeof(*l));
	l->head = head;
	l->tail = tail;
	return l;
}

F_frame F_newFrame(Temp_label name, U_boolList formals){
    F_frame new_frame = (F_frame)checked_malloc(sizeof(struct F_frame_));
    U_boolList formal_p;
    F_accessList head = NULL, tail = NULL;
    int i = 0, j = 0;

    new_frame->name = name;
    for(formal_p = formals; formal_p; formal_p = formal_p->tail) {
        F_access ac = NULL;
        if(i < F_MAX_REG && !(formal_p->head)){
            ac = InReg(Temp_newtemp());
            i++;
        }
        else{
            j++;
            ac = InFrame((j+1)*F_WORD_SIZE);
        }

        if(head){

            tail->tail = F_AccessList(ac, NULL);;
            tail = tail->tail;
        }
        else{

            head = F_AccessList(ac, NULL);;
            tail = head;
        }
    }
    new_frame->formals = head;
    new_frame->locals = NULL;
    new_frame->lc = 0;

    return new_frame;
}



Temp_label F_name(F_frame f){
    return f->name;
}

F_accessList F_formals(F_frame f){
    return f->formals;
}

F_access F_allocaLocal(F_frame f, bool escape){
    f->lc++;

    if(escape){
        return InFrame(-F_WORD_SIZE*(f->lc));
    }
    else{
        return InReg(Temp_newtemp());
    }
}

F_frag F_StringFrag(Temp_label label, string str){
    F_frag frag = (F_frag)checked_malloc(sizeof(struct F_frag_));
    frag->kind = F_stringFrag;
    frag->u.stringg.label = label;
    frag->u.stringg.str = str;

    return frag;
}

F_frag F_ProcFrag(T_stm body, F_frame frame){
    F_frag frag = (F_frag)checked_malloc(sizeof(struct F_frag_));
    frag->kind = F_procFrag;
    frag->u.proc.body = body;
    frag->u.proc.frame = frame;
    return frag;
}

F_fragList F_FragList(F_frag head, F_fragList tail){
    F_fragList fraglist = (F_fragList)checked_malloc(sizeof(struct F_fragList_));
    fraglist->head = head;
    fraglist->tail = tail;

    return fraglist;
}



Temp_temp F_FP(){
    static Temp_temp FP = NULL;
	if (!FP)
	{
		FP = Temp_newtemp();
	}
	return FP;
}
Temp_temp F_RV(void){
    static Temp_temp RV = NULL;
    if(!RV){
        RV = Temp_newtemp();
    }
    return RV;
}

T_exp F_Exp(F_access acc, T_exp framePtr){
    T_exp expr;
    if(acc->kind == inFrame){
        expr = T_Mem(T_Binop(T_plus, T_Const(acc->u.offset), framePtr));
        return expr;
    }
    else{
        expr = T_Temp(acc->u.reg);
        return expr;
    }
}

T_exp F_externalCall(string s, T_expList args){
    T_exp expr;
    expr = T_Call(T_Name(Temp_namedlabel(s)), args);

    return expr;
}
