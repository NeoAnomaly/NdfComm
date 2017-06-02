#pragma once

static const ULONG ApiVersion = 1;

///
///
///

typedef enum _COMMAND
{
    CommandStart,
    CommandStop,

} COMMAND;

///
///
///

typedef struct _COMMAND_MESSAGE
{
    ULONG ApiVersion;

    COMMAND Command;

    UCHAR Data[ANYSIZE_ARRAY];

} COMMAND_MESSAGE, *PCOMMAND_MESSAGE;
