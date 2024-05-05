/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include "../CommonCfg.h"

#include "../Tokenizer/tokenizer.h"

#include <map>
#include <memory>
#include <string>
using std::map;
using std::string;

// use this for faster implementation, that allows an intrusive use
#include "myboost/shared_in_ptr.hpp"
using myboost::shared_in_base;
using myboost::shared_in_ptr;

class CSQLText;

typedef unsigned HASH;

enum HASHTYPE
{
	HT_EXACT,
	HT_PARAMETERIZED,
};

class COptimizer;

typedef shared_in_ptr<COptimizer> refOptimizer;
typedef map<HASH, refOptimizer> mapOptimizers;

class COptimizer : public shared_in_base<>
{
public:
	HASH		_hash;
	HASHTYPE	_hashType;
	int			_id;		// row id in DB
	string		_optimizedSQL;

	COptimizer(HASHTYPE hashType) : _hash(0), _hashType(hashType), _id(0)
	{}

	virtual void Initialize(HASH hash, int id, const char* optimizedSQL);

	virtual void OptimizeSQL(wstring& sql) = 0;

	static HASH CalculateHash(const WCHAR* clientSQL, tokens_vec* tokens = nullptr);

	static HASH CalculateHash(const char* clientSQL, int cnt, tokens_vec* tokens = nullptr);
};

class COptimizerParameterized : public COptimizer
{
	tokens_vec	_tokens;
public:
	COptimizerParameterized();
	COptimizerParameterized(COptimizerParameterized& other, const tokens_vec& tokens);

	void OptimizeSQL(wstring& sql);
};

class COptimizerExact : public COptimizer
{
public:
	COptimizerExact();

	void OptimizeSQL(wstring& sql);
};

class COptimizerFactory
{
public:
	static COptimizer* CreateOptimizer(HASHTYPE hashType);
};
