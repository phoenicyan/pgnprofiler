/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "driver.h"
#include "expreval.c"
#include <boost/regex.hpp>

extern const WCHAR* TRC_TYPE_STR[];
extern const WCHAR* CMD_TYPE_STR[26];
extern const WCHAR* CURSOR_TYPE_STR[];

namespace bisparser {

CVariable::CVariable(const char* name, int index, BISVAR type) : _index(index), _type(type)
{
	strcpy(_sym.name, name);

	_sym.type = (type == BISVAR_NUMERIC) ? Parser::token::VAR : Parser::token::STRING;
}

int CVariable::SetEnumVal(int val)
{
	_sym.value.var = (double)val;
	return 0;
}

int CVariable::SetText(const char* text)
{
	_sym.value.pstr = (char*)text;
	return 0;
}

int CVariable::SetNumeric(double val)
{
	_sym.value.var = val;
	return 0;
}

Driver::Driver()
	: _parser(*this)
	, _bisrez(0.0)
	, _lexemeIx(0)
{
	lastError[0] = 0;

	// variable name should not be longer than MAX_SYMREC_NAME_LEN
	AddKnownVariable("abstime", 0, BISVAR_TEXT);
	AddKnownVariable("reltime", 1, BISVAR_NUMERIC);
	AddKnownVariable("sqltype", 2, BISVAR_ENUM);
	AddKnownVariable("clientsql", 3, BISVAR_TEXT);
	AddKnownVariable("parse", 4, BISVAR_NUMERIC);
	AddKnownVariable("prepare", 5, BISVAR_NUMERIC);
	AddKnownVariable("execute", 6, BISVAR_NUMERIC);
	AddKnownVariable("getrows", 7, BISVAR_NUMERIC);
	AddKnownVariable("rows", 8, BISVAR_NUMERIC);
	AddKnownVariable("database", 9, BISVAR_TEXT);
	AddKnownVariable("username", 10, BISVAR_TEXT);
	AddKnownVariable("pid", 11, BISVAR_NUMERIC);
	AddKnownVariable("sessid", 12, BISVAR_NUMERIC);
	AddKnownVariable("cmdid", 13, BISVAR_NUMERIC);
	AddKnownVariable("cursormode", 14, BISVAR_ENUM);
	AddKnownVariable("application", 15, BISVAR_TEXT);
	AddKnownVariable("cmdtype", 16, BISVAR_ENUM);

	int i=0;
	for (const WCHAR** p = &TRC_TYPE_STR[0]; *p != 0; p++)
	{
		CW2A name(*p);
		AddKnownConstant(name, i++);
	}

	i=0;
	for (const WCHAR** p = &CMD_TYPE_STR[0]; *p != 0; p++)
	{
		CW2A name(*p);
		AddKnownConstant(name, i++);
	}

	i=0;
	for (const WCHAR** p = &CURSOR_TYPE_STR[0]; *p != 0; p++)
	{
		CW2A name(*p);
		AddKnownConstant(name, i++);
	}
}

Driver::~Driver()
{
	for (std::map<std::string, CVariable*>::iterator it = _knownvars.begin(); it != _knownvars.end(); it++)
	{
		delete it->second;
	}
	_knownvars.clear();

	_knownconsts.clear();
}

bool Driver::Prepare(const char* input, const char* sname)
{
	return (BuildLexemes(input) == 0);
}

void Driver::error(const class location& l,
		   const std::string& m)
{
    //std::cerr << l << ": " << m << std::endl;
	memset(lastError, 0, sizeof(lastError));
	strncpy(lastError, m.c_str(), 255);
}

void Driver::error(const std::string& m)
{
	memset(lastError, 0, sizeof(lastError));
	strncpy(lastError, m.c_str(), 255);
}

Parser::token::yytokentype Driver::lex(Parser::semantic_type* yylval, location* loc)
{
	if (_lexemeIx >= _lexemes.size())
	{
		return (Parser::token::yytokentype)0;
	}

	CLexeme& l = _lexemes[_lexemeIx++];
	switch (l._type)
	{
	case Parser::token::NUM:
		yylval->doubleVal = l._val.dVal;
		break;
	case Parser::token::STRING:
		yylval->stringVal = l._val.sVal;
		break;
	case Parser::token::VAR:
		yylval->symtabPtr = &l._val.vVal->_sym;
		break;
	}
	return (Parser::token::yytokentype)l._type;
}

int Driver::BuildLexemes(const char* text)
{
	int c, c2;
	
	*lastError = 0;
	ClearLexemes();

start:		
	/* Ignore white space, get first nonwhite character. */
	while ((c = *text++) == ' ' || c == '\t' || c == '\r' || c == '\n');
	if (c == 0)
		return 0;	// end of input reached
		
	/* Char starts a number => parse the number. */
	if (c == '.' || isdigit (c))
	{
		LexemeVal val;
		sscanf(--text, "%lf", &val);
		_lexemes.push_back(CLexeme(Parser::token::NUM, val));
		while (*text == '.' || isdigit (*text)) text++;
		goto start;
	}

	/* Char starts an identifier => read the name. */
	if (isalpha (c) || c == '_')
	{
		std::string sym;

		do
		{
			/* If buffer is full, make it bigger. */
			if (sym.length() > 8000)
			{
				strcpy(lastError, "The identifier is too long (exceeded 8K limit for the name).");
				return BISLEX_UNDEF_IDENT;
			}
			
			/* Add this character to the buffer. */
			sym += c;
			
			/* Get another character. */
			c = *text++;
		}
		while (isalnum (c) || c == '_');

		--text;

		/* Check if operator LIKE used. */
		if (0 == _stricmp(sym.c_str(), "like"))
		{
			_lexemes.push_back(CLexeme(Parser::token::LIKE));
			goto start;
		}
		
		/* Check if operator ILIKE used. */
		if (0 == _stricmp(sym.c_str(), "ilike"))
		{
			_lexemes.push_back(CLexeme(Parser::token::ILIKE));
			goto start;
		}
		
		/* Check if operator NOT used. */
		if (0 == _stricmp(sym.c_str(), "not"))
		{
			_lexemes.push_back(CLexeme(Parser::token::NOT));
			goto start;
		}
		
		/* Check if a known variable is used. */
		CVariable* v = GetKnownVariable(sym.c_str());
		if (v != 0)
		{
			/* Remember the variable in "used variables" list. */
			UseVariable(v);

			LexemeVal val;
			val.vVal = v;
			_lexemes.push_back(CLexeme(Parser::token::VAR, val));
			goto start;
		}

		/* Check if a known constant is used. */
		int const_index = GetKnownConstant(sym.c_str());
		if (const_index >= 0)
		{
			LexemeVal val;
			val.dVal = (double)const_index;
			_lexemes.push_back(CLexeme(Parser::token::NUM, val));
			goto start;
		}

		/* Oops! Report error. */
		strcpy(lastError, "Undefined identifier: ");
		strcat(lastError, sym.c_str());
		return BISLEX_UNDEF_IDENT;
	}

	/* Process operator. */
	switch (c)
	{
	case '|':
		if ('|' == (c2 = *text++)) { _lexemes.push_back(CLexeme(Parser::token::OR)); goto start; }
		--text;
		break;
	case '&':
		if ('&' == (c2 = *text++)) { _lexemes.push_back(CLexeme(Parser::token::AND)); goto start; }
		--text;
		break;
	case '=':
		if ('=' == (c2 = *text++)) { _lexemes.push_back(CLexeme(Parser::token::EQ)); goto start; }
		--text;
		break;
	case '!':
		if ('=' == (c2 = *text++)) { _lexemes.push_back(CLexeme(Parser::token::NE)); goto start; }
		--text;
		break;
	case '<':
		if ('=' == (c2 = *text++)) { _lexemes.push_back(CLexeme(Parser::token::LE)); goto start; }
		if ('>' == c2) { _lexemes.push_back(CLexeme(Parser::token::NEALT)); goto start; }
		--text;
		break;
	case '>':
		if ('=' == (c2 = *text++)) { _lexemes.push_back(CLexeme(Parser::token::GE)); goto start; }
		--text;
		break;
	}
	
	/* Literal string: '(\\.|''|[^'\n])*' | "(\\.|\"\"|[^"\n])*\". */
	if (c == '\'' || c == '"')
	{
		int quote = c;
		std::string sym;

		do
		{
			sym += c;
			
			/* Get another character. */
			c = *text++;
			
			if (c == quote)
			{
				sym += c;
				LexemeVal val;
				val.sVal = new std::string(sym.substr(1,sym.length()-2));
				_lexemes.push_back(CLexeme(Parser::token::STRING, val));
				goto start;
			}
		}
		while (c != '\n' && c != EOF);

		strcpy(lastError, "Invalid string literal: ");
		strcat(lastError, sym.c_str());
		return BISLEX_UNDEF_IDENT;
	}

	/* Any other character is a token by itself. */
	_lexemes.push_back(CLexeme((Parser::token::yytokentype)c));
	goto start;
}

void Driver::ClearLexemes()
{
	_lexemes.clear();
}

int Driver::AddKnownVariable(const char* name, int index, BISVAR type)
{
	std::string lwName(name);
	_knownvars.insert(std::pair<std::string, CVariable*>(_strlwr((char*)lwName.c_str()), new CVariable(name, index, type)));
	return 0;
}

int Driver::AddKnownConstant(const char* name, int index)
{
	std::string lwName(name);
	_knownconsts.insert(std::pair<std::string, int>(_strlwr((char*)lwName.c_str()), index));
	return 0;
}

CVariable* Driver::GetKnownVariable(const std::string& name)
{
	std::string lwName(name);
	
	std::map<std::string, CVariable*>::iterator it=_knownvars.find(_strlwr((char*)lwName.c_str()));
	return it != _knownvars.end() ? it->second : 0;
}

int Driver::GetKnownConstant(const std::string& name)
{
	std::string lwName(name);
	
	std::map<std::string, int>::iterator it=_knownconsts.find(_strlwr((char*)lwName.c_str()));
	return it != _knownconsts.end() ? it->second : -1;
}

void Driver::UseVariable(CVariable* v)
{
	bool used = false;
	for (vector<CVariable*>::const_iterator it=_vars.begin(); it != _vars.end(); it++)
	{
		if (v == *it)
		{
			used = true;
			break;
		}
	}

	if (!used)
		_vars.push_back(v);
}

// Returns 0 if success, otherwise 1.
// User expression result is stored in the member variable "_bisrez".
int Driver::Evaluate()
{
	_lexemeIx = 0;
	return _parser.parse();
}

// op values (see production rule 'like' in expreval.y):
//   0 - LIKE
//   1 - ILIKE
//   2 - NOT LIKE
//   3 - NOT ILIKE
double Driver::IsLike(symrec *var, double op, char const *pstr)
{
	std::string s(var->value.pstr), sre(pstr);
	boost::regex re;
	bool bNot = (op == 2 || op == 3);
	bool bCaseSensitive = (op == 0 || op == 2);

	try
	{	// Assignment and construction initialize the FSM used for regexp parsing
		if (bCaseSensitive)
			re = sre;
		else
			re.assign(sre, boost::regex_constants::icase);
	}
	catch (boost::regex_error& e)
	{
		sprintf(lastError, "%s is not a valid regular expression: \"%s\"\n", sre.c_str(), e.what());
		return 0;
	}

	int rez = boost::regex_match(s.begin(), s.end(), re) ? 1 : 0;
	if (bNot)
		rez = !rez;

	return (double)rez;
}

} // namespace bisparser
