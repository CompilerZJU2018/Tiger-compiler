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

decs  : dec decs
  | 
  ;

dec : tydec
  |   vardec
  |   funcdec
  ;

tydec : TYPE ID EQ ty
  ;

ty  : ID
  |   LBRACE tyfields RBRACE
  |   ARRAY OF ID
  ;

tyfields  : 
  |   tyfield COMMA tyfields
  |   tyfield
  ;

tyfield : ID COLON ID
  ;

vardec  : VAR ID ASSIGN exp
  |   VAR ID COLON ID ASSIGN exp
  ;

funcdec : FUNCTION ID LPAREN tyfields RPAREN EQ exp
  |   FUNCTION ID LPAREN tyfields RPAREN COLON ID EQ exp
  ;

lvalue  : ID
  |   lvalue_not_ID
  ;

lvalue_not_ID  :   lvalue DOT ID
  |   lvalue_not_ID LBRACK exp RBRACK
  |   ID LBRACK exp RBRACK

exp : ID
  |   lvalue_not_ID
  |   NIL
  |   LPAREN expseq RPAREN
  |   LPAREN RPAREN
  |   INT
  |   STRING
  |   MINUS exp %prec UMINUS

  |   exp PLUS exp
  |   exp MINUS exp
  |   exp TIMES exp
  |   exp DIVIDE exp

  |   ID LPAREN RPAREN
  |   ID LPAREN parameter RPAREN

  |   exp EQ exp
  |   exp NEQ exp
  |   exp LT exp
  |   exp LE exp
  |   exp GT exp
  |   exp GE exp

  |   exp AND exp
  |   exp OR exp

  |   ID LBRACK exp RBRACK OF exp
  |   ID LBRACE RBRACE
  |   ID LBRACE asm RBRACE
  |   lvalue ASSIGN exp
  |   IF exp THEN exp
  |   IF exp THEN exp ELSE exp
  |   WHILE exp DO exp
  |   FOR ID ASSIGN exp TO exp DO exp
  |   BREAK
  
  |   LET decs IN expseq END
  ;

expseq  : exp
  | exp SEMICOLON expseq
  ;

asm : ID EQ exp
  | ID EQ exp COMMA asm
  ;

parameter : exp
  | exp COMMA parameter
  ;


