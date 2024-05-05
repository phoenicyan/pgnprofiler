%{
#include "expression.h"
%}

/* Require bison 2.3 or later */
%require "2.3"

/*%debug*/

%start start

/* write out a header file containing the token defines */
%defines

/* use newer C++ skeleton file */
%skeleton "lalr1.cc"

/* namespace to enclose parser in */
%name-prefix="bisparser"

/* set the parser's class identifier */
%define "parser_class_name" "Parser"

/* keep track of the current position within the input */
%locations

/* The driver is passed by reference to the parser and to the scanner. This
 * provides a simple but effective pure interface, not relying on global
 * variables. */
%parse-param { class Driver& driver }

/* verbose error messages */
%error-verbose

%union {
	double doubleVal; /* For returning numbers. */
	std::string* stringVal; /* For returning literal strings. */
	class symrec* symtabPtr; /* For returning symbol-table pointers. */
}

%token <doubleVal> NUM /* Simple double precision number. */
%token <stringVal> STRING /* Literal string. */
%token <symtabPtr> VAR /* Variable. */

%token <operator>  OR     "||"
%token <operator>  AND    "&&"
%token <operator>  EQ     "=="
%token <operator>  NE     "!="
%token <operator>  NEALT  "<>"
%token <operator>  LE     "<="
%token <operator>  GE     ">="
%token <operator>  LIKE   "like"
%token <operator>  ILIKE  "ilike"
%token <operator>  NOT   "not"

%type <doubleVal> exp
%type <doubleVal> like

%left '=' EQ OR AND '>' '<' LE GE NE NEALT LIKE ILIKE NOT
%left '-' '+'
%left '*' '/'
%left NEG '!' /* negation--unary minus and logical not operator */

%{
#include "driver.h"
#include "parser.h"

/* this "connects" the bison parser in the driver to the flex scanner class
 * object. it defines the yylex() function call to pull the next token from the
 * current lexer object of the driver context. */
#undef yylex
#define yylex driver.lex
%}

%% /* The grammar follows. */
start:	/* empty */
		| exp ';' { driver._bisrez = $1; }
		;

/*		| STRING { }
*/
exp:	NUM { $$ = $1; }
		| VAR { $$ = $1->value.var; }
		| VAR like STRING { $$ = driver.IsLike($1, $2, $3->c_str()); }
/*		| VAR "ilike" STRING { $$ = driver.IsILike($1, $3->c_str()); }
		| VAR "not" "like" STRING { $$ = !driver.IsLike($1, $3->c_str()); }
		| VAR "not" "ilike" STRING { $$ = !driver.IsILike($1, $3->c_str()); }
*/		
		/* note: = and == are the same, i.e. "is equal" operator */
		| VAR '=' NUM { $$ = ($1->value.var == $3); }
		| VAR "==" NUM { $$ = ($1->value.var == $3); }    
		| VAR '=' STRING { $$ = 0 == strcmp($1->value.pstr, $3->c_str()); }
		| VAR "==" STRING { $$ = 0 == strcmp($1->value.pstr, $3->c_str()); }    
		
		| VAR "!=" NUM { $$ = ($1->value.var != $3); }
		| VAR "<>" NUM { $$ = ($1->value.var != $3); }
		| VAR "!=" STRING { $$ = 0 != strcmp($1->value.pstr, $3->c_str()); }
		| VAR "<>" STRING { $$ = 0 != strcmp($1->value.pstr, $3->c_str()); }

		| exp "||" exp { $$ = ($1 != 0.0) || ($3 != 0); }
		| exp "&&" exp { $$ = ($1 != 0.0) && ($3 != 0); }
		| VAR '>' exp { $$ = $1->value.var > $3; }
		| VAR '<' exp { $$ = $1->value.var < $3; }
		| VAR ">=" exp { $$ = $1->value.var >= $3; }
		| VAR "<=" exp { $$ = $1->value.var <= $3; }
		
		| exp '+' exp { $$ = $1 + $3; }
		| exp '-' exp { $$ = $1 - $3; }
		| exp '*' exp { $$ = $1 * $3; }
		| exp '/' exp { $$ = $1 / $3; }
		| '(' exp ')' { $$ = $2; }
		| '-' exp %prec NEG { $$ = -$2; }
		| '!' exp %prec NOT { $$ = !($2); }
		;

/*		
like:	"like" { $$ = new std::string("like"); } | "ilike" { $$ = new std::string("ilike"); } | "not" "like" { $$ = new std::string("not like"); } | "not" "ilike" { $$ = new std::string("not ilike"); } 
*/
like:	"like" { $$ = 0; } | "ilike" { $$ = 1; } | "not" "like" { $$ = 2; } | "not" "ilike" { $$ = 3; }

%%

void bisparser::Parser::error(const Parser::location_type& l, const std::string& m)
{
    driver.error(l, m);
}
