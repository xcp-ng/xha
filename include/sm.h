//
//  MODULE: sm.h
//

#ifndef SM_H
#define SM_H (1)    // Set flag indicating this file was included

//
//      Copyright (c) Stratus Technologies Bermuda Ltd., 2008.
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
//      This is state manager header file.
//
//  AUTHORS:
//
//      Keiichi Koyama
//
//  CREATION DATE: March 05, 2008
//
//  DESIGN ISSUES:
//


#include "mtctypes.h"
#include "xapi_mon.h"
#include "state_checker.h"

//
// Status Manager
//
#define COM_ID_SM   "statemanager"

typedef struct _COM_DATA_SM {
    MTC_HOSTMAP         current_liveset;
    MTC_HOSTMAP         proposed_liveset;
//    MTC_HOSTMAP         excluded;
    MTC_FENCING_MODE    fencing;
    MTC_STATUS          status;
    SM_PHASE            phase;
    MTC_BOOLEAN         SR2;

    MTC_HOSTMAP         sf_access;
    MTC_HOSTMAP         sf_corrupted;

    MTC_BOOLEAN         hb_approaching_timeout;
    MTC_BOOLEAN         sf_approaching_timeout;
    MTC_BOOLEAN         xapi_approaching_timeout;

    MTC_U32             weight;             //  Latest weight specified by ha_set_host_weight
    MTC_U32             commited_weight;    //  Committed weight of the local host
} COM_DATA_SM, *PCOM_DATA_SM;


//
// Heartbeat
//
#define COM_ID_HB   "heartbeat"

typedef struct _COM_DATA_HB {
    struct {
        MTC_BOOLEAN enable_HB_send;     // Enable to send heartbeat if TRUE.

        MTC_BOOLEAN enable_HB_receive;  // Discard all received heartbeat if FALSE.
                                        // HB will always receive heartbeat.
        MTC_BOOLEAN join;
        MTC_BOOLEAN fence_request;
    } ctl;

    MTC_S32     latency;                // Interval [msec] between the last two
                                        // heartbeat transmitted from the local
                                        // host.
                                        // -1 if the stat is not available.

    MTC_S32     latency_max;            // Maximum interval [msec].
                                        // -1 if the stat is not available.
                                        // Query_liveset should set -1 when it
                                        // retrieves this data.

    MTC_S32     latency_min;            // Minimum interval [msec].
                                        // -1 if the stat is not available.
                                        // Query_liveset should set -1 when it
                                        // retrieves this data.

    MTC_S64     time_last_HB[MAX_HOST_NUM];
                                        // Time [msec] last received from node x.

    MTC_HOSTMAP hbdomain;               // ON if the host looks active on the heartbeat
    MTC_HOSTMAP notjoining;             // Bit-on if the corresponding host is NOT ready to join.

    RAW_DATA    raw[MAX_HOST_NUM];      // Raw data from node x.
                                        // raw[local_host] shows the last data
                                        // transmitted from the local host.

    MTC_S8      err_string[MAX_HOST_NUM][XAPI_MAX_ERROR_STRING_LEN + 1];
                                        // Xapi error string.
                                        // set if xapi healthcheck failed.
    

    MTC_FENCING_MODE    fencing;        // Fencing mode (NULL->ARMED->DISARM_REQUESTED->DISARMED)
    MTC_BOOLEAN         SF_accelerate;  // 1 if SF acceleration requested
    SM_PHASE            sm_phase[MAX_HOST_NUM];
                                        //
} COM_DATA_HB, *PCOM_DATA_HB;

extern void
start_fh(
    MTC_BOOLEAN force);

extern MTC_STATUS
sm_initialize(
    MTC_S32 phase);

extern void
self_fence(MTC_STATUS code, PMTC_S8 message);

extern MTC_BOOLEAN
sm_get_join_block();

#endif  // SM_H
