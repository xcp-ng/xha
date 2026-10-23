#ifndef STATE_CHECKER_H
#define STATE_CHECKER_H

#include "sm.h"
#ifdef __cplusplus
extern "C"
{
#endif

//
//      Copyright (c) Vates SAS
//      All Rights Reserved. Unpublished rights reserved
//      under the copyright laws of the United States.
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU Lesser General Public License as published
//      by the Free Software Foundation; version 2.1 only. with the special
//      exception on linking described in file LICENSE.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU Lesser General Public License for more details.
//
//
//  DESCRIPTION:
//
//      This module is responsible for abstracting system status.
//      It does the bridge btween statefiles and state scripts.
//
//  AUTHORS:
//
//      Antoine Bartuccio
//
//  CREATION DATE: 
//
//      Oct 23, 2026
//
//   

#include <stdbool.h>

#include "mtctypes.h"

typedef struct
{
	struct {
		bool start; // starting + write + read
	} ctl;

	MTC_U32 pool_state; // pool_state

	MTC_HOSTMAP domains; // sfdomain

} COM_STATE_CHECKER, *PCOM_STATE_CHECKER;

#ifdef __cplusplus
}
#endif

#endif // STATE_CHECKER_H