/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

// Specialized class to break a SQL string into tokens.  Handles '', "" and [] wrapped strings.

#include <string>
#include <vector>

using std::wstring;
using std::vector;

#include <Windows.h>

struct _strsep	// bitmasks
{
	wchar_t		open;
	wchar_t		close;
};

enum _chartype
{
	CT_OTH	= 0x00,


	CT_SQT	= 0x01,
	CT_USQT	= 0x02,
	CT_DQT	= 0x04,

	CT_QUOTE = (CT_SQT | CT_USQT | CT_DQT),		// AND'd with _chartype value, to determine if a string type

	CT_BRC	= 0x08,

	CT_OP	= 0x10,
	CT_WS	= 0x20,		// whitespace

	CT_SEP	= (CT_OP | CT_WS),		// AND'd with value to see if it is either a CT_OP or CT_WS char type, which is a separator

	CT_DIG	= 0x40,

	CT_COLON= 0x80,
};

enum _token		// bitmasks
{
	TOK_EOF = 0,

	// known token
	TOK_SYMBOL	= 0x1,
	TOK_OP		= 0x2,

	TOK_STRING	= 0x04,
	TOK_USTRING = 0x08,
	TOK_NUMBER	= 0x10,

	TOK_LITERAL = (TOK_STRING | TOK_USTRING | TOK_NUMBER),

	TOK_PARAM	= 0x20,

	TOK_UNKNOWN = 0x10000
};

enum _quotetype
{
	QTYP_SINGLE = 1,
	QTYP_USINGLE,
	QTYP_DOUBLE,
	QTYP_BRACE,

	QTYP_NONE = -1
};

extern wchar_t		sepchars[];
extern int			nsepchars;

extern _strsep		strsepchars[];

extern _chartype	specialchrs[];

struct TOKEN
{
	_token			token;
	const wchar_t*	curPos;
	const wchar_t*	tokStart;

	TOKEN()
		:token(TOK_UNKNOWN), curPos(NULL), tokStart(NULL)
	{ }
};

class tokens_vec;

class CTokenizer
{
	friend class tokens_vec;

	const wchar_t*		tokenize;
	bool				binSTRblock;		// for [], '' or "" blocks
	_quotetype			nSTRSepType;		// int element in array of strsepchars

	bool				bUpperTokens;

	bool inline instrsep(void)
	{
		if (binSTRblock)
		{
			if (strsepchars[nSTRSepType].close == *curPos)
				return true;
		}
		else
		{
			int i = specialchrs[*curPos];
			if (CT_SQT == i)
			{
				binSTRblock = true;
				nSTRSepType = QTYP_SINGLE;
				return true;
			}

			i = specialchrs[*(curPos+1)];
			if ('N' == *curPos && CT_SQT == i)
			{
				binSTRblock = true;
				nSTRSepType = QTYP_USINGLE;
				curPos++;		// skip N prefix
				return true;
			}
		}

		return false;
	}

public:
	TOKEN					currentToken;

	const wchar_t*			curPos;
	const wchar_t*			tokStart;

	const wchar_t*			copy;				// contain copy of the original string

	static const wchar_t	eof;

private:
	CTokenizer(void)
		: tokenize(NULL), binSTRblock(false), nSTRSepType(QTYP_NONE), bUpperTokens(true), copy(nullptr)
	{
		tokStart = curPos = tokenize;
	}

public:
	void Init(const wchar_t* txt)
	{
		binSTRblock = false;
		nSTRSepType = QTYP_NONE;
		bUpperTokens = true;
		copy = nullptr;
		tokStart = curPos = tokenize = txt;
	}

	void Init(const char* txt, int cnt)
	{
		binSTRblock = false;
		nSTRSepType = QTYP_NONE;
		bUpperTokens = true;

		int copylen = MultiByteToWideChar(CP_ACP, 0, txt, cnt, 0, 0);

		tokStart = curPos = tokenize = copy = new wchar_t[copylen+1];
		MultiByteToWideChar(CP_ACP, 0, txt, cnt, (LPWSTR)copy, copylen);
		((WCHAR*)copy)[copylen] = 0;
	}

	~CTokenizer()
	{
		delete [] copy;
	}

	void Reset(wchar_t* aTok)
	{
		tokenize = aTok;

		binSTRblock = false;
		nSTRSepType = QTYP_NONE;
		tokStart = curPos = tokenize;
	}

	_token Next(void)
	{
		currentToken.token = TOK_UNKNOWN;

		// first find non whitespace
		while (*curPos < 128 && (specialchrs[(char)*curPos] == CT_WS))
			curPos++;

		tokStart = curPos;

		if (*curPos == eof)
		{
			currentToken.token = TOK_EOF;
		}
		else
		{	
			if (*curPos < 128)
			{
				if (specialchrs[(char)*curPos] == CT_OP)
				{
					curPos++;
					currentToken.token = TOK_OP;
				}
				else if (specialchrs[(char)*curPos] == CT_COLON)
				{
					curPos++;
					if (*curPos < 128 && *curPos == L'?')
					{
						curPos++;
						currentToken.token = TOK_PARAM;
					}
				}
				else if (instrsep())
				{
					currentToken.token = (QTYP_SINGLE == nSTRSepType) ? TOK_STRING : TOK_USTRING;

					curPos++;
					for (;*curPos != eof; curPos++)
					{
						if (instrsep())
						{
							curPos++;
							
							// make sure it's not two quotes following each other
							if(instrsep())
								continue;

							binSTRblock = false;
							break;
						}
					}
				}
				else
					GetSymbolOrNumber();
			}
			else
				GetSymbolOrNumber();
		}

		currentToken.tokStart	= tokStart;
		currentToken.curPos		= curPos;

		return currentToken.token;
	}

	void GetSymbolOrNumber(void)
	{
		if ((*curPos < 128) && (specialchrs[(char)*curPos] & CT_DIG))
			currentToken.token = TOK_NUMBER;
		else
			currentToken.token = TOK_SYMBOL;

		for (;*curPos != eof; curPos++)
		{
			if ((*curPos < 128) && (specialchrs[(char)*curPos] & CT_SEP))		// determine if char is whitespace or operator
			{																	// which is a valid separator of symbols
				break;
			}
		}
	}
};

class tokens_vec : public vector<TOKEN>
{
public:
	CTokenizer _tokenizer;
};
