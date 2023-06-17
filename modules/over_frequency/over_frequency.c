/*
 * MAXFWD module
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of opensips, a free SIP server.
 *
 * opensips is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * opensips is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 *
 * History:
 * --------
 *  2003-03-11  updated to the new module interface (andrei)
 *  2003-03-16  flags export parameter added (janakj)
 *  2003-03-19  all mallocs/frees replaced w/ pkg_malloc/pkg_free (andrei)
 *  2004-08-15  max value of max-fwd header is configurable via max_limit
 *              module param (bogdan)
 *  2005-09-15  max_limit param cannot be disabled anymore (according to RFC)
 *              (bogdan)
 *  2005-11-03  is_maxfwd_lt() function added; MF value saved in
 *              msg->maxforwards->parsed (bogdan)
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include "../../sr_module.h"
#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../lock_ops.h"
#include "../../dprint.h"
#include "../../str.h"
#include "../../pvar.h"
#include "../../error.h"
#include "../../timer.h"
#include "../../resolve.h"
#include "../../data_lump.h"
#include "../../mod_fix.h"
#include "../../script_cb.h"
#include "../../sl_cb.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_expires.h"
#include "../../parser/contact/parse_contact.h"
#include "../../cachedb/cachedb.h"
#include "../../cachedb/cachedb_cap.h"
#include "../dialog/dlg_load.h"
#include "../tm/tm_load.h"


#include "../../ut.h"



static unsigned int max_frequency;
static unsigned int of_interval;
static int CheckFrequency(struct sip_msg *msg, unsigned int);

static int mod_init(void);


static cmd_export_t cmds[]={
	
	{"check_frequency", (cmd_function)CheckFrequency, {
		{CMD_PARAM_INT, 0, 0}, {0,0,0}},
		REQUEST_ROUTE},
	{0,0,{{0,0,0}},0}
};

static param_export_t params[]={
	{"max_frequency", INT_PARAM, &max_frequency},
    {"time_interval", INT_PARAM, &of_interval},
	{0,0,0}
};

static dep_export_t deps = {
    {
        /* OpenSIPS module dependencies */
        {MOD_TYPE_CACHEDB, "cachedb_local", DEP_ABORT},
        {MOD_TYPE_NULL, NULL, 0},
    },
    {
        /* modparam dependencies */
        {NULL, NULL},
    },
};



#ifdef STATIC_MAXFWD
struct module_exports maxfwd_exports = {
#else
struct module_exports exports= {
#endif
	"maxfwd",
	MOD_TYPE_DEFAULT,/* class of this module */
	MODULE_VERSION,
	DEFAULT_DLFLAGS, /* dlopen flags */
	0,				 /* load function */
	&deps,            /* OpenSIPS module dependencies */
	cmds,
	0,
	params,
	0,          /* exported statistics */
	0,          /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,			/* exported transformations */
	0,          /* extra processes */
	0,          /* pre-init function */
	mod_init,
	(response_function) 0,
	(destroy_function) 0,
	0,          /* per-child init function */
	0           /* reload confirm function */
};

/* local cache */
cachedb_funcs overf_cdbf;
cachedb_con *overf_cache;

static int mod_init(void)
{
	LM_INFO("initializing...\n");
	
	str local_url = {"local://", 8};
    if (cachedb_bind_mod(&local_url, &overf_cdbf) < 0)
    {
        LM_ERR("cannot bind functions for cachedb_url %.*s\n", local_url.len, local_url.s);
        return -1;
    }
    overf_cache = overf_cdbf.init(&local_url);
    if (!overf_cache)
    {
        LM_ERR("cannot connect to cachedb_url %.*s\n", local_url.len, local_url.s);
        return -1;
    }
	return 0;
}







enum {
    DIRE_CALLER = 1U << 0,
    DIRE_CALLEE = 1U << 1
};

static int CheckFrequency(struct sip_msg *msg, unsigned int direction)
{
    struct sip_uri *uri = NULL;
    int val;

    // 如果是在direction使用to头域
    if (direction & DIRE_CALLER)
    {
        uri = parse_from_uri(msg);
    }
    else if (direction & DIRE_CALLEE)
    {
        uri = parse_to_uri(msg);
    }

    if (!uri || !uri->user.s || !uri->user.len)
    {
        LM_ERR("get call number failed,"
               "direction:%d\n",
               direction);
        return -1;
    }

    if (overf_cdbf.add(overf_cache, &uri->user, 1,
                       of_interval, &val) < 0)
    {
        LM_ERR("local cache add failed!!!\n");
        return -1;
    }

    if (val > max_frequency)
        return -1;

    return 1;
}

