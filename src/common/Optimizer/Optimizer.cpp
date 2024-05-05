/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "Optimizer.h"

#include "../crc/crc32.h"
#include "../auto_ptr_array.h"

#ifdef _DEBUG
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif
#endif

void COptimizer::Initialize(HASH hash, int id, const char* optimizedSQL)
{
	_hash = hash;
	_id = id;
	_optimizedSQL = optimizedSQL;
}

HASH COptimizer::CalculateHash(const char* clientSQL, int cnt, tokens_vec* tokens)
{
	if (!tokens)
	{
		ClsCrc32 crc32;
		return crc32.Crc((LPBYTE)clientSQL, cnt);
	}

	CTokenizer& tokenizer = tokens->_tokenizer;
	tokenizer.Init(clientSQL, cnt);

	wstring invariantSQL;
	const wchar_t* startpos = tokenizer.copy;
	while (tokenizer.Next() != TOK_EOF)
	{
		if (tokenizer.currentToken.token & TOK_LITERAL)
		{
			invariantSQL.append(startpos, tokenizer.currentToken.tokStart);
			invariantSQL.append(L":?");
			startpos = tokenizer.curPos;
			if (TOK_USTRING == tokenizer.currentToken.token)
				tokenizer.currentToken.tokStart++;	// skip N prefix
			tokens->push_back(tokenizer.currentToken);
		}
	}

	if (tokenizer.tokStart - startpos)
	{
		invariantSQL.append(startpos, tokenizer.tokStart - startpos);
	}

	return COptimizer::CalculateHash(invariantSQL.c_str());
}

HASH COptimizer::CalculateHash(const WCHAR* clientSQL, tokens_vec* tokens)
{
	wstring invariantSQL;

	if (tokens != nullptr)
	{
		// Replace all literal int, float and string values with :?. Effectively the literals are not used for CRC calculation.
		CTokenizer& tokenizer = tokens->_tokenizer;
		tokens->_tokenizer.Init(clientSQL);

		const wchar_t* startpos = tokenizer.tokStart;
		while (tokenizer.Next() != TOK_EOF)
		{
			if (tokenizer.currentToken.token & TOK_LITERAL)
			{
				invariantSQL.append(startpos, tokenizer.currentToken.tokStart);
				invariantSQL.append(L":?");
				startpos = tokenizer.curPos;
				if (TOK_USTRING == tokenizer.currentToken.token)
					tokenizer.currentToken.tokStart++;	// skip N prefix
				tokens->push_back(tokenizer.currentToken);
			}
		}

		if (tokenizer.tokStart - startpos)
		{
			invariantSQL.append(startpos, tokenizer.tokStart - startpos);
		}

		clientSQL = invariantSQL.c_str();
	}

	int len = WideCharToMultiByte(CP_ACP, 0, clientSQL, -1, NULL, 0, 0, 0);
	auto_ptr_array<char> buf(new char[len]);
	WideCharToMultiByte(CP_ACP, 0, clientSQL, -1, buf.get(), len, 0, 0);

	ClsCrc32 crc32;
	return crc32.Crc((LPBYTE)buf.get(), len-1);
}

COptimizerExact::COptimizerExact() : COptimizer(HT_EXACT)
{}

void COptimizerExact::OptimizeSQL(wstring& sql)
{
	int wlen = MultiByteToWideChar(CP_ACP, 0, _optimizedSQL.c_str(), -1, NULL, 0);
	WCHAR* buf = new WCHAR[wlen];
	MultiByteToWideChar(CP_ACP, 0, _optimizedSQL.c_str(), -1, buf, wlen);
	
	sql.assign(buf);
	
	delete [] buf;
}

COptimizerParameterized::COptimizerParameterized() : COptimizer(HT_PARAMETERIZED)
{
}

COptimizerParameterized::COptimizerParameterized(COptimizerParameterized& other, const tokens_vec& tokens) : COptimizer(HT_PARAMETERIZED)
{
	*this = other;
	_tokens = tokens;
}

void COptimizerParameterized::OptimizeSQL(wstring& clientSQL)
{
	int wlen = MultiByteToWideChar(CP_ACP, 0, _optimizedSQL.c_str(), -1, NULL, 0);
	WCHAR* buf = new WCHAR[wlen];
	MultiByteToWideChar(CP_ACP, 0, _optimizedSQL.c_str(), -1, buf, wlen);
	wstring workstr(buf);
	delete [] buf;
	MultiByteToWideChar(CP_ACP, 0, _optimizedSQL.c_str(), -1, &workstr[0], wlen);

	WCHAR temp[50] = L"$";

	for (int i=0; i < _tokens.size(); i++)
	{
		TOKEN& tok = _tokens[i];
		_itow(i+1, &temp[1], 10);

		const WCHAR* p;
		while ((p = wcsstr(&workstr[0], temp)) != nullptr)
		{
			workstr.replace(p - &workstr[0], wcslen(temp), tok.tokStart, wstring::size_type(tok.curPos - tok.tokStart));
		}
	}

	clientSQL.assign(workstr);
}

COptimizer* COptimizerFactory::CreateOptimizer(HASHTYPE hashType)
{
	switch (hashType)
	{
	case HT_EXACT:
		return new COptimizerExact();

	case HT_PARAMETERIZED:
		return new COptimizerParameterized();
	}

	return NULL;
}
