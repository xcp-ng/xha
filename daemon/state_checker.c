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
#include <pthread.h>

static bool _stc_is_available = FALSE;

// ctl structs
static bool _stc_is_starting = FALSE;
static bool _stc_is_enabled = FALSE;

static struct 
{
	bool initialized;
	struct Latency latency;
	pthread_mutex_t lock;
} _stc_latency = {
	.latency = {
		.latest_ms = -1,
		.max_ms = -1,
		.min_ms = -1,
	},
	.lock = PTHREAD_MUTEX_INITIALIZER,
};

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

void stc_set_latency(const struct Latency *latency)
{
	pthread_mutex_lock(&_stc_latency.lock);
	_stc_latency.latency.latest_ms = latency->latest_ms;
	_stc_latency.latency.min_ms = latency->min_ms;
	_stc_latency.latency.max_ms = latency->max_ms;
	pthread_mutex_unlock(&_stc_latency.lock);
}

void stc_get_latency(struct Latency *latency)
{
	pthread_mutex_lock(&_stc_latency.lock);
	latency->latest_ms = _stc_latency.latency.latest_ms;
	latency->min_ms = _stc_latency.latency.min_ms;
	latency->max_ms = _stc_latency.latency.max_ms;
	pthread_mutex_unlock(&_stc_latency.lock);
}