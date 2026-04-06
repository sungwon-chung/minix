#ifndef _TRAPS_H
#define _TRAPS_H

#include "kernel.h"

extern ExceptionInfo *current_exception;

extern void TrapKernelHandler(ExceptionInfo *info);
extern void TrapClockHandler(ExceptionInfo *info);
extern void TrapIllegalHandler(ExceptionInfo *info);
extern void TrapMemoryHandler(ExceptionInfo *info);
extern void TrapMathHandler(ExceptionInfo *info);
extern void TrapTTYReceiveHandler(ExceptionInfo *info);
extern void TrapTTYTransmitHandler(ExceptionInfo *info);

#endif