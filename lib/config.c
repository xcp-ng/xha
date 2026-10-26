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
//      Config-File reader
//
//  AUTHORS:
//
//      Shinji Matsumoto
//
//  CREATION DATE: 
//
//      Mar 4, 2008
//
//   

//
//
//  O P E R A T I N G   S Y S T E M   I N C L U D E   F I L E S
//
//

#include <libgen.h>
#include <linux/limits.h>
#include <sys/queue.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <libxml2/libxml/xmlmemory.h>
#include <libxml2/libxml/parser.h>


//
//
//  M A R A T H O N   I N C L U D E   F I L E S
//
//

#include "mtctypes.h"
#include "mtcerrno.h"
#include "config.h"
#include "log.h"


//
//
//  L O C A L   D E F I N I T I O N S
//
//


#define VALID_VERSION_STRING "1.0"
typedef MTC_S32 (*XmlConverter)(xmlDocPtr, xmlNodePtr, void*, void*);
struct xmlTagParser
{
    char *name;
    void *value;
    MTC_BOOLEAN is_deprecated;
    XmlConverter converter;
    void* converter_args;
};


//
//
//  F U N C T I O N   D E F I N I T I O N S
//
//
//

//++
//
//  NAME:
//
//      uuid_strcpy
//
//  DESCRIPTION:
//
//      copy uuid string removing '-'
//      check size
//      check character
//      output does not terminate with '\0'
//
//  FORMAL PARAMETERS:
//
//      packed_uuid - output string
//      uuid - input string
//
//  RETURN VALUE:
//
//      0: success
//      -1: failed (too long or too short)
//
//  ENVIRONMENT:
//
//      None
//
//--
//
//


static int 
uuid_strcpy(
    char *packed_uuid,
    char *uuid)
{
    char *src;
    MTC_U32 count;

    count = 0;
    for (src = uuid; *src; src++) 
    {
        if (*src == '-') continue;
        if (count < MTC_UUID_SIZE) 
        {
            packed_uuid[count] = *src;
            count++;
        }
        else
        {
            // too long uuid
            return MTC_ERROR_CF_INVALID_FORMAT;
        }
    }
    if (count < MTC_UUID_SIZE) 
    {
        // too short uuid
        for (; count < MTC_UUID_SIZE; count++)
        {
            packed_uuid[count] = '\0';
        }
        return MTC_ERROR_CF_INVALID_FORMAT;
    }
    return MTC_SUCCESS;
}

static MTC_S32 xml_to_long(xmlDocPtr doc, xmlNodePtr node, void* config, void* args)
{
    char *endptr;
    xmlChar *txt = xmlNodeListGetString(doc, node->children, 1);
    if (txt == NULL) 
    {
        log_internal(MTC_LOG_ERR, "%s: failed to get %s\n", __func__, node->name);
        return MTC_ERROR_CF_INVALID_FORMAT;
    }
    *(MTC_U32*)config = strtol((char *) txt, &endptr, 10);
    if (*endptr != '\0') 
    {
        log_internal(MTC_LOG_ERR, "%s: invalid %s %s\n", __func__, node->name, txt);
        xmlFree(txt);
        return MTC_ERROR_CF_INVALID_FORMAT;
    }
    xmlFree(txt);
    log_internal(MTC_LOG_DEBUG, "%s=%d\n", node->name, *(MTC_U32*) config);
    return MTC_SUCCESS;
}

struct xmlToStringArg
{
    size_t max_length;
};

static MTC_S32 xml_to_string(xmlDocPtr doc, xmlNodePtr node, void* config, void* args)
{
    struct xmlToStringArg* arg = (struct xmlToStringArg*) args;
    xmlChar *txt = xmlNodeListGetString(doc, node->children, 1);
    if (txt == NULL) 
    {
        log_internal(MTC_LOG_ERR, "%s: failed to get %s\n", __func__, node->name);
        return MTC_ERROR_CF_INVALID_FORMAT;
    }

    strncpy((char*) config, (char*) txt, arg->max_length - 1);
    ((char*) config)[arg->max_length - 1] = '\0';
    xmlFree(txt);
    log_internal(MTC_LOG_DEBUG, "%s=%s\n", node->name, (char*) config);
    return MTC_SUCCESS;
}

struct xmlToHostnameArg
{
    MTC_U32 len;
    HA_CONFIG_HOST_INFO *hosts;
};

static MTC_S32 xml_to_hostname(xmlDocPtr doc, xmlNodePtr node, void* config, void* args)
{
    struct xmlToHostnameArg* hostnames = (struct xmlToHostnameArg*) args;
    char host_id[MTC_UUID_SIZE];
    xmlChar *txt = xmlNodeListGetString(doc, node->children, 1);
    if (txt == NULL) 
    {
        log_internal(MTC_LOG_ERR, "%s: failed to get %s\n", __func__, node->name);
        return MTC_ERROR_CF_INVALID_FORMAT;
    }
    if (uuid_strcpy(host_id, (char *) txt) != MTC_SUCCESS)
    {
        log_internal(MTC_LOG_ERR, "%s: invalid HostID %s\n", __func__, txt);
        xmlFree(txt);
        return MTC_ERROR_CF_INVALID_FORMAT;
    }
    for (MTC_U32 i = 0; i < hostnames->len; i++) 
    {
        if (!strncmp(host_id, hostnames->hosts[i].host_id, MTC_UUID_SIZE)) 
        {
            *(MTC_U32*)config = i;
            xmlFree(txt);
            log_internal(MTC_LOG_DEBUG, "%s=%d from %s\n", node->name, *(MTC_U32*)config, hostnames->hosts[i].host_id);
            return MTC_SUCCESS;
        }
    }
    log_internal(MTC_LOG_ERR, "%s: localhost %s is not found in host list\n", __func__, txt);
    xmlFree(txt);
    return MTC_ERROR_CF_INVALID_FORMAT;
}

static MTC_S32 xml_to_cmd_arg(xmlDocPtr doc, xmlNodePtr node, void* config, void *args)
{
    HA_CONFIG_STATE_CHECKER_LOCAL_SCRIPT *cfg = (HA_CONFIG_STATE_CHECKER_LOCAL_SCRIPT*) config;
    struct command_arg *arg = (struct command_arg*) malloc(sizeof(struct command_arg));
    MTC_S32 conversion_status = MTC_SUCCESS;

    if (arg == NULL)
    {
        log_internal(MTC_LOG_ERR, "%s: failed to allocate memory for cmd arg\n", __func__);
        return MTC_ERROR_SYSTEM_LEVEL_FAILURE;
    }
    conversion_status = xml_to_string(doc, node, &arg->arg, (void*)&(struct xmlToStringArg){ .max_length = PATH_MAX });
    if (conversion_status != MTC_SUCCESS){
        free(arg);
        return conversion_status;
    }

    TAILQ_INSERT_TAIL(&cfg->args, arg, args);
    log_internal(MTC_LOG_DEBUG, "%s=%d\n", node->name, *(MTC_U32*) config);
    return MTC_SUCCESS;
}

static MTC_S32 xml_to_exec_path(xmlDocPtr doc, xmlNodePtr node, void* config, void *args)
{

    // Basename of the ExecPath should be inserted as the first element of the arg list
    HA_CONFIG_STATE_CHECKER_LOCAL_SCRIPT *cfg = (HA_CONFIG_STATE_CHECKER_LOCAL_SCRIPT*) config;
    struct command_arg *arg = (struct command_arg*) malloc(sizeof(struct command_arg));
    MTC_S32 status = MTC_SUCCESS;

    if (arg == NULL)
    {
        log_internal(MTC_LOG_ERR, "%s: failed to allocate memory for cmd arg\n", __func__);
        return MTC_ERROR_SYSTEM_LEVEL_FAILURE;
    }

    status = xml_to_string(doc, node, &cfg->executable, (void*)&(struct xmlToStringArg){ .max_length = PATH_MAX });
    if (status != MTC_SUCCESS)
    {
        free(arg);
        return status;
    }

    strncpy((char *) arg->arg, basename((char *) cfg->executable), PATH_MAX);

    // Insert at the top of the args
    TAILQ_INSERT_HEAD(&cfg->args, arg, args);
    log_internal(MTC_LOG_DEBUG, "%s=%d\n", node->name, *(MTC_U32*) config);
    return MTC_SUCCESS;
}

static MTC_S32 walk_tags(xmlDocPtr doc, xmlNodePtr cur, struct xmlTagParser* tags) {

    for (xmlNodePtr sub = cur->children; sub != NULL; sub = sub->next) 
    {
        for (MTC_U32 i = 0; tags[i].name != NULL; i++) 
        {
            if (xmlStrcmp(sub->name, (const xmlChar *) tags[i].name) != 0) 
            {
                continue;
            }

            if (tags[i].converter == NULL)
            {
                log_internal(MTC_LOG_CRIT, "Config option %s can not be converted\n", sub->name);
                return MTC_ERROR_SYSTEM_LEVEL_FAILURE;
            }

            if (tags[i].is_deprecated)
            {
                log_internal(MTC_LOG_WARNING, "Deprecated config option %s\n", sub->name);
            }

            MTC_S32 converter_errror = tags[i].converter(doc, sub, (void*) tags[i].value, tags[i].converter_args);
            if (converter_errror != MTC_SUCCESS)
            {
                return converter_errror;
            }
            break;
        }
    }
    return MTC_SUCCESS;
}

//
//++
//
//  NAME:
//
//      set_default_value
//
//  DESCRIPTION:
//
//      set default value for HA_CONFIG
//
//  FORMAL PARAMETERS:
//
//      c - pointer to HA_CONFIG
//
//  RETURN VALUE:
//
//      None
//
//  ENVIRONMENT:
//
//      None
//
//--
//


static void set_default_value(HA_CONFIG *c)
{
    c->common.heartbeat_interval = HEARTBEAT_INTERVAL_DEFAULT;
    c->common.heartbeat_timeout = HEARTBEAT_TIMEOUT_DEFAULT;
    c->common.heartbeat_watchdog_timeout = HEARTBEAT_WATCHDOG_TIMEOUT_DEFAULT;
    c->common.boot_join_timeout = BOOT_JOIN_TIMEOUT_DEFAULT;
    c->common.enable_join_timeout = ENABLE_JOIN_TIMEOUT_DEFAULT;
    c->common.xapi_healthcheck_interval = XAPI_HEALTH_CHECK_INTERVAL_DEFAULT;
    c->common.xapi_healthcheck_timeout = XAPI_HEALTH_CHECK_TIMEOUT_DEFAULT;
    c->common.xapi_restart_attempts = XAPI_RESTART_ATTEMPTS_DEFAULT;
    c->common.xapi_restart_timeout = XAPI_RESTART_TIMEOUT_DEFAULT;
    c->common.xapi_licensecheck_timeout = XAPI_LICENSE_CHECK_TIMEOUT;
    c->common.state_checker.interval = STATE_CHECKER_INTERVAL_DEFAULT;
    c->common.state_checker.timeout = STATE_CHECKER_TIMEOUT_DEFAULT;
    c->common.state_checker.watchdog_timeout = STATE_CHECKER_WATCHDOG_TIMEOUT_DEFAULT;
    c->local.state_checker.type = UNSET;
}

//
//++
//
//  NAME:
//
//      free_config
//
//  DESCRIPTION:
//
//      free datastructures allocated inside HA_CONFIG
//
//  FORMAL PARAMETERS:
//
//      conf - pointer to the HA_CONFIG
//
//  RETURN VALUE:
//
//      None
//
//  ENVIRONMENT:
//
//      None
//
//--
//

void free_config(HA_CONFIG *conf)
{
    if (conf->local.state_checker.type == SCRIPT)
    {
        struct command_arg *current = TAILQ_FIRST(&conf->local.state_checker.script.args);
        while (current != NULL)
        {
            struct command_arg *next = TAILQ_NEXT(current, args);
            free(current);
            current = next;
        }
        TAILQ_INIT(&conf->local.state_checker.script.args);
    }
}

//
//++
//
//  NAME:
//
//      valid_version
//
//  DESCRIPTION:
//
//      check version string match or not
//
//  FORMAL PARAMETERS:
//
//      version - version string of xhad-config
//
//  RETURN VALUE:
//
//      TRUE: valid
//      FALSE: invalid
//
//  ENVIRONMENT:
//
//      None
//
//--
//


static MTC_S32 
valid_version(
    char *version)
{
    if (strlen(version) >= sizeof(((HA_CONFIG *)0)->version)) 
    {
        return FALSE;
    }
    if (strcmp(version, VALID_VERSION_STRING) != 0) 
    {
        return FALSE;
    }

    return TRUE;
}

//
//++
//
//  NAME:
//
//      valid_config
//
//  DESCRIPTION:
//
//      check HA_CONFIG data structure
//      if the all required parameter is set or not.
//
//  FORMAL PARAMETERS:
//
//      c - HA_CONFIG
//
//  RETURN VALUE:
//
//      TRUE: valid
//      FALSE: invalid
//
//  ENVIRONMENT:
//
//      None
//
//--
//

static MTC_S32 
valid_config(
    HA_CONFIG *c) 
{
    if (c->common.generation_uuid[0] == '\0') 
    {
        log_internal(MTC_LOG_ERR, "%s: GenerationUUID is not set\n", __func__);
        return FALSE;
    }
    if (c->common.udp_port == 0) 
    {
        log_internal(MTC_LOG_ERR, "%s: UDPport is not set\n", __func__);
        return FALSE;
    }
    if (c->common.hostnum == 0) 
    {
        log_internal(MTC_LOG_ERR, "%s: host num is 0\n", __func__);
        return FALSE;
    }
    if (c->local.heartbeat_interface[0] == '\0') 
    {
        log_internal(MTC_LOG_ERR, "%s: HeartbeatInterface is not set\n", __func__);
        return FALSE;
    }
    if (c->local.state_checker.type == STATEFILE)
    {
        if (c->local.state_checker.statefile.path[0] == '\0') 
        {
            log_internal(MTC_LOG_ERR, "%s: StateFile is not set\n", __func__);
            return FALSE;
        }
    }
    else if (c->local.state_checker.type == SCRIPT)
    {
        log_internal(MTC_LOG_ERR, "%s: Script is not supported yet\n", __func__);
        return FALSE;
    }
    else {
        log_internal(MTC_LOG_ERR, "%s: Unsupported StateChecker type %d\n", __func__, c->local.state_checker.type);
        return FALSE;
    }
    if (c->local.localhost_index >= MAX_HOST_NUM || c->local.localhost_index < 0) 
    {
        log_internal(MTC_LOG_ERR, "%s: localhost.HostID is not set\n", __func__);
        return FALSE;
    }

    return TRUE;
}

//
//++
//
//  NAME:
//
//      host_info_compar
//
//  DESCRIPTION:
//
//      lexicographical compare of host_id
//      This is callback function for qsort
//
//  FORMAL PARAMETERS:
//
//      h1 - host info 1
//      h2 - host info 2
//
//  RETURN VALUE:
//
//      -1: h1 is less than h2 in lexicographical order.
//       1: h1 is more than h2 in lexicographical order.
//
//  ENVIRONMENT:
//
//      None
//
//--
//


static int 
host_info_compar(
    const void *h1, 
    const void *h2) 
{
    HA_CONFIG_HOST_INFO *host1, *host2;

    host1 = (HA_CONFIG_HOST_INFO *) h1;
    host2 = (HA_CONFIG_HOST_INFO *) h2;

    return strncmp(host1->host_id, host2->host_id, MTC_UUID_SIZE);
}


//
//++
//
//  NAME:
//
//      walk_host_config
//
//  DESCRIPTION:
//
//      walk host subtree and set HA_CONFIG
//
//  FORMAL PARAMETERS:
//
//      doc - pointer for xml doc
//      cur - xml node for host element
//      c - HA_CONFIG
//
//  RETURN VALUE:
//
//      0: success
//      other: failed
//
//  ENVIRONMENT:
//
//      None
//
//--
//

static MTC_S32 
walk_host_config(
    xmlDocPtr doc,
    xmlNodePtr cur,
    HA_CONFIG *c) 
{
    xmlNodePtr sub;
    xmlChar *txt;

    //
    // Walk XML
    //

    if (c->common.hostnum >= MAX_HOST_NUM) 
    {
        log_internal(MTC_LOG_ERR,
                    "%s: hostnum(%d) exceeds MAX_HOST_NUM(%d)\n",
                    __func__, c->common.hostnum, MAX_HOST_NUM);
        return MTC_ERROR_CF_INVALID_FORMAT;
    }

    for (sub = cur->children; sub != NULL; sub = sub->next) 
    {

        //
        // HostID
        //
        //

        if (xmlStrcmp(sub->name, (const xmlChar *) "HostID") == 0) 
        {
            txt = xmlNodeListGetString(doc, sub->children, 1);
            if (txt == NULL) 
            {
                log_internal(MTC_LOG_ERR, "%s: failed to get HostID\n", __func__);
                return MTC_ERROR_CF_INVALID_FORMAT;
            }
            if (uuid_strcpy(c->common.host[c->common.hostnum].host_id, (char *) txt) < 0) {
                log_internal(MTC_LOG_ERR, "%s: invalid HostID %s\n", __func__, txt);

                xmlFree(txt);
                return MTC_ERROR_CF_INVALID_FORMAT;
            }
            xmlFree(txt);
        }

        //
        // IPaddress
        //

        else if (xmlStrcmp(sub->name, (const xmlChar *) "IPaddress") == 0) 
        {
            txt = xmlNodeListGetString(doc, sub->children, 1);
            if (txt == NULL) 
            {
                log_internal(MTC_LOG_ERR, "%s: failed to get IPaddress\n", __func__);
                return MTC_ERROR_CF_INVALID_FORMAT;
            }

            const char *ip_address = (char *) txt;

            struct addrinfo hints;
            struct addrinfo *result;
            memset(&hints, 0, sizeof hints);
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_DGRAM;
            hints.ai_flags = AI_NUMERICHOST;
            hints.ai_protocol = 0;
            hints.ai_canonname = NULL;
            hints.ai_addr = NULL;
            hints.ai_next = NULL;

            errno = 0;
            const int ret = getaddrinfo(ip_address, NULL, &hints, &result);
            if (ret)
            {
                log_internal(MTC_LOG_ERR, "%s: failed to get addr info of `%s` (info: %s). (sys %d)\n",
                    __func__, ip_address, gai_strerror(ret), errno);
                xmlFree(txt);
                return MTC_ERROR_CF_INVALID_FORMAT;
            }

            xmlFree(txt);

            socket_address *ss = &c->common.host[c->common.hostnum].sock_address;
            switch (result->ai_family)
            {
                case AF_INET: {
                    struct sockaddr_in *sa = &ss->sa_in;
                    memcpy(&sa->sin_addr, &((struct sockaddr_in *) result->ai_addr)->sin_addr, sizeof sa->sin_addr);
                    sa->sin_family = AF_INET;
                    break;
                }
                case AF_INET6: {
                    struct sockaddr_in6 *sa6 = &ss->sa_in6;
                    memcpy(&sa6->sin6_addr, &((struct sockaddr_in6 *) result->ai_addr)->sin6_addr, sizeof sa6->sin6_addr);
                    sa6->sin6_family = AF_INET6;
                    break;
                }
                default:
                    log_internal(MTC_LOG_ERR, "%s: Unsupported address type: %d\n", __func__, result->ai_family);
                    freeaddrinfo(result);
                    return MTC_ERROR_CF_INVALID_FORMAT;
            }

            freeaddrinfo(result);
        }
    }
    c->common.hostnum++;
    return MTC_SUCCESS; // SUCCESS
}

//
//++
//
//  NAME:
//
//      walk_common_state_checker
//
//  DESCRIPTION:
//
//      walk host subtree and set HA_CONFIG_STATE_CHECKER_COMMON
//
//  FORMAL PARAMETERS:
//
//      doc    - pointer for xml doc
//      cur    - xml node for host element
//      config - HA_CONFIG_STATE_CHECKER_COMMON
//      args   - ignored
//
//  RETURN VALUE:
//
//      0: success
//      other: failed
//
//  ENVIRONMENT:
//
//      None
//
//--
//

static MTC_S32 walk_common_state_checker(xmlDocPtr doc, xmlNodePtr cur, void *config, void *args)
{
    struct ha_config_state_checker_common *checker = (struct ha_config_state_checker_common*) config;
    struct xmlTagParser tags[] = {
        {"Interval", &(checker->interval), FALSE, xml_to_long, NULL},
        {"Timeout", &(checker->timeout), FALSE, xml_to_long, NULL},
        {"WatchdogTimeout", &(checker->watchdog_timeout), FALSE, xml_to_long, NULL},
        {NULL, NULL, FALSE, NULL, NULL}
    };
    return walk_tags(doc, cur, tags);
}

//
//++
//
//  NAME:
//
//      walk_parameters_config
//
//  DESCRIPTION:
//
//      walk parameters subtree and set HA_CONFIG
//
//  FORMAL PARAMETERS:
//
//      doc - pointer for xml doc
//      cur - xml node for parameters element
//      c - HA_CONFIG
//
//  RETURN VALUE:
//
//      0: success
//      other: failed
//
//  ENVIRONMENT:
//
//      None
//
//--
//

static MTC_S32 
walk_parameters_config(
    xmlDocPtr doc,
    xmlNodePtr cur,
    HA_CONFIG *c) 
{
    struct xmlTagParser tags[] = 
    {
        {"HeartbeatInterval", &(c->common.heartbeat_interval), FALSE, xml_to_long, NULL},
        {"HeartbeatTimeout", &(c->common.heartbeat_timeout), FALSE, xml_to_long, NULL},
        {"StateFileInterval", &(c->common.state_checker.interval), TRUE, xml_to_long, NULL},
        {"StateFileTimeout", &(c->common.state_checker.timeout), TRUE, xml_to_long, NULL},
        {"HeartbeatWatchdogTimeout", &(c->common.heartbeat_watchdog_timeout), FALSE, xml_to_long, NULL},
        {"StateFileWatchdogTimeout", &(c->common.state_checker.watchdog_timeout), TRUE, xml_to_long, NULL},
        {"BootJoinTimeout", &(c->common.boot_join_timeout), FALSE, xml_to_long, NULL},
        {"EnableJoinTimeout", &(c->common.enable_join_timeout), FALSE, xml_to_long, NULL},
        {"XapiHealthCheckInterval", &(c->common.xapi_healthcheck_interval), FALSE, xml_to_long, NULL},
        {"XapiHealthCheckTimeout", &(c->common.xapi_healthcheck_timeout), FALSE, xml_to_long, NULL},
        {"XapiRestartAttempts", &(c->common.xapi_restart_attempts), FALSE, xml_to_long, NULL},
        {"XapiRestartTimeout", &(c->common.xapi_restart_timeout), FALSE, xml_to_long, NULL},
        {"XapiLicenseCheckTimeout", &(c->common.xapi_licensecheck_timeout), FALSE, xml_to_long, NULL},
        {"state-checker", &(c->common.state_checker), FALSE, walk_common_state_checker, NULL},
        {NULL, NULL, FALSE, NULL, NULL}
    };
    return walk_tags(doc, cur, tags);
}


//
//++
//
//  NAME:
//
//      walk_local_state_checker_statefile_legacy
//
//  DESCRIPTION:
//
//      extract config for legacy StateFile argument
//
//  FORMAL PARAMETERS:
//
//      doc - pointer for xml doc
//      node - xml node for parameters element
//      config - HA_CONFIG_STATE_CHECKER_LOCAL*
//      args - ignored
//
//  RETURN VALUE:
//
//      0: success
//      other: failed
//
//  ENVIRONMENT:
//
//      None
//
//--
//

static MTC_S32 walk_local_state_checker_statefile_legacy(xmlDocPtr doc, xmlNodePtr node, void* config, void *args)
{
    HA_CONFIG_STATE_CHECKER_LOCAL *cfg = (HA_CONFIG_STATE_CHECKER_LOCAL*) config;
    cfg->type = STATEFILE;
    return xml_to_string(doc, node, &cfg->statefile.path, (void*)&(struct xmlToStringArg){ .max_length = PATH_MAX });
}

//
//++
//
//  NAME:
//
//      walk_local_state_checker_statefile
//
//  DESCRIPTION:
//
//      extract config for state-checker node of type STATEFILE
//
//  FORMAL PARAMETERS:
//
//      doc - pointer for xml doc
//      node - xml node for parameters element
//      config - HA_CONFIG_STATE_CHECKER_LOCAL*
//      args - ignored
//
//  RETURN VALUE:
//
//      0: success
//      other: failed
//
//  ENVIRONMENT:
//
//      None
//
//--
//

static MTC_S32 walk_local_state_checker_statefile(xmlDocPtr doc, xmlNodePtr node, void* config, void *args)
{
    HA_CONFIG_STATE_CHECKER_LOCAL *cfg = (HA_CONFIG_STATE_CHECKER_LOCAL*) config;
    cfg->type = STATEFILE;
    struct xmlTagParser tags[] =
    {
        {"Path", &cfg->statefile.path, FALSE, xml_to_string, (void*)&(struct xmlToStringArg){ .max_length = PATH_MAX }},
        {NULL, NULL, FALSE, NULL, NULL}
    };

    return walk_tags(doc, node, tags);
}

//
//++
//
//  NAME:
//
//      walk_local_state_checker_script
//
//  DESCRIPTION:
//
//      extract config for state-checker node of type SCRIPT
//
//  FORMAL PARAMETERS:
//
//      doc - pointer for xml doc
//      node - xml node for parameters element
//      config - HA_CONFIG_STATE_CHECKER_LOCAL*
//      args - ignored
//
//  RETURN VALUE:
//
//      0: success
//      other: failed
//
//  ENVIRONMENT:
//
//      None
//
//--
//


static MTC_S32 walk_local_state_checker_script(xmlDocPtr doc, xmlNodePtr node, void* config, void *args)
{

    HA_CONFIG_STATE_CHECKER_LOCAL *cfg = (HA_CONFIG_STATE_CHECKER_LOCAL*) config;

    // Init
    cfg->type = SCRIPT;
    TAILQ_INIT(&cfg->script.args);
    cfg->script.working_directory[0] = '\0';

    struct xmlTagParser tags[] =
    {
        {"ExecPath", &cfg->script, FALSE, xml_to_exec_path, NULL},
        {"WorkingDir", &cfg->script.working_directory, FALSE, xml_to_string, (void*)&(struct xmlToStringArg){ .max_length = PATH_MAX }},
        {"Arg", &cfg->script, FALSE, xml_to_cmd_arg, NULL},
        {NULL, NULL, FALSE, NULL, NULL}
    };
    return walk_tags(doc, node, tags);
}

//
//++
//
//  NAME:
//
//      walk_local_state_checker
//
//  DESCRIPTION:
//
//      walk parameters subtree for state-checker node of any type
//
//  FORMAL PARAMETERS:
//
//      doc - pointer for xml doc
//      node - xml node for parameters element
//      config - HA_CONFIG_STATE_CHECKER_LOCAL*
//
//  RETURN VALUE:
//
//      0: success
//      other: failed
//
//  ENVIRONMENT:
//
//      None
//
//--
//

static MTC_S32 walk_local_state_checker(xmlDocPtr doc, xmlNodePtr node, void* config, void *args)
{
    xmlChar *type = xmlGetProp(node, (const xmlChar *) "type");
    if (type == NULL)
    {
        log_internal(MTC_LOG_ERR, "%s: failed to get type property for %s\n", __func__, node->name);
        return MTC_ERROR_CF_INVALID_FORMAT;
    }

    if (xmlStrEqual(type, (const xmlChar *) "script"))
    {
        xmlFree(type);
        return walk_local_state_checker_script(doc, node, config, args);
    }
    if (xmlStrEqual(type, (const xmlChar *) "statefile"))
    {
        xmlFree(type);
        return walk_local_state_checker_statefile(doc, node, config, args);
    }
    log_internal(MTC_LOG_ERR, "%s: invalid type property %s for %s\n", __func__, type, node->name);

    xmlFree(type);
    return MTC_ERROR_CF_INVALID_FORMAT;
}

//
//++
//
//  NAME:
//
//      walk_localhost
//
//  DESCRIPTION:
//
//      walk localhost subtree and set HA_CONFIG
//
//  FORMAL PARAMETERS:
//
//      doc - pointer for xml doc
//      cur - xml node for localhost element
//      c - HA_CONFIG
//
//  RETURN VALUE:
//
//      0: success
//      other: failed
//
//  ENVIRONMENT:
//
//      None
//
//--
//

static MTC_S32 
walk_localhost(
    xmlDocPtr doc,
    xmlNodePtr cur,
    HA_CONFIG *c) 
{

    struct xmlTagParser tags[] =
    {
        {"HostID", &c->local.localhost_index, FALSE, xml_to_hostname, (void*)&(struct xmlToHostnameArg){ .len = c->common.hostnum, .hosts = c->common.host }},
        {"HeartbeatInterface", &c->local.heartbeat_interface, FALSE, xml_to_string, (void*)&(struct xmlToStringArg){ .max_length = HEARTBEAT_INTERFACE_LEN }},
        {"HeartbeatPhysicalInterface", &c->local.heartbeat_physical_interface, FALSE, xml_to_string, (void*)&(struct xmlToStringArg){ .max_length = HEARTBEAT_INTERFACE_LEN }},
        {"StateFile", &c->local.state_checker, TRUE, walk_local_state_checker_statefile_legacy, NULL},
        {"WatchdogMode", &c->local.watchdog_mode, FALSE, xml_to_string, (void*)&(struct xmlToStringArg){ .max_length = WATCHDOG_MODE_LEN }},
        {"state-checker", &c->local.state_checker, FALSE, walk_local_state_checker, NULL},
        {NULL, NULL, FALSE, NULL, NULL}
    };

    return walk_tags(doc, cur, tags);
}


//
//++
//
//  NAME:
//
//      walk_common_config
//
//  DESCRIPTION:
//
//      walk common_config subtree and set HA_CONFIG
//
//  FORMAL PARAMETERS:
//
//      doc - pointer for xml doc
//      cur - xml node for common-config element
//      c - HA_CONFIG
//
//  RETURN VALUE:
//
//      0: success
//      other: failed
//
//  ENVIRONMENT:
//
//      None
//
//--
//


static MTC_S32 
walk_common_config(
    xmlDocPtr doc,
    xmlNodePtr cur,
    HA_CONFIG *c) 
{
    xmlNodePtr sub;
    xmlChar *txt;
    MTC_S32 ret;

    //
    // Walk XML
    //

    for (sub = cur->children; sub != NULL; sub = sub->next) 
    {
        if (xmlStrcmp(sub->name, (const xmlChar *) "GenerationUUID") == 0) 
        {
            txt = xmlNodeListGetString(doc, sub->children, 1);
            if (txt == NULL) 
            {
                log_internal(MTC_LOG_ERR, "%s: failed to get GenerationUUID\n", __func__);
                return MTC_ERROR_CF_INVALID_FORMAT;
            }
            if (uuid_strcpy(c->common.generation_uuid, (char *) txt) != MTC_SUCCESS) 
            {
                log_internal(MTC_LOG_ERR, "%s: invalid GenerationUUID %s\n", __func__, txt);
                xmlFree(txt);
                return MTC_ERROR_CF_INVALID_FORMAT;
            }
            xmlFree(txt);
        }
        else if (xmlStrcmp(sub->name, (const xmlChar *) "UDPport") == 0) 
        {
            char *endptr;

            txt = xmlNodeListGetString(doc, sub->children, 1);
            if (txt == NULL) 
            {
                log_internal(MTC_LOG_ERR, "%s: failed to get UDPport\n", __func__);
                return MTC_ERROR_CF_INVALID_FORMAT;
            }
            c->common.udp_port = strtol((char *) txt, &endptr, 10);
            if (*endptr != '\0' || c->common.udp_port == 0) 
            {
                log_internal(MTC_LOG_ERR, "%s: invalid UDPport %s\n", __func__, txt);
                xmlFree(txt);
                return MTC_ERROR_CF_INVALID_FORMAT;
            }
            xmlFree(txt);
        }
        else if (xmlStrcmp(sub->name, (const xmlChar *) "host") == 0) 
        {
            if ((ret = walk_host_config(doc, sub, c)) != MTC_SUCCESS) 
            {
                return ret;
            }
        }
        else if (xmlStrcmp(sub->name, (const xmlChar *) "parameters") == 0) 
        {
            if ((ret = walk_parameters_config(doc, sub, c)) != MTC_SUCCESS) 
            {
                return ret;
            }
        }
    }

    qsort(c->common.host, (size_t) c->common.hostnum, 
          sizeof(HA_CONFIG_HOST_INFO), host_info_compar);

    return MTC_SUCCESS; // SUCCESS
}

//
//++
//
//  NAME:
//
//      walk_local_config
//
//  DESCRIPTION:
//
//      walk local_config subtree and set HA_CONFIG
//
//  FORMAL PARAMETERS:
//
//      doc - pointer for xml doc
//      cur - xml node for local-config element
//      c - HA_CONFIG
//
//  RETURN VALUE:
//
//      0: success
//      other: failed
//
//  ENVIRONMENT:
//
//      None
//
//--
//


static MTC_S32
walk_local_config(
    xmlDocPtr doc,
    xmlNodePtr cur,
    HA_CONFIG *c) 
{
    xmlNodePtr sub;
    MTC_S32 ret;

    //
    // Walk XML
    //

    for (sub = cur->children; sub != NULL; sub = sub->next) 
    {
        if (xmlStrcmp(sub->name, (const xmlChar *) "localhost") == 0) 
        {
            if ((ret = walk_localhost(doc, sub, c)) != MTC_SUCCESS) 
            {
                return ret;
            }
        }
    }
    return MTC_SUCCESS; // SUCCESS
}


//
//++
//
//  NAME:
//
//      interpret_config_file
//
//  DESCRIPTION:
//
//      interpret config-file and set HA_CONFIG
//
//  FORMAL PARAMETERS:
//
//      path - filepath of config-file
//      c - HA_CONFIG
//
//  RETURN VALUE:
//
//      0: success
//      other: failed
//
//  ENVIRONMENT:
//
//      None
//
//--
//

MTC_S32 
interpret_config_file(
    MTC_S8 *path,
    HA_CONFIG *c)
{
    xmlChar *txt;
    xmlDocPtr doc;
    xmlNodePtr cur, sub;
    MTC_S32 ret;
    struct stat buf;

    //
    // parameter check
    //

    if (path == NULL) 
    {
        log_internal(MTC_LOG_ERR, "%s: config-path is NULL\n", __func__);
        return MTC_ERROR_CF_OPEN;
    }

    if (c == NULL) 
    {
        log_internal(MTC_LOG_ERR, "%s: HA_CONFIG is NULL\n", __func__);
        return MTC_ERROR_CF_INVALID_PARAMETER;
    }

    //
    // file check
    //

    if (stat(path, &buf) != 0) 
    {
        log_message(MTC_LOG_ERR, "CF: config-file (%s) not found\n", path);
        return MTC_ERROR_CF_OPEN;
    }

    //
    // fill default value
    //

    set_default_value(c);
    
    //
    // poison host_index
    //

    c->local.localhost_index = (MTC_U32) -1;

    if ((doc = xmlParseFile(path)) == NULL) 
    {
        log_message(MTC_LOG_ERR, "CF: config-file (%s) contains invalid XML text.\n", path);
        return MTC_ERROR_CF_INVALID_FORMAT;
    }

    //
    // get root element and check version
    //

    if ((cur = xmlDocGetRootElement(doc)) == NULL) 
    {
        log_message(MTC_LOG_ERR, "CF: config-file (%s) contains invalid data.\n", path);
        log_internal(MTC_LOG_ERR, "%s: failed to xmlDocGetRootElement %s\n", __func__, path);
        xmlFreeDoc(doc);
        return MTC_ERROR_CF_INVALID_FORMAT;
        
    }
    if (xmlStrcmp( cur->name, (const xmlChar *) "xhad-config")) 
    {
        log_message(MTC_LOG_ERR, "CF: config-file (%s) contains invalid data.\n", path);
        log_internal(MTC_LOG_ERR, "%s: name of root element is invalid\n", __func__);
        xmlFreeDoc(doc);
        return MTC_ERROR_CF_INVALID_FORMAT;
    }

    if ((txt = xmlGetProp(cur, (const xmlChar *) "version")) == NULL) 
    {
        // attribute "version" not found
        log_message(MTC_LOG_ERR, "CF: config file (%s) contains invalid data.\n", path);
        log_internal(MTC_LOG_ERR, "%s: failed to get version\n", __func__);

        xmlFreeDoc(doc);
        return MTC_ERROR_CF_INVALID_FORMAT;
    }
    if (!valid_version((char *) txt)) 
    {
        log_message(MTC_LOG_ERR, "CF: config-file (%s) version mismatch.\n", path);
        log_internal(MTC_LOG_ERR, "%s: version %s is invalid\n", __func__, txt);

        xmlFree(txt);
        xmlFreeDoc(doc);
        return MTC_ERROR_CF_INVALID_FORMAT;
    }
    strcpy(c->version, (char *) txt);
    xmlFree(txt);
    
    //
    // Walk XML
    //

    for (sub = cur->children; sub != NULL; sub = sub->next) 
    {
        if (xmlStrcmp(sub->name, (const xmlChar *) "common-config") == 0) 
        {
            if ((ret = walk_common_config(doc, sub, c)) != MTC_SUCCESS) 
            {
                log_message(MTC_LOG_ERR, "CF: config-file (%s) contains invalid data.\n", path);
                xmlFreeDoc(doc);
                return ret;
            }
        }
        else if (xmlStrcmp(sub->name, (const xmlChar *) "local-config") == 0) 
        {
            if ((ret = walk_local_config(doc, sub, c)) != MTC_SUCCESS) 
            {
                log_message(MTC_LOG_ERR, "CF: config file (%s) contains invalid data.\n", path);
                xmlFreeDoc(doc);
                return ret;
            }
        }
    }

    //
    // check output
    //

    if (!valid_config(c)) 
    {
        log_message(MTC_LOG_ERR, "CF: config-file (%s) contains invalid data.\n", path);
        xmlFreeDoc(doc);
        return MTC_ERROR_CF_INVALID_FORMAT;
    }
    xmlFreeDoc(doc);
    return MTC_SUCCESS; // SUCCESS
}

