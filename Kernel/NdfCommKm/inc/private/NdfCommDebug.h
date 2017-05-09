#pragma once

#include <ntddk.h>

#define NDFCOMM_STRING_MEM_TAG		'10CN'
#define NDFCOMM_CLIENT_MEM_TAG		'20CN'
#define NDFCOMM_CCB_MEM_TAG			'30CN'

#if !defined(TRACE_LEVEL_NONE)
#define TRACE_LEVEL_NONE        0  
#define TRACE_LEVEL_CRITICAL    1  
#define TRACE_LEVEL_FATAL       1  
#define TRACE_LEVEL_ERROR       2  
#define TRACE_LEVEL_WARNING     3  
#define TRACE_LEVEL_INFORMATION 4  
#define TRACE_LEVEL_VERBOSE     5  
#define TRACE_LEVEL_RESERVED6   6
#define TRACE_LEVEL_RESERVED7   7
#define TRACE_LEVEL_RESERVED8   8
#define TRACE_LEVEL_RESERVED9   9
#endif

#if DBG
#	define NdfCommDebugTrace(_Level, _Reserved, _MessageFmt, ...) DbgPrintEx((ULONG)-1, _Level, "NdfComm!%s" _MessageFmt, __func__, __VA_ARGS__)
#else
#	define NdfCommDebugTrace
#endif

VOID
NdfCommDbgPrint(
	__in ULONG Level,
	__in PCSTR Fn,
	__in PCSTR MessageFmt,
	...
);