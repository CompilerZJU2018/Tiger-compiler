#include <stdio.h>
#include <stdlib.h>
#include "errormsg.h"
#include "semant.h"
#include "types.h"
#include "env.h"
#include "translate.h"

struct expty {
	Tr_exp exp;
	Ty_ty  ty;
};

struct expty expTy(Tr_exp exp, Ty_ty ty) {
	struct expty e;
	e.exp = exp;
	e.ty = ty;
	return e;
}

static int break_level = 0;

// 主要函数
static struct expty transExp(Tr_level level, Tr_exp breakk, S_table venv, S_table tenv, A_exp a);
static struct expty transVar(Tr_level level, Tr_exp breakk, S_table venv, S_table tenv, A_var v);
static Tr_exp transDec(Tr_level level, Tr_exp breakk, S_table venv, S_table tenv, A_dec d);
static Ty_ty transTy(S_table tenv, A_ty t);

// 辅助函数
static Ty_ty actual_ty(Ty_ty ty); // ty_name 转化为其他基本类型
static bool tycmp(Ty_ty a, Ty_ty b); // 比较两个类型是否相等

static Ty_tyList makeFormalTyList(S_table tenv, A_fieldList params); // 抽象参数list 转为 tylist
static U_boolList makeFormals(A_fieldList params);

//
static Ty_fieldList transFieldList(S_table tenv, A_fieldList a); 
static Ty_tyList transTyList(S_table tenv, A_fieldList a);
static U_boolList makeFormalList(A_fieldList params);
//

//
struct set_ {
	S_symbol s[1000];
	int pos;
};
typedef struct set_ *set;
static set set_init();
static void set_reset(set s);
static bool set_push(set s, S_symbol x);
static set s;
//

// API
F_fragList SEM_transProg(A_exp exp) {
	S_table venv = E_base_venv();
	S_table tenv = E_base_tenv();
	s = set_init();
	struct expty mainfunc = transExp(Tr_outermost(), NULL, venv, tenv, exp);
	Tr_procEntryExit(Tr_outermost(), mainfunc.exp, NULL);
	return Tr_getResult();
}


// ty_name 转化为其他基本类型
static Ty_ty actual_ty(Ty_ty ty) {
	if(!ty)
		return NULL;
	while (ty && ty->kind == Ty_name)
		ty = ty->u.name.ty;
	return ty;
}

// 比较两个类型是否相等
static bool tycmp(Ty_ty a, Ty_ty b) {
	assert(a && b);
	a = actual_ty(a);
	b = actual_ty(b);
	// 别忘了底层的ty_xxx用的都是一个对象
	if(a == b && a->kind != Ty_nil) return TRUE;
	else {
		// record类型和 nil类型也可以比较的
		if ((a->kind == Ty_record && b->kind == Ty_nil) || (a->kind == Ty_nil && b->kind == Ty_record))
			return TRUE;
		else 
			return FALSE;
	}
}

// 抽象参数list 转为 tylist
static Ty_tyList makeFormalTyList(S_table tenv, A_fieldList params) {
	Ty_tyList heads = NULL;
	Ty_tyList tails = heads;
	A_fieldList f = params;
	for(; f; f = f->tail) {
		Ty_ty t = S_look(tenv, f->head->typ);
		if(!t) {
			EM_error(f->head->pos, "undefined type <%s>", S_name(f->head->typ));
		} else {
			if(heads) {
				tails->tail = Ty_TyList(t, NULL);
				tails = tails->tail;
			} else {
				heads = Ty_TyList(t, NULL);
				tails = heads;
			}
		}
	}
	return heads;
}

static U_boolList makeFormals(A_fieldList params) {
	U_boolList formalsList = NULL, tailList = NULL;
	A_fieldList paramList;
	for (paramList = params; paramList; paramList = paramList->tail) {
		if (formalsList) {
			tailList->tail = U_BoolList(TRUE, NULL);
			tailList = tailList->tail;
		} else {
			formalsList = U_BoolList(TRUE, NULL);
			tailList = formalsList;
		}
	}
	return formalsList;
}

// 解决escape的问题
U_boolList makeFormalList(A_fieldList params) {
	U_boolList head = NULL, tail = NULL;
	while (params) {
		if (head) {
			tail->tail = U_BoolList(params->head->escape, NULL);
			tail = tail->tail;
		} else {
			head = U_BoolList(params->head->escape, NULL);
			tail = head;
		}
		params = params->tail;
	}
	return head;
}

// 抽象field列表转化为ty_fieldlist
Ty_fieldList transFieldList(S_table tenv, A_fieldList a) {
	if (!a)
		return NULL;
	if (!set_push(s, a->head->name)) {
		EM_error(a->head->pos, "redeclaration of '%s'", S_name(a->head->name));
		return transFieldList(tenv, a->tail);
	}
	Ty_ty t = S_look(tenv, a->head->typ);
	if (!t) {
		EM_error(a->head->pos, "undefined type '%s'", S_name(a->head->typ));
		t = Ty_Int();
	}
	return Ty_FieldList(Ty_Field(a->head->name, t), transFieldList(tenv, a->tail));
}

// 在定义中可能出现 abs tree 中 type list 转为 tylist
Ty_tyList transTyList(S_table tenv, A_fieldList a) {
	if (!a)
		return NULL;
	if (!set_push(s, a->head->name)) {
		EM_error(a->head->pos, "redeclaration of '%s'", S_name(a->head->name));
		return transTyList(tenv, a->tail);
	}
	Ty_ty t = S_look(tenv, a->head->typ);
	if (!t) {
		EM_error(a->head->pos, "undefined type '%s'", S_name(a->head->typ));
		t = Ty_Int();
	}
	return Ty_TyList(t, transTyList(tenv, a->tail));
}

// set function
set set_init() {
	set t = checked_malloc(sizeof(*t));
	t->pos = 0;
	return t;
}

void set_reset(set s) {
	s->pos = 0;
}

bool set_push(set s, S_symbol x) {
	int i;
	for (i = 0; i < s->pos; i++)
		if (s->s[i] == x)
			return FALSE;
	s->s[s->pos++] = x;
	return TRUE;
}
//

struct expty transVar(Tr_level level, Tr_exp breakk, S_table venv, S_table tenv, A_var v) {
	switch(v->kind) {
		case A_simpleVar: {
			E_enventry simple_entry = S_look(venv, v->u.simple);
			// 找到了 返回actual type
			if(simple_entry && simple_entry->kind == E_varEntry)
				return expTy(Tr_simpleVar(simple_entry->u.var.access, level), actual_ty(simple_entry->u.var.ty));
			else {
				EM_error(v->pos, "undefined variable <%s>", S_name(v->u.simple));
				return expTy(Tr_noExp(), Ty_Int());
			}
		}
		case A_fieldVar: {
			// record_var.field
			struct expty var = transVar(level, breakk, venv, tenv, v->u.field.var);
			if(var.ty->kind != Ty_record) {
				EM_error(v->u.field.var->pos, "unexpected type: variable not a record type");
				return expTy(Tr_noExp(), Ty_Int());
			} else {
				Ty_fieldList f = var.ty->u.record;
				// 找到对应的field
				int field_num = 0;
				for(; f; f = f->tail, field_num++) {
					if(f->head->name == v->u.field.sym) {
						return expTy(Tr_fieldVar(var.exp, field_num), actual_ty(f->head->ty));
					}
				}
				EM_error(v->pos, "unexpected field %s for record", S_name(v->u.field.sym));
				return expTy(Tr_noExp(), Ty_Int());
			}
		}
		case A_subscriptVar: {
			// a[2]
			struct expty var = transVar(level, breakk, venv, tenv, v->u.subscript.var);
			if(var.ty->kind != Ty_array) {
				EM_error(v->u.subscript.var->pos, "not a array type");
				return expTy(Tr_noExp(), actual_ty(var.ty->u.array));
			} else {
				struct expty exp = transExp(level, breakk, venv, tenv, v->u.subscript.exp);
				if(exp.ty->kind != Ty_int) {
					EM_error(v->u.subscript.exp->pos, "unexpected type: subscript, integer required");
					return expTy(Tr_noExp(), actual_ty(var.ty->u.array));
				}
				else
					return expTy(Tr_subscriptVar(var.exp, Tr_noExp()), actual_ty(var.ty->u.array));
			}
		}
	}
	assert(0);
}


// return: actual type
struct expty transExp(Tr_level level, Tr_exp breakk, S_table venv, S_table tenv, A_exp a) {
	switch(a->kind) {
		case A_varExp:
			return transVar(level, breakk, venv, tenv, a->u.var);
		case A_nilExp:
			return expTy(Tr_nilExp(), Ty_Nil());
		case A_intExp:
			return expTy(Tr_intExp(a->u.intt), Ty_Int());
		case A_stringExp:
			return expTy(Tr_stringExp(a->u.stringg), Ty_String());
		case A_callExp: {
			E_enventry func_entry = S_look(venv, a->u.call.func);
			if(!func_entry) {
				EM_error(a->pos, "undeclared function <%s>", S_name(a->u.call.func));
				return expTy(Tr_noExp(), Ty_Int());
			} else if(func_entry->kind == E_varEntry) {
				// 变量冒充了函数名
				EM_error(a->pos, "<%s> not a function, but variable", S_name(a->u.call.func));
				return expTy(Tr_noExp(), func_entry->u.var.ty);
			}
			// 检查形参与实参
			Ty_tyList parameter_list = func_entry->u.fun.formals; // 形参
			A_expList argument_list = a->u.call.args; // 实参
			Tr_expList h = NULL, t = NULL;
			for(; parameter_list && argument_list; parameter_list = parameter_list->tail, argument_list = argument_list->tail) {
				struct expty single_argument = transExp(level, breakk, venv, tenv, argument_list->head);
				if(!tycmp(parameter_list->head, single_argument.ty)) {
					EM_error(a->pos, "unexpected type: argument mismatch");
					return expTy(Tr_noExp(), func_entry->u.var.ty);
				}
				if (h) {
					t->tail = Tr_ExpList(single_argument.exp, NULL);
					t = t->tail;
				} else {
					h = Tr_ExpList(single_argument.exp, NULL);
					t = h;
				}
			}
			// 实参是不是过多少了
			if(parameter_list)
				EM_error(a->pos, "too few arguments");
			else if(argument_list)
				EM_error(a->pos, "too many arguments");
			else
				return expTy(Tr_callExp(func_entry->u.fun.label, h, func_entry->u.fun.level, level), func_entry->u.fun.result);
			// 表达式的返回值类型是函数返回值类型
			return expTy(Tr_noExp(), func_entry->u.fun.result);
		}
		case A_opExp: {
			A_oper oper = a->u.op.oper;
			struct expty left = transExp(level, breakk, venv, tenv, a->u.op.left);
			struct expty right = transExp(level, breakk, venv, tenv, a->u.op.right);
			switch(oper) {
				case A_plusOp:
				case A_minusOp:
				case A_timesOp:
				case A_divideOp: {
					// 只有整数
					if(left.ty->kind != Ty_int)
						EM_error(a->u.op.left->pos, "unexpected type: integer required");
					else if(right.ty->kind != Ty_int)
						EM_error(a->u.op.right->pos, "unexpected type: integer required");
					else
						return expTy(Tr_arithExp(a->u.op.oper, left.exp, right.exp), Ty_Int());
					return expTy(Tr_noExp(), Ty_Int());
				}
				case A_eqOp:
				case A_neqOp: {
					// 判断两个类型是否一致
					// array 和 record比较的是reference是不是相同
					// if (left.ty->kind == Ty_void)
					// 	EM_error(a->u.op.left->pos, "unexpected type: left had no value");	
					// else if (right.ty->kind == Ty_void) 
					// 	EM_error(a->u.op.right->pos, "unexpected type: right had no value");	
					// else if (!tycmp(left.ty, right.ty))
					// 	EM_error(a->pos, "comparison type mismatch");
					Tr_exp exp = Tr_noExp();
					if (!tycmp(left.ty, right.ty))
						EM_error(a->u.op.left->pos, "comparison type mismatch");
					else if (left.ty->kind == Ty_int)
						exp = Tr_eqExp(a->u.op.oper, left.exp, right.exp);
					else if (left.ty->kind == Ty_string)
						exp = Tr_eqStringExp(a->u.op.oper, left.exp, right.exp);
					else if (left.ty->kind == Ty_array)
						exp = Tr_eqRef(a->u.op.oper, left.exp, right.exp);
					else if (left.ty->kind == Ty_record)
						exp = Tr_eqRef(a->u.op.oper, left.exp, right.exp);
					else
						EM_error(a->u.op.left->pos, "unexpected type in comparsion");
					return expTy(exp, Ty_Int());
				}
				case A_ltOp:
				case A_leOp:
				case A_gtOp:
				case A_geOp: {
					// 只有int 和 string可以用大小比较
					// if (left.ty->kind != Ty_int && left.ty->kind != Ty_string) 
					// 	EM_error(a->u.op.left->pos, "unexpected type: string or integer required");
					// else if (right.ty->kind != left.ty->kind) 
					// 	EM_error(a->u.op.right->pos, "comparison type mismatch");
					// return expTy(NULL, Ty_Int());
					if (left.ty->kind != Ty_int) 
						EM_error(a->u.op.left->pos, "integer required");
					else if (right.ty->kind != Ty_int) 
						EM_error(a->u.op.right->pos, "integer required");
					else
						return expTy(Tr_relExp(a->u.op.oper, left.exp, right.exp), Ty_Int());
					return expTy(Tr_noExp(), Ty_Int());
				}
			}
		}
		case A_recordExp: {
			// record表达式 a{f1=1, f2=2}
			Ty_ty record_type = actual_ty(S_look(tenv, a->u.record.typ));
			if(!record_type) {
				EM_error(a->pos, "undefined type '%s'", S_name(a->u.record.typ));
				return expTy(Tr_noExp(), Ty_Int());
			} else if(record_type->kind != Ty_record) {
				// 引用了非record类型
				EM_error(a->pos, "<%s> was not a record type", S_name(a->u.record.typ));
				return expTy(Tr_noExp(), Ty_Record(NULL));
			}
			// 测试调用的初始化列表和声明的初始化列表
			Ty_fieldList decl_list = record_type->u.record;
			A_efieldList real_list = a->u.record.fields;
			Tr_expList el = NULL;
			int num = 0;
			for(; decl_list && real_list; decl_list = decl_list->tail, real_list = real_list->tail) {
				if (decl_list->head->name != real_list->head->name) {
					EM_error(a->pos, "unexpected field: expect field <%s> but <%s>", S_name(decl_list->head->name), S_name(real_list->head->name));
					continue;
				}
				// 测试调用的field表达式的类型
				struct expty real_field = transExp(level, breakk, venv, tenv, real_list->head->exp);
				el = Tr_ExpList(real_field.exp, el);
				num++;
				if (!tycmp(decl_list->head->ty, real_field.ty))
					EM_error(a->pos, "unexpected type: field <%s> type mismatch", S_name(decl_list->head->name));
			}
			if (num)
				return expTy(Tr_recordExp(num, el), record_type);
			else
				return expTy(Tr_noExp(), Ty_Record(NULL));
		}
		case A_seqExp: {
			// seq语法是允许 () 和两个及以上的(a1;a2...)
			if (a->u.seq == NULL)
				return expTy(Tr_noExp(), Ty_Void());
			// // 两个及以上的seq
			// A_expList el = a->u.seq;
			// for (; el->tail; el = el->tail)
			// 	transExp(level, breakk, venv, tenv, el->head);
			// // 最后一个exp是seq返回值
			// return transExp(level, breakk, venv, tenv, el->head);
			A_expList al;
			struct expty et;
			Tr_expList tl = NULL;
			for (al = a->u.seq; al; al =al->tail) {
				et = transExp(level, breakk, venv, tenv,al->head);
				tl = Tr_ExpList(et.exp,tl);	
			}
			return expTy(Tr_seqExp(tl), et.ty);
		}
		case A_assignExp: {
			// var := exp
			struct expty var = transVar(level, breakk, venv, tenv, a->u.assign.var);	
			struct expty exp = transExp(level, breakk, venv, tenv, a->u.assign.exp);	
			if (!tycmp(var.ty, exp.ty))
				EM_error(a->u.assign.exp->pos, "unexpected type: assignment type mismatch");
			// assignment 是 no value
			return expTy(Tr_assignExp(var.exp, exp.exp), Ty_Void());
		}
		case A_ifExp: {
			// if-then -> no value    if-then-else  ->  then-else value
			struct expty test = transExp(level, breakk, venv, tenv, a->u.iff.test);
			struct expty then = transExp(level, breakk, venv, tenv, a->u.iff.then);
			if(test.ty->kind != Ty_int)
				EM_error(a->u.iff.test->pos, "unexpected type: integer required");
			// 测试是上述哪种之一
			if(a->u.iff.elsee) {
				struct expty elsee = transExp(level, breakk, venv, tenv, a->u.iff.elsee);
				if(!tycmp(then.ty, elsee.ty) && !(then.ty->kind == Ty_nil && elsee.ty->kind == Ty_nil))
					EM_error(a->u.iff.elsee->pos, "then-else branches must return the same type");
				return expTy(Tr_ifExp(test.exp, then.exp, elsee.exp), then.ty);
			}
			else {
				if(then.ty->kind != Ty_void)
					EM_error(a->u.iff.then->pos, "if-then must return no value");
				return expTy(Tr_ifExp(test.exp, then.exp, NULL), Ty_Void());
			}
		}
		case A_whileExp: {
			// 循环里面都需要注意break的层数
			struct expty test = transExp(level, breakk, venv, tenv, a->u.whilee.test);
			Tr_exp done = Tr_doneExp();
			break_level++;
			struct expty body = transExp(level, breakk, venv, tenv, a->u.whilee.body);
			break_level--;
			
			if (test.ty->kind != Ty_int)
				EM_error(a->pos, "unexpected type: while- was not an integer");
			if (body.ty->kind != Ty_void)
				EM_error(a->pos, "unexpected type: do- shouldn't return a value");
			return expTy(Tr_whileExp(test.exp, body.exp, done), Ty_Void());
		}
		case A_forExp: {
			struct expty lo = transExp(level, breakk, venv, tenv, a->u.forr.lo);
			struct expty hi = transExp(level, breakk, venv, tenv, a->u.forr.hi);
			// S_beginScope(venv);
			// S_beginScope(tenv);
			// break_level++;
			// Tr_access ac = Tr_allocLocal(level, a->u.forr.escape); // access
			// S_enter(venv, a->u.forr.var, E_VarEntry(ac, Ty_Int()));
			// struct expty body = transExp(level, breakk, venv, tenv, a->u.forr.body);
			// S_endScope(venv);
			// S_endScope(tenv);
			// break_level--;
			// if (lo.ty->kind != Ty_int)
			// 	EM_error(a->pos, "unexpected type: low- was not an integer");
			// if (hi.ty->kind != Ty_int)
			// 	EM_error(a->pos, "unexpected type: high- was not an integer");
			// if (body.ty->kind != Ty_void)
			// 	EM_error(a->pos, "unexpected type: body must return no value");
			// return expTy(NULL, Ty_Void());
			Tr_exp done = Tr_doneExp();
			if (lo.ty->kind != Ty_int)
				EM_error(a->pos, "low bound was not an integer");
			if (hi.ty->kind != Ty_int)
				EM_error(a->pos, "high bound was not an integer");
			
			S_beginScope(venv);
			break_level++;
			Tr_access ac = Tr_allocLocal(level, a->u.forr.escape); // access
			S_enter(venv, a->u.forr.var, E_VarEntry(ac, Ty_Int()));
			struct expty body = transExp(level, done, venv, tenv, a->u.forr.body);
			S_endScope(venv);
			break_level--;
			if (body.ty->kind != Ty_void)
				EM_error(a->pos, "body exp shouldn't return a value");
			Tr_exp forexp = Tr_forExp(lo.exp, hi.exp, body.exp, done, Tr_simpleVar(ac, level));
			return expTy(forexp, Ty_Void());
		}
		case A_breakExp: {
			if(!break_level)
				EM_error(a->pos, "break statement should be in a loop");
			else
				return expTy(Tr_breakExp(breakk), Ty_Void());	
			return expTy(Tr_noExp(), Ty_Void());
		}
		case A_letExp: {
			struct expty exp;
			A_decList d;
			S_beginScope(venv);
			S_beginScope(tenv);
			Tr_exp te;
			Tr_expList el = NULL;
			for(d = a->u.let.decs; d; d = d->tail) {
				//transDec(level, breakk, venv, tenv, d->head);
				te = transDec(level, breakk, venv, tenv, d->head);
				el = Tr_ExpList(te, el);
			}
			exp = transExp(level, breakk, venv, tenv, a->u.let.body);
			el = Tr_ExpList(exp.exp, el);
			S_endScope(tenv);
			S_endScope(venv);
			return expTy(Tr_seqExp(el), exp.ty);
		}
		case A_arrayExp: {
			// var exp1 of exp2
			Ty_ty array_type = actual_ty(S_look(tenv, a->u.array.typ));
			if(!array_type) {
				EM_error(a->pos, "undefined type <%s>", S_name(a->u.array.typ));
				return expTy(Tr_noExp(), Ty_Int());
			} else if(array_type->kind != Ty_array) {
				// 同record..
				EM_error(a->pos, "unexpected type: <%s> not a array type", S_name(a->u.array.typ));
				return expTy(Tr_noExp(), Ty_Int());
			}
			// exp1是size exp2是初始值
			struct expty size = transExp(level, breakk, venv, tenv, a->u.array.size);	
			struct expty init = transExp(level, breakk, venv, tenv, a->u.array.init);	
			if (size.ty->kind != Ty_int)
				EM_error(a->pos, "unexpected type: array size must be integer");
			if (!tycmp(init.ty, array_type->u.array))
				EM_error(a->pos, "unexpected type: array init type mismatch");
			// 返回值是Ty_array
			return expTy(Tr_arrayExp(size.exp, init.exp), array_type);
		}
	}
	assert(0);
}

static Tr_exp transDec(Tr_level level, Tr_exp breakk, S_table venv, S_table tenv, A_dec d) {
	switch(d->kind) {
		case A_varDec: {
			// var i : typ := init
			// 右边初始值部分
			struct expty init = transExp(level, breakk, venv, tenv, d->u.var.init);
			Tr_access ac = Tr_allocLocal(level, d->u.var.escape); // access
			if (d->u.var.typ) {
				// 如果指明了变量类型
				Ty_ty t = S_look(tenv, d->u.var.typ);
				if (!t)
					EM_error(d->pos, "undefined type <%s>", S_name(d->u.var.typ));
				else {
					if (!tycmp(t, init.ty)) 
						EM_error(d->pos, "variable type and initialize type mismatch");
					
					S_enter(venv, d->u.var.var, E_VarEntry(ac, t));
					return Tr_assignExp(Tr_simpleVar(ac, level), init.exp);;
				}
			}
			// 既然不是指明变量类型，那就不可能是record，注意nil的上下文
			if (init.ty == Ty_Void())
				EM_error(d->pos, "initialize expression return no value");
			else if (init.ty == Ty_Nil())
				EM_error(d->pos, "<%s> is not a record", S_name(d->u.var.var));
			// 默认情况下初始化的类型
			S_enter(venv, d->u.var.var, E_VarEntry(ac, init.ty));
			return Tr_assignExp(Tr_simpleVar(ac, level), init.exp);
		}
		case A_functionDec: {
			A_fundecList fl = d->u.function;

			set fs = set_init();
			set_reset(fs);

			// 类似type dec
			// tiger将连续定义的两个函数联合成一个list
			// 连续的两个list可以进行递归定义
			// 被阻断的两个重名函数是合法的：即看成不同作用域的覆盖定义

			// 首先插入 headers
			for (; fl; fl = fl->tail) {
				A_fundec single_function = fl->head;
				Ty_ty result_type = NULL;

				if (!set_push(fs, single_function->name)) {
					EM_error(single_function->pos, "redefinition of function '%s'", S_name(single_function->name));
					continue;
				}
				if (single_function->result) {
					result_type = S_look(tenv, single_function->result);
					if (!result_type) {
						EM_error(d->pos, "function result: undefined type '%s'", S_name(single_function->result));
						result_type = Ty_Int();
					}
				} else
					result_type = Ty_Void();
				set_reset(s);

				// 插入临时变量
				Ty_tyList type_list = transTyList(tenv, single_function->params);
				Temp_label label = Temp_newlabel();
				Tr_level function_level = Tr_newLevel(level, label, makeFormalList(single_function->params));
				S_enter(venv, single_function->name, E_FunEntry(function_level, label, type_list, result_type));
			}

			set_reset(fs);

			// 第二次遍历，插入具体类型和测试递归定义
			for (fl = d->u.function; fl; fl = fl->tail) {
				A_fundec single_function = fl->head;

				if (!set_push(fs, single_function->name))
					continue;

				E_enventry function_entry = S_look(venv, single_function->name);

				// 检测递归定义的local变量作用域和重复定义
				S_beginScope(venv);

				// 将抽象参数列表转化为 tylist
				A_fieldList single_param = single_function->params;
				Tr_accessList al = Tr_formals(function_entry->u.fun.level);

				set_reset(s);
				for (; single_param; single_param = single_param->tail) {
					if (!set_push(s, single_param->head->name))
						continue;
					Ty_ty t = S_look(tenv, single_param->head->typ);
					if (!t)
						t = Ty_Int();
					assert(al);
					S_enter(venv, single_param->head->name, E_VarEntry(al->head, t));
					al = al->tail;
				}

				struct expty exp = transExp(function_entry->u.fun.level, breakk, venv, tenv, single_function->body);

				// 函数出口
				Tr_procEntryExit(function_entry->u.fun.level, exp.exp, al);

				if (function_entry->u.fun.result->kind == Ty_void && exp.ty->kind != Ty_void)
					EM_error(single_function->pos, "procedure '%s' should't return a value", S_name(single_function->name));
				else if (!tycmp(function_entry->u.fun.result, exp.ty))
					EM_error(single_function->pos, "body result type mismatch");
				
				S_endScope(venv);
			}
			return Tr_noExp();
		}
		case A_typeDec: {
			A_nametyList nl = d->u.type;
			// 由于有递归定义的存在，现插入ty_name类型的空类型的symbol
			set_reset(s);

			// 首先将 name headers 插入
			for(; nl; nl = nl->tail) {
				// 由于tiger存在连续定义的概念，所以两个连续定义之间如果重名则是非法的
				// 但是连续定义被阻断了，重名是合法的
				if (!set_push(s, nl->head->name)) {
					EM_error(d->pos, "redefinition of '%s'", S_name(nl->head->name));
					continue;
				}
				S_enter(tenv, nl->head->name, Ty_Name(nl->head->name, NULL));
			}
			
			// 二次遍历，插入实际值
			set_reset(s);
			for(nl = d->u.type; nl; nl = nl->tail) {
				if(!set_push(s, nl->head->name)) 
					continue;
				Ty_ty t = S_look(tenv, nl->head->name);
				t->u.name.ty = transTy(tenv, nl->head->ty);
			}

			// 检查回环定义
			for (nl = d->u.type; nl; nl = nl->tail) {
				Ty_ty t = S_look(tenv, nl->head->name);
				set_reset(s);
				t = t->u.name.ty;
				while (t && t->kind == Ty_name) {
					if (!set_push(s, t->u.name.sym)) {
						EM_error(d->pos, "illegal recursive definition '%s'", S_name(t->u.name.sym));
						t->u.name.ty = Ty_Int();
						break;
					}
					t = t->u.name.ty;	
					t = t->u.name.ty;	
				}
			}
			return Tr_noExp();
		}
	}
}

static Ty_ty transTy(S_table tenv, A_ty t) {
	switch(t->kind) {
		case A_nameTy: {
			Ty_ty ty = S_look(tenv, t->u.name);
			if(!ty) {
				EM_error(t->pos, "undefined type <%s>", S_name(t->u.name));
				return Ty_Int();
			} else
				return Ty_Name(t->u.name, ty);
		}
		case A_recordTy: {
			// 抽象fieldlist 转为 ty_fieldlist
			set_reset(s);
			return Ty_Record(transFieldList(tenv, t->u.record));
		}
		case A_arrayTy: {
			Ty_ty ty = S_look(tenv, t->u.array);
			if(!ty) {
				EM_error(t->pos, "undefined type <%s>", S_name(t->u.array));
				return Ty_Array(Ty_Int());
			} else
				return Ty_Array(S_look(tenv, t->u.array));
		}
	}
	assert(0);
}
