/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "HookMgr.h"
#include "ColorPalette.h"
#include "SkinMenuMgr.h"

extern CColorPalette g_palit;

template<class HookMgrType>
HookMgrType& CHookMgr<HookMgrType>::GetHookMgrInstance()
{
	static HookMgrType manager(g_palit);
	return manager;
}

// Explicitly request instantiation of CHookMgr class to prevent LNK2019 error.
template CHookMgr<CSkinMenuMgr>;
