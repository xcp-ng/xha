#ifndef STATE_CHECKER_H
#define STATE_CHECKER_H

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

typedef enum {
    FENCING_NONE,
    FENCING_ARMED,
    FENCING_DISARM_REQUESTED,
    FENCING_DISARMED,
} MTC_FENCING_MODE, *PMTC_FENCING_MODE;

typedef enum {
    SM_PHASE_STARTING,  // 0
    SM_PHASE_STARTED,   // 1
    SM_PHASE_ABORTED,   // 2
    SM_PHASE_FHREADY,   // 3
    SM_PHASE_FH1,       // 4
    SM_PHASE_FH1DONE,   // 5
    SM_PHASE_FH2,       // 6
    SM_PHASE_FH2DONE,   // 7
    SM_PHASE_FH3,       // 8
    SM_PHASE_FH3DONE,   // 9
    SM_PHASE_FH4,       // 10
    SM_PHASE_FH4DONE,   // 11
} SM_PHASE, *PSM_PHASE;



//
// common data structure for HB, SF, SM
//
typedef struct _RAW_DATA {
    MTC_HOSTMAP current_liveset;        // Current liveset as seen by this host
    MTC_HOSTMAP proposed_liveset;       // Proposed liveset by this host
    MTC_HOSTMAP hbdomain;               // Current host set on the heartbeat
    MTC_HOSTMAP sfdomain;               // Current host set on the state file

    MTC_S32     time_since_last_HB_receipt[MAX_HOST_NUM];
                                        // Set of time [msec] since last HB
                                        // receipt, received from node x.

    MTC_S32     time_since_last_SF_update[MAX_HOST_NUM];
                                        // Set of time [msec] since last SF
                                        // update, received from node x.

    MTC_S32     time_since_xapi_restart;
                                        // Set of time [msec] since xapi
                                        // restart received from node x.
} RAW_DATA, *PRAW_DATA;




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

//
// StateFile (To be hopefully removed / moved to statefile.h)
//
#define COM_ID_SF   "statefile"

typedef struct _COM_DATA_SF
{
    MTC_U32 modified_mask;                  // Fields designated by the mask have been
                                            // updated on memory (in this structure),
                                            // but not written to the State-File.
                                            // modified_mask is only for internal use by SF.
#define SF_MODIFIED_MASK_NONE           (0)
#define SF_MODIFIED_MASK_POOL_STATE     (1 << 0)
#define SF_MODIFIED_MASK_MASTER         (1 << 1)
#define SF_MODIFIED_MASK_EXCLUDED       (1 << 2)

    //  Set by the State Manager (sm)
    //  Referenced by the State-File handler (sf)
    
    struct {
        MTC_BOOLEAN enable_SF_write;
        MTC_BOOLEAN enable_SF_read;
        MTC_BOOLEAN starting;
    } ctl;

    //  Set and referenced by the State-File handler (sf)
    //  and by the lock manager (lm)

    struct {
        MTC_BOOLEAN request;                // TRUE if this host has requested a lock
        MTC_HOSTMAP grant;                  // MTC_HOSTMAP_ISON(grant, Y) is TRUE
                                            // if this host has granted a lock to the host Y
    } lm[MAX_HOST_NUM];

    MTC_U32     pool_state;                 // Pool state as written in the SF global section.
    MTC_UUID    master;                     // Master's host UUID as written in the SF global section.
    MTC_BOOLEAN SF_access;                  // TRUE if if the local host has access to the SF
    MTC_BOOLEAN SF_corrupted;               // TRUE if SF corruption is detected. SF_access turns to FALSE as well.
    MTC_S64     time_last_SF[MAX_HOST_NUM]; // Milliseconds since the last update to the State-File
                                            // was seen from each host.
    MTC_HOSTMAP excluded;                   // Bit-on if the corresponding host has been excluded.
    MTC_HOSTMAP starting;                   // Bit-on if the corresponding host is starting.
    RAW_DATA    raw[MAX_HOST_NUM];          // Array of raw data as seen by each host

    MTC_S32 latency;                        // State-Fie access latency in ms (latest)
    MTC_S32 latency_max;                    // State-Fie access latency in ms (max since the last query_liveset)
    MTC_S32 latency_min;                    // State-Fie access latency in ms (min since the last query_liveset)

    MTC_HOSTMAP sfdomain;                   // ON if the host looks active on the State File
    MTC_FENCING_MODE fencing;               // Fencing mode (NULL->ARMED->DISARM_REQUESTED->DISARMED)

    SM_PHASE    sm_phase[MAX_HOST_NUM];     // Current SM phase of each host
    MTC_U32     weight[MAX_HOST_NUM];       // Weight committed by each host
} COM_DATA_SF, *PCOM_DATA_SF;

#endif // STATE_CHECKER_H