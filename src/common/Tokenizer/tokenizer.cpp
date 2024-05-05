/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "Tokenizer.h"

wchar_t		sepchars[] = {L',', L'=', L'>', L'<', L'(', L')', L'-', L'+', L'*', L'/', L'|', L'{', L'}'};
int			nsepchars = sizeof(sepchars) / sizeof sepchars[0];

_chartype	specialchrs[128];

_strsep		strsepchars[] = {{L'\0', L'\0'}, /*QTYP_SINGLE*/{L'\'', L'\''}, /*QTYP_USINGLE*/{L'\'', L'\''}, /*QTYP_DOUBLE*/{L'"', L'"'},{L'\0', L'\0'}, /*QTYP_BRACE*/{L'[', L']'}};	// Caution: QTYP_* are undexes in this array. Changes in this array must be made along with changes in those constants.

const wchar_t	CTokenizer::eof = L'\0';

struct _init_specialchrs
{
	_init_specialchrs(void)
	{
		memset(specialchrs, 0, sizeof specialchrs);

		// whitespace chars
		specialchrs[' '] = CT_WS;
		specialchrs['\t'] = CT_WS;
		specialchrs['\r'] = CT_WS;
		specialchrs['\n'] = CT_WS;
		specialchrs['\f'] = CT_WS;
		specialchrs['\v'] = CT_WS;

		// digits
		specialchrs['0'] = CT_DIG;
		specialchrs['1'] = CT_DIG;
		specialchrs['2'] = CT_DIG;
		specialchrs['3'] = CT_DIG;
		specialchrs['4'] = CT_DIG;
		specialchrs['5'] = CT_DIG;
		specialchrs['6'] = CT_DIG;
		specialchrs['7'] = CT_DIG;
		specialchrs['8'] = CT_DIG;
		specialchrs['9'] = CT_DIG;

		// special operator chars
		specialchrs[','] = CT_OP;
		specialchrs['='] = CT_OP;
		specialchrs['>'] = CT_OP;
		specialchrs['<'] = CT_OP;
		specialchrs['('] = CT_OP;
		specialchrs[')'] = CT_OP;
		specialchrs['-'] = CT_OP;
		specialchrs['+'] = CT_OP;
		specialchrs['*'] = CT_OP;
		specialchrs['/'] = CT_OP;
		specialchrs['|'] = CT_OP;
		specialchrs['{'] = CT_OP;
		specialchrs['}'] = CT_OP;
		specialchrs['?'] = CT_OP;

		specialchrs[':'] = CT_COLON;

		// SQL quote type chars
		specialchrs['['] = CT_BRC;
		specialchrs[']'] = CT_BRC;
		specialchrs['\''] = CT_SQT;
		specialchrs['"'] = CT_DQT;
	}
} g_init_specialchrs;
