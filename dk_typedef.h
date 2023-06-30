/*
 * dk_typedef.h
 *
 *  Created on: Jun 21, 2023
 *      Author: lochy
 */

#ifndef UITLS_DK_TYPEDEF_H_
#define UITLS_DK_TYPEDEF_H_

#include "stdint.h"
#include "stdlib.h"
#include "stddef.h"

/* Type definitions. */
#define portSTACK_TYPE  uint32_t
#define portBASE_TYPE   int32_t
#define portUBASE_TYPE  uint32_t
#define portMAX_DELAY ( TickType_t ) 0xffffffffUL


typedef portSTACK_TYPE StackType_t;
typedef portBASE_TYPE BaseType_t;
typedef portUBASE_TYPE UBaseType_t;
typedef portUBASE_TYPE TickType_t;

/* Legacy type definitions. */
#define portCHAR        char
#define portFLOAT       float
#define portDOUBLE      double
#define portLONG        long
#define portSHORT       short

#ifndef pvPortMalloc
#define pvPortMalloc        malloc
#endif
#ifndef vPortFree
#define vPortFree           free
#endif

#ifndef pdFALSE
#define pdFALSE                                  ( ( BaseType_t ) 0 )
#endif

#ifndef pdTRUE
#define pdTRUE                                   ( ( BaseType_t ) 1 )
#endif

#ifndef configASSERT
#define configASSERT( x )
#endif

#endif /* UITLS_DK_TYPEDEF_H_ */
