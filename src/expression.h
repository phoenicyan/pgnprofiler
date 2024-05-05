/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#if USE_PARF
#include <ppl.h>
#include <ppltasks.h>
#endif

#include "expreval.h"
#include <map>
#include <vector>
#include <string>

namespace bisparser
{

#define MAX_SYMREC_NAME_LEN		16

class symrec
{
public:
	char name[MAX_SYMREC_NAME_LEN]; /* name of symbol */
	int type; /* type of symbol: either VAR or STRING */

	union
	{
		double var; /* value of a VAR */
		char* pstr; /* value of a STRING */
	} value;
};

double like(symrec *var, char const *pstr);

}
