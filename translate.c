#include <stdio.h>
#include <stdlib.h>
#include "util.h"
#include "absyn.h"
#include "frame.h"
#include "translate.h"

Tr_expList Tr_ExpList(Tr_exp head, Tr_expList tail)
{
	Tr_expList el = checked_malloc(sizeof(*el));
	el->head = head;
	el->tail = tail;
	return el;
}

static patchList PatchList(Temp_label *head, patchList tail)
{
    patchList p = (patchList)checked_malloc(sizeof(*p));
	p->head = head;
	p->tail = tail;
	return p;
}

static void doPatch(patchList tList, Temp_label label) //fill the label in every head of patchlist's every node
{
    for(; tList; tList = tList->tail)
        *(tList->head) = label;
}

static patchList joinPatch(patchList first, patchList second)
{
    if(!first)
        return second;
    for(; first->tail ; first = first->tail);
    first->tail = second;
    return first;
}

static T_expList Tr_trans_expList(Tr_expList el)
{
    T_expList head = NULL;
    T_expList temp = NULL;
	for (; el; el = el->tail) {
		if (head) {
			temp->tail = T_ExpList(unEx(el->head), NULL);
			temp = temp->tail;
		}
        else {
			head = T_ExpList(unEx(el->head), NULL);
			temp = head;
		}
	}
	return head;
}
static T_exp Tr_static_link(Tr_level level, Tr_level fun_level)
{
    T_exp framelink = T_Temp(F_FP());
	if (level == fun_level) {
		return framelink;
	}
	while (level != fun_level) {
		assert(level);
		F_access ac = F_formals(level->frame)->head;
		framelink = F_Exp(ac, framelink);
		level = level->parent;
	}
	return framelink;
}

static Tr_exp Tr_Ex(T_exp ex)
{
    Tr_exp e = (Tr_exp)checked_malloc(sizeof(*e));
	e->kind = Tr_ex;
	e->u.ex = ex;
	return e;
}

static Tr_exp Tr_Nx(T_stm nx)
{
    Tr_exp e = (Tr_exp)checked_malloc(sizeof(*e));
	e->kind = Tr_nx;
	e->u.nx = nx;
	return e;
}

static Tr_exp Tr_Cx(patchList trues, patchList falses, T_stm stm)
{
    Tr_exp e = (Tr_exp)checked_malloc(sizeof(*e));
	e->kind = Tr_cx;
	e->u.cx.trues  = trues;
	e->u.cx.falses = falses;
	e->u.cx.stm = stm;
	return e;
}

static T_exp unEx(Tr_exp e) //change to ex
{
    assert(e);
    switch(e->kind){
        case Tr_ex:
            return e->u.ex;
        case Tr_cx:{
            Temp_temp r = Temp_newtemp();
            Temp_label t = Temp_newlabel(), f = Temp_newlabel();
            doPatch(e->u.cx.trues, t);
            doPatch(e->u.cx.falses, f);
            return T_Eseq(T_Move(T_Temp(r), T_Const(1)),
                        T_Eseq(e->u.cx.stm,
                            T_Eseq(T_Label(f),
                                T_Eseq(T_Move(T_Temp(r), T_Const(0)),
                                    T_Eseq(T_Label(t), T_Temp(r))))));
        }
        case Tr_nx:
            return T_Eseq(e->u.nx, T_Const(0));
    }
    assert(0);
}

static T_stm unNx(Tr_exp e) //change to nx
{
    assert(e);
    switch(e->kind){
        case Tr_ex:
            return T_Exp(e->u.ex);
        case Tr_nx:
            return e->u.nx;
        case Tr_cx:{
            Temp_temp r = Temp_newtemp();
		    Temp_label t = Temp_newlabel(), f = Temp_newlabel();
            doPatch(e->u.cx.trues, t);
            doPatch(e->u.cx.falses, f);
            return T_Exp(T_Eseq(T_Move(T_Temp(r), T_Const(1)),
                        T_Eseq(e->u.cx.stm,
                            T_Eseq(T_Label(f),
                                T_Eseq(T_Move(T_Temp(r), T_Const(0)),
                                    T_Eseq(T_Label(t), T_Temp(r)))))));
        }
    }
    assert(0);
}

static struct Cx unCx(Tr_exp e) //change to cx
{  
    assert(e);
    switch(e->kind){
        case Tr_ex:{
            struct Cx cx;
		    cx.stm = T_Cjump(T_eq, e->u.ex, T_Const(1), NULL, NULL);
		    cx.trues = PatchList(&(cx.stm->u.CJUMP.true), NULL);
		    cx.falses = PatchList(&(cx.stm->u.CJUMP.false), NULL);
		    return cx;
        }
        //case Tr_nx:
        case Tr_cx:
            return e->u.cx;
        default: /* no kind */ break;
    }
    assert(0);
}

Tr_accessList Tr_AccessList(Tr_access head, Tr_accessList tail)
{
    Tr_accessList a = (Tr_accessList)checked_malloc(sizeof(*a));
    a->head = head;
    a->tail = tail;
    return a;
}

static Tr_level outerlevel;
static bool outerlevelinit = FALSE;
Tr_level Tr_outermost(void)
{
    if(outerlevelinit == FALSE){
        outerlevel = Tr_newLevel(NULL, Temp_newlabel(), NULL);
        outerlevelinit = TRUE;
    }
    return outerlevel;
}

Tr_level Tr_newLevel(Tr_level parent, Temp_label name, U_boolList f)
{
    Tr_level level = (Tr_level)checked_malloc(sizeof(*level));
    level->parent = parent;
    level->name = name;
    level->frame = F_newFrame(name, U_BoolList(TRUE, f));
    
    F_accessList al = NULL;
    Tr_accessList formals = NULL;
    Tr_accessList temp_formals = NULL;

    al = F_formals(level->frame);
    for(; al; al = al-> tail){
        Tr_access ac = (Tr_access)checked_malloc(sizeof(*ac));
        ac->level = level;
        ac->access = al->head;
        Tr_accessList formals_end = Tr_AccessList(ac, NULL);

        if(formals == NULL){
            formals = formals_end;
            temp_formals = formals;   
        }
        else{
            temp_formals->tail = formals_end;
            temp_formals = temp_formals->tail;
        }
    }
    level->formals = formals;
    return level;
}

Tr_accessList Tr_formals(Tr_level level)
{
    return level->formals;
}

Tr_access Tr_allocLocal(Tr_level level, bool escape)
{
    Tr_access ac = (Tr_access)checked_malloc(sizeof(*ac));
    ac->access = F_allocaLocal(level->frame, escape);
    ac->level = level;
    return ac;
}

Tr_exp Tr_simpleVar(Tr_access ac, Tr_level level)
{
    assert(ac && level);
    return Tr_Ex(F_Exp(ac->access, Tr_static_link(level, ac->level)));
}

Tr_exp Tr_fieldVar(Tr_exp e, int k) //k is the offsets.
{
    assert(e);
    return Tr_Ex(T_Mem(T_Binop(T_plus, unEx(e), T_Const(k * F_WORD_SIZE))));
}

Tr_exp Tr_subscriptVar(Tr_exp e1, Tr_exp e2) //e1 is the base expression, e2 is used to calculate k
{
    assert(e1 && e2);
    return Tr_Ex(T_Mem(T_Binop(T_plus, unEx(e1), T_Binop(T_mul, unEx(e2), T_Const(F_WORD_SIZE)))));
}

Tr_exp Tr_intExp(int i)
{
    return Tr_Ex(T_Const(i));
}

static F_fragList stringFragList = NULL;
Tr_exp Tr_stringExp(string s)
{
    Temp_label label = Temp_newlabel();
    F_frag frag = F_StringFrag(label, s);
    stringFragList = F_FragList(frag, stringFragList);

    return Tr_Ex(T_Name(label));
}

Tr_exp Tr_noExp()
{
    return Tr_Ex(T_Const(0));
}

// static Tr_exp temp_nil = NULL;
// Tr_exp Tr_nilExp()
// {
//     if(!temp_nil)
//         temp_nil = Tr_Ex(T_Mem(T_Const(0)));
//     return temp_nil;
// }
static Temp_temp temp_nil = NULL;
Tr_exp Tr_nilExp()
{
    if(!temp_nil){
        temp_nil = Temp_newtemp();
         return Tr_Ex(T_Eseq(T_Move(T_Temp(temp_nil), 
                            F_externalCall("nil init", T_ExpList(T_Const(0 * F_WORD_SIZE), NULL))), T_Temp(temp_nil)));
    }
    else
        return Tr_Ex(T_Temp(temp_nil));
}

Tr_exp Tr_recordExp(int i, Tr_expList el)
{
    assert(i && el);
    Temp_temp temp_record = Temp_newtemp();
    T_stm init = T_Move(T_Temp(temp_record), 
                            F_externalCall("Record init", T_ExpList(T_Const(i * F_WORD_SIZE), NULL)));
    unsigned int record_count = i - 1;
    T_stm record = T_Move(T_Mem(T_Binop(T_plus, T_Temp(temp_record), T_Const((record_count--) * F_WORD_SIZE))), unEx(el->head));
    el = el->tail;
    for(; el; el = el->tail)
        record = T_Seq(T_Move(T_Mem(T_Binop(T_plus, T_Temp(temp_record), T_Const((record_count--) * F_WORD_SIZE))), unEx(el->head)), record);
    return Tr_Ex(T_Eseq(T_Seq(init, record), T_Temp(temp_record)));
}

Tr_exp Tr_arrayExp(Tr_exp size, Tr_exp init)
{
    assert(size && init);
    return Tr_Ex(F_externalCall("Array init", T_ExpList(unEx(size), T_ExpList(unEx(init), NULL))));
}

Tr_exp Tr_seqExp(Tr_expList el)
{
    assert(el);
    T_exp result = unEx(el->head);
    el = el->tail;
    for(; el; el = el->tail)
        result = T_Eseq(T_Exp(unEx(el->head)), result);
    return Tr_Ex(result);
}

Tr_exp Tr_callExp(Temp_label fun, Tr_expList el, Tr_level fun_level, Tr_level level)
{
    assert(fun);
    T_expList args = Tr_trans_expList(el);	
	args = T_ExpList(Tr_static_link(level, fun_level->parent), args);

	return Tr_Ex(T_Call(T_Name(fun), args));
}

Tr_exp Tr_doneExp()
{
    return Tr_Ex(T_Name(Temp_newlabel()));
}

Tr_exp Tr_ifExp(Tr_exp test, Tr_exp then, Tr_exp elsee)
{
    assert(test && then);
    Temp_label t = Temp_newlabel();
    Temp_label f = Temp_newlabel();
    Temp_label end = Temp_newlabel();
	
    //construct the cjump
    struct Cx cond = unCx(test);
	doPatch(cond.trues, t);
	doPatch(cond.falses, f);

    T_stm stm_then;
    switch(then->kind){
            case Tr_ex: stm_then = T_Exp(then->u.ex); break;
            case Tr_nx: stm_then = then->u.nx; break;
            case Tr_cx: stm_then = then->u.cx.stm; break;
    }
	if (elsee != NULL) {    //there is else
        T_stm Jumptoend = T_Jump(T_Name(end), Temp_LabelList(end, NULL));
        T_stm stm_elsee;
        switch(elsee->kind){
            case Tr_ex: stm_elsee = T_Exp(elsee->u.ex); break;
            case Tr_nx: stm_elsee = elsee->u.nx; break;
            case Tr_cx: stm_elsee = elsee->u.cx.stm; break;
        }
        if (then->kind != Tr_nx && elsee->kind != Tr_nx){ //then's kind and else's kind are both Tr_nx, which means there is more than one conditoin
            Temp_temp r = Temp_newtemp();
            return Tr_Ex(T_Eseq(cond.stm,
                            T_Eseq(T_Label(t),
                                T_Eseq(T_Move(T_Temp(r), unEx(then)),
                                    T_Eseq(Jumptoend,
                                        T_Eseq(T_Label(f),
                                            T_Eseq(T_Move(T_Temp(r), unEx(elsee)),
                                                T_Eseq(Jumptoend,
                                                    T_Eseq(T_Label(end), T_Temp(r))))))))));
        }
		return Tr_Nx(T_Seq(cond.stm,
					    T_Seq(T_Label(t),
							T_Seq(stm_then,
								T_Seq(Jumptoend,
									T_Seq(T_Label(f),
							            T_Seq(stm_elsee,
											T_Seq(Jumptoend, T_Label(end)))))))));
	}
	else //there is no else
		return Tr_Nx(T_Seq(cond.stm, T_Seq(T_Label(t), T_Seq(stm_then, T_Label(f)))));
}

Tr_exp Tr_forExp(Tr_exp lower_limit, Tr_exp upper_limit, Tr_exp loop, Tr_exp after_loop, Tr_exp i)
{
    Temp_label start_loop = Temp_newlabel();
    Temp_label next_loop = Temp_newlabel();
    Temp_label end = unEx(after_loop)->u.NAME;

    T_stm cjump = T_Cjump(T_le, unEx(i), unEx(upper_limit), start_loop, end);
    return Tr_Ex(T_Eseq(T_Move(unEx(i), unEx(lower_limit)), 
                    T_Eseq(cjump,
                        T_Eseq(T_Label(start_loop),
                            T_Eseq(unNx(loop),
                                T_Eseq(T_Move(unEx(i), T_Binop(T_plus, unEx(i), T_Const(1))),
                                    T_Eseq(T_Label(next_loop),
                                        T_Eseq(cjump, 
                                            T_Eseq(T_Label(end), T_Const(0))))))))));
}

Tr_exp Tr_whileExp(Tr_exp test, Tr_exp body, Tr_exp done)
{
    assert(test && body);
    Temp_label body_label = Temp_newlabel();
    Temp_label test_label = Temp_newlabel();
    return Tr_Ex(T_Eseq(T_Label(test_label),
                    T_Eseq(T_Cjump(T_eq, unEx(test), T_Const(1), body_label, test_label),
                        T_Eseq(T_Label(body_label),
                            T_Eseq(unNx(body),
                                T_Eseq(T_Jump(T_Name(test_label), Temp_LabelList(test_label, NULL)),
                                    T_Eseq(T_Label(unEx(done)->u.NAME), T_Const(0))))))));

}

Tr_exp Tr_breakExp(Tr_exp e)
{
    assert(e);
    return Tr_Nx(T_Jump(T_Name(unEx(e)->u.NAME), 
                    Temp_LabelList(unEx(e)->u.NAME, NULL)));
}

Tr_exp Tr_assignExp(Tr_exp lvalue, Tr_exp e)
{
    return Tr_Nx(T_Move(unEx(lvalue), unEx(e)));
}

Tr_exp Tr_eqExp(A_oper op, Tr_exp left, Tr_exp right)
{
    T_relOp oper;
    if(op == A_eqOp)
        oper = T_eq;
    else
        oper = T_ne;
    T_stm cond = T_Cjump(oper, unEx(left), unEx(right), NULL, NULL);
    patchList trues = PatchList(&cond->u.CJUMP.true, NULL);
    patchList falses = PatchList(&cond->u.CJUMP.false, NULL);

    return Tr_Cx(trues, falses, cond);
}

Tr_exp Tr_eqStringExp(A_oper op, Tr_exp left, Tr_exp right)
{
    T_exp result = F_externalCall("String Compare", T_ExpList(unEx(left), T_ExpList(unEx(right), NULL)));
    if(op == A_eqOp){
        return Tr_Ex(result);
    }
    else{
        if(result->kind == T_CONST && result->u.CONST != 0)
            return Tr_Ex(T_Const(0));
        else
            return Tr_Ex(T_Const(1));
    }
}

Tr_exp Tr_eqRef(A_oper op, Tr_exp left, Tr_exp right)
{
    assert(left && right);
    T_relOp oper;
    if(op == A_eqOp)
        oper = T_eq;
    else
        oper = T_ne;
    T_stm cond = T_Cjump(oper, unEx(left), unEx(right), NULL, NULL);
    patchList trues = PatchList(&cond->u.CJUMP.true, NULL);
    patchList falses = PatchList(&cond->u.CJUMP.false, NULL);

    return Tr_Cx(trues, falses, cond);
}

Tr_exp Tr_arithExp(A_oper op, Tr_exp left, Tr_exp right)
{
    assert(left && right);
    T_binOp oper;
    switch(op){
        case A_plusOp: oper = T_plus; break;
        case A_minusOp: oper = T_minus; break;
        case A_timesOp: oper = T_mul; break;
        case A_divideOp: oper = T_div; break;
        default: /* no operation */ break;
    }
    return Tr_Ex(T_Binop(oper, unEx(left), unEx(right)));
}

Tr_exp Tr_relExp(A_oper op, Tr_exp left, Tr_exp right)
{
    assert(left && right);
    T_relOp oper;
    switch(op){
        case A_ltOp: oper = T_lt; break;
        case A_leOp: oper = T_le; break;
        case A_gtOp: oper = T_gt; break;
        case A_geOp: oper = T_ge; break;
        default: /* no operation */ break;
    }
    T_stm cond = T_Cjump(oper, unEx(left), unEx(right), NULL, NULL);
    patchList trues = PatchList(&cond->u.CJUMP.true, NULL);
    patchList falses = PatchList(&cond->u.CJUMP.false, NULL);

    return Tr_Cx(trues, falses, cond);
}

static F_fragList procFragList = NULL;
void Tr_procEntryExit(Tr_level level, Tr_exp body, Tr_accessList formals)
{
    F_frag procfrag = F_ProcFrag(unNx(body), level->frame);
    procFragList = F_FragList(procfrag, procFragList);
}

F_fragList Tr_getResult()
{
    F_fragList current = NULL;
    F_fragList prev = NULL;
	for(current = stringFragList; current; current = current->tail)
		prev = current;
	if(prev)
		prev->tail = procFragList;
	return stringFragList ? stringFragList : procFragList;
}
