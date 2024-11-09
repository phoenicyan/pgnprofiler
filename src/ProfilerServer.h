/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include <ATLComTime.h>

class CLoggerItemBase;
class UnlimitedWait;

extern DWORD ProfilerTimerInit();
extern DWORD GetAppList(DWORD* appList, /*inout*/int* entries);
extern void CaptureToggle(CLoggerItemBase* pBlock, UnlimitedWait* pUnlimitedWait);
