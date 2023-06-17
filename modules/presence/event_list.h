/*
 * presence module - presence server implementation
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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
 *  2007-04-05  initial version (Anca Vamanu)
 */

#ifndef _PRES_EV_LST_H
#define  _PRES_EV_LST_H

#include "../../parser/msg_parser.h"
#include "../../parser/parse_event.h"
#include "../../str.h"
#include "subscribe.h"

#define WINFO_TYPE			1<< 0
#define PUBL_TYPE			1<< 1

struct subscription;

typedef int (apply_auth_t)(str* , struct subscription*, str** );

typedef int (publ_handling_t)(struct sip_msg*, int* sent_event);

typedef int (subs_handling_t)(struct sip_msg*,struct subscription *,int*, str*);

typedef str* (empty_pres_info_t)(str* pres_uri, str* extra_hdrs);

typedef str* (agg_nbody_t)(str* pres_user, str* pres_domain, str** body_array, int n, int off_index);
/* params for agg_body_t
 *	body_array= an array with all the bodies stored for that resource
 *	n= the number of bodies
 *	off_index= the index of the registration(etag) for which a Publish
 *				with Expires: 0 has just been received
 *	*/
typedef str* (aux_body_processing_t)(struct subscription *subs, str* body);
/* params for agg_body_t
 *	subs= a subscription structure to manipulate the body for a certain watcher
 *	body= the original body
 *
 * return value: 0: means that there was no manipulation or the manipulation was
 *                  done directly in the original body
 *           pointer: a pointer to str for the "per watcher" body. gets freed by aux_free_body()
 *	*/
typedef int (is_allowed_t)(struct subscription* subs);
typedef int (get_rules_doc_t)(str* user, str* domain, str** rules_doc);
/* return code rules for is_allowed_t
 *	< 0  if error occurred
 *	=0	 if no change in status(if no xcap document exists)
 *	>0   if change in status
 *	*/

/* This function provides a substitute for the presentity information. Upon a
 * subscribe, instead of pushing a notify with a body built from the published
 * presentities, you can dynamically build with this function whatever 
 * body you want to be returned with the body
 * A free_body_t function must also be provided if this function is used.
 * Input data: presentity SIP URI and the SUBSCRIBE's body
 * Output data: * the body (may be NULL if nothing to return); it must
 *                be a pkg allocated str and separate pkg allocated body
 *              * the content-type string - must be a pkg mem chunk 
 *                holding the CT body; note that the str itself is managed by
 *                the upper/calling layer, so just use/write into it.
 * Returns : body str or NULL (on error or no body to be returned)
 */
typedef str* (build_notify_body_t)(str *pres_uri, str *subs_body,
		str *ct_type, int *suppress_notify);


/* event specific body free function */
typedef void(free_body_t)(char* body);

struct pres_ev
{
	str name;
	event_t* evp;
	str content_type;
	str* extra_hdrs;
	int default_expires;
	int type;
	/* Flag that sets the requirements for body:
	 *
	 * 0 - body is not mandatory
	 * 1 - body is mandatory
	 */
	int mandatory_body;
	/* Flag that sets the requirements for timeout notification:
	 *
	 * 0 - NOTIFY with reason "timeout" is not mandatory
	 * 1 - NOTIFY with reason "timeout" is mandatory
	 */
	int mandatory_timeout_notification;
	/*
	 *  0 - the standard mechanism (allocating new etag for each Publish)
	 *  1 - allocating an etag only for an initial Publish
	 * */
	int etag_not_new;
	/* fileds that deal with authorization rules*/
	/*
	 *  req_auth -> flag 0  - if not require
	 *  is_watcher_allowed  - get subscription state from xcap rules
	 *  apply_auth_nbody    - alter the body according to authorization rules
	 */
	int req_auth;
	get_rules_doc_t* get_rules_doc;
	apply_auth_t*  apply_auth_nbody;
	is_allowed_t*  get_auth_status;

	/* an agg_body_t function should be registered if the event permits having
	 * multiple published states and requires an aggregation of the information
	 * otherwise, this field should be NULL and the last published state is taken
	 * when constructing Notify msg
	 * */
	agg_nbody_t* agg_nbody;
	publ_handling_t  * evs_publ_handl;
	subs_handling_t  * evs_subs_handl;
	/* for some phones and specific events, we need to provide some dummy presence
	 * information - like a dummy body for dialog event when no information is
	 * available in the presentity table
	 */
	empty_pres_info_t  * build_empty_pres_info;
	free_body_t* free_body;
	/* sometimes it is necessary that a module make changes for a body for each
	 * active watcher (e.g. setting the "version" parameter in an XML document.
	 * If a module registers the aux_body_processing callback, it gets called for
	 * each watcher. It either gets the body received by the PUBLISH, or the body
	 * generated by the agg_nbody function.
	 * The module can deceide if it makes a copy of the original body, which is then
	 * manipulated, or if it works directly in the original body. If the module makes a
	 * copy of the original body, it also has to register the aux_free_body() to
	 * free this "per watcher" body.
	 */
	aux_body_processing_t* aux_body_processing;
	free_body_t* aux_free_body;

	build_notify_body_t *build_notify_body;

	struct pres_ev* wipeer;
	struct pres_ev* next;
};
typedef struct pres_ev pres_ev_t;

/* pointer holders for 'dialog' and 'presence_event',
 * they need fast access */
extern pres_ev_t** pres_event_p;
extern pres_ev_t** dialog_event_p;

typedef struct evlist
{
	int ev_count;
	pres_ev_t* events;
}evlist_t;

evlist_t* init_evlist(void);

int add_event(pres_ev_t* event);

typedef int (*add_event_t)(pres_ev_t* event);

void free_event_params(param_t* params, int mem_type);

pres_ev_t* contains_event(str* name, event_t* parsed_event);

typedef pres_ev_t* (*contains_event_t)(str* name, event_t* parsed_event);

int get_event_list(str** ev_list);

typedef int (*get_event_list_t) (str** ev_list);

void destroy_evlist(void);

extern evlist_t* EvList;

pres_ev_t* search_event(event_t* event);
typedef pres_ev_t* (*search_event_t)(event_t* event);

event_t* shm_copy_event(event_t* e);

void shm_free_event(event_t* ev);

void free_pres_event(pres_ev_t* ev);


#endif
