#pragma once

#include <Windows.h>

BOOL WinSvcCreate(__in LPTSTR ServiceName, __in LPTSTR FilePath);
BOOL WinSvcStart(__in LPTSTR ServiceName);
BOOL WinSvcStop(__in LPTSTR ServiceName);
BOOL WinSvcDelete(__in LPTSTR ServiceName);

BOOL WinSvcIsRunning(__in LPTSTR ServiceName, __out PBOOL IsRunning);
