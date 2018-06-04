%{
#include <stdio.h>
#include "util.h"
#include "errormsg.h"
#define YYDEBUG 1

int yylex(void); /* function prototype */

void yyerror(char *s)
{
 EM_error(EM_tokPos, "%s", s);
}
%}


%union {
	int pos;
	int ival;
	string sval;
	}

%token <sval> ID STRING
%token <ival> INT

%token 
  COMMA COLON SEMICOLON LPAREN RPAREN LBRACK RBRACK 
  LBRACE RBRACE DOT 
  PLUS MINUS TIMES DIVIDE EQ NEQ LT LE GT GE
  AND OR ASSIGN
  ARRAY IF THEN ELSE WHILE FOR TO DO LET IN END OF 
  BREAK NIL
  FUNCTION VAR TYPE 

%left SEMICOLON
%right THEN ELSE DOT DO OF
%right ASSIGN
%left OR
%left AND
%nonassoc EQ NEQ LT LE GT GE
%left MINUS PLUS
%left TIMES DIVIDE
%left UMINUS

%start program

%%

program :	exp
  ;

decs  : dec decs   {$$ = A_DecList($1, $2);}
  |                {$$ = NULL;}
  ;

dec : tydecs     {$$ = A_TypeDec(EM_tokPos, $1);}
  |   vardec    {$$ = $1;}
  |   funcdecs   {$$ = A_FunctionDec(EM_tokPos, $1);}
  ;

tydecs  : tydec tydecs
  | 
  ;

tydec : TYPE ID EQ ty
  ;

ty  : ID                          {$$ = A_NameTy(EM_tokPos, S_Symbol($1));}
  |   LBRACE tyfields RBRACE      {$$ = A_RecordTy(EM_tokPos, $2);}
  |   ARRAY OF ID                 {$$ = A_ArrayTy(EM_tokPos, S_Symbol($3));}
  ;

tyfields  : 
  |   tyfield COMMA tyfields      {$$ = A_FieldList($1, $3);}
  |   tyfield                     
  ;

tyfield : ID COLON ID             {$$ = A_FieldList(A_Field(EM_tokPos, S_Symbol($1), S_Symbol($3)), NULL);}
  ;

vardec  : VAR ID ASSIGN exp       {$$ = A_VarDec(EM_tokPos, S_Symbol($2), NULL, $4);}
  |   VAR ID COLON ID ASSIGN exp  {$$ = A_VarDec(EM_tokPos, S_Symbol($2), S_Symbol($4), $6);}
  ;

funcdecs :  funcdec funcdecs
  |   
  ;
funcdec : FUNCTION ID LPAREN tyfields RPAREN EQ exp       {$$ = A_Fundec(EM_tokPos, S_Symbol($2), $4, NULL, $7);}
  |   FUNCTION ID LPAREN tyfields RPAREN COLON ID EQ exp  {$$ = A_Fundec(EM_tokPos, S_Symbol($2), $4, S_Symbol($7), $9);}
  ;

lvalue  : ID                  
  |   lvalue_not_ID
  ;

lvalue_not_ID  :   lvalue DOT ID      {$$ = A_FieldVar(EM_tokPos, $1, S_Symbol($3));}
  |   lvalue_not_ID LBRACK exp RBRACK {$$ = A_SubscriptVar(EM_tokPos, $1, $3);}
  |   ID LBRACK exp RBRACK            {$$ = A_SubscriptVar(EM_tokPos, A_SimpleVar(EM_tokPos, S_Symbol($1)), $3);}

exp : ID                      
  |   lvalue_not_ID           {$$ = A_VarExp(EM_tokPos, $1);}
  |   NIL                     {$$ = A_NilExp(EM_tokPos);}
  |   LPAREN expseq RPAREN    {$$ = A_SeqExp(EM_tokPos, $2);}
  |   LPAREN RPAREN           {$$ = A_SeqExp(EM_tokPos, null);}
  |   INT                     {$$ = A_IntExp(EM_tokPos, $1);}
  |   STRING                  {$$ = A_StringExp(EM_tokPos, $1);}
  |   MINUS exp %prec UMINUS  {$$ = A_OpExp(EM_tokPos, A_minusOp, A_IntExp(EM_tokPos, 0), $1);}

  |   exp PLUS exp            {$$ = A_OpExp(EM_tokPos, A_plusOp, $1, $3);}
  |   exp MINUS exp           {$$ = A_OpExp(EM_tokPos, A_minusOp, $1, $3);}
  |   exp TIMES exp           {$$ = A_OpExp(EM_tokPos, A_timesOp, $1, $3);}
  |   exp DIVIDE exp          {$$ = A_OpExp(EM_tokPos, A_divideOp, $1, $3);}

  |   ID LPAREN RPAREN            {$$ = A_CallExp(EM_tokPos, S_Symbol($1), null);}    
  |   ID LPAREN parameter RPAREN  {$$ = A_CallExp(EM_tokPos, S_Symbol($1), $3);}

  |   exp EQ exp              {$$ = A_OpExp(EM_tokPos, A_eqop, $1, $3);}
  |   exp NEQ exp             {$$ = A_OpExp(EM_tokPos, A_neqop, $1, $3);}
  |   exp LT exp              {$$ = A_OpExp(EM_tokPos, A_ltop, $1, $3);}
  |   exp LE exp              {$$ = A_OpExp(EM_tokPos, A_leop, $1, $3);}
  |   exp GT exp              {$$ = A_OpExp(EM_tokPos, A_gtop, $1, $3);}
  |   exp GE exp              {$$ = A_OpExp(EM_tokPos, A_geop, $1, $3);}

  |   exp AND exp             {$$ = A_IfExp(EM_tokPos, $1, $3, A_IntExp(EM_tokPos, 0));}
  |   exp OR exp              {$$ = A_IfExp(EM_tokPos, $1, A_IntExp(EM_tokPos, 1), $3);}

  |   ID LBRACK exp RBRACK OF exp     {$$ = A_ArrayExp(EM_tokPos, S_Symbol($1), $3, $6);}
  |   ID LBRACE RBRACE                {$$ = A_RecordExp(EM_tokPos, S_Symbol($1), null);}
  |   ID LBRACE asm RBRACE            {$$ = A_RecordExp(EM_tokPos, S_Symbol($1), $3);}
  |   lvalue ASSIGN exp               {$$ = A_AssignExp(EM_tokPos, $1, $3);}
  |   IF exp THEN exp                 {$$ = A_IfExp(EM_tokPos, $2, $4, NULL);}
  |   IF exp THEN exp ELSE exp        {$$ = A_IfExp(EM_tokPos, $2, $4, $6);}
  |   WHILE exp DO exp                {$$ = A_WhileExp(EM_tokPos, $2, $4);}
  |   FOR ID ASSIGN exp TO exp DO exp {$$ = A_ForExp(EM_tokPos, S_Symbol($2), $4, $6, $8);}
  |   BREAK                           {$$ = A_BreakExp(EM_tokPos);}
  
  |   LET decs IN expseq END          {$$ = A_LetExp(EM_tokPos, $2, A_SeqExp(EM_tokPos, $4));}
  ;

expseq  : exp                {$$ = A_ExpList($1, NULL);}
  | exp SEMICOLON expseq     {$$ = A_ExpList($1, $3);}
  ;

asm : ID EQ exp              {$$ = A_EfieldList(A_Efield(S_Symbol($1), $3), NULL);}
  | ID EQ exp COMMA asm      {$$ = A_EfieldList(A_Efield(S_Symbol($1), $3), $5);}
  ;

parameter : exp              {$$ = A_ExpList($1, NULL);}
  | exp COMMA parameter      {$$ = A_ExpList($1, $3);}
  ;


