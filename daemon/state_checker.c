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

#include "state_checker.h"

static bool _stc_is_available = FALSE;

// ctl structs
static bool _stc_is_starting = FALSE;
static bool _stc_is_enabled = FALSE;

void stc_set_is_available(bool is_available)
{
	__atomic_store_n(&_stc_is_available, is_available, __ATOMIC_SEQ_CST);
}

bool stc_is_available(void)
{
	return __atomic_load_n(&_stc_is_available, __ATOMIC_SEQ_CST);
}

void stc_set_is_starting(bool is_starting)
{
	__atomic_store_n(&_stc_is_starting, is_starting, __ATOMIC_SEQ_CST);
}

bool stc_is_starting()
{
	return __atomic_load_n(&_stc_is_starting, __ATOMIC_SEQ_CST);
}

void stc_set_is_enabled(bool is_enabled)
{
	__atomic_store_n(&_stc_is_enabled, is_enabled, __ATOMIC_SEQ_CST);
}

bool stc_is_enabled()
{
	return __atomic_load_n(&_stc_is_enabled, __ATOMIC_SEQ_CST);
}
