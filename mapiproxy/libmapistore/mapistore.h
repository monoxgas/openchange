/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

   Copyright (C) Julien Kerihuel 2009-2010

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef	__MAPISTORE_H
#define	__MAPISTORE_H

#ifndef	_GNU_SOURCE
#define	_GNU_SOURCE
#endif

#ifndef	_PUBLIC_
#define	_PUBLIC_
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>

#include <tdb.h>
#include <ldb.h>
#include <talloc.h>
#include <util/debug.h>

#include "libmapi/libmapi.h"

#define	MAPISTORE_SUCCESS	0

typedef	int (*init_backend_fn) (void);

#define	MAPISTORE_INIT_MODULE	"mapistore_init_backend"

#define	MAPISTORE_FOLDER_TABLE	1
#define	MAPISTORE_MESSAGE_TABLE	2
#define	MAPISTORE_FAI_TABLE	3
#define	MAPISTORE_RULE_TABLE	4

#define MAPISTORE_FOLDER	1
#define	MAPISTORE_MESSAGE	2
#define	MAPISTORE_ATTACHMENT	3

#define	MAPISTORE_SOFT_DELETE		1
#define	MAPISTORE_PERMANENT_DELETE	2

struct mapistore_message {
	struct SRowSet			*recipients;
	struct SRow			*properties;
};

struct indexing_folders_list {
	uint64_t			*folderID;
	uint32_t			count;
};

enum table_query_type {
	MAPISTORE_PREFILTERED_QUERY,
	MAPISTORE_LIVEFILTERED_QUERY,
};

/* proof of concept: a new structure to simplify property queries */
struct mapistore_property_data {
        void *data;
        int error; /* basically MAPISTORE_SUCCESS or MAPISTORE_ERR_NOT_FOUND */
};

struct mapistore_backend {
	const char	*name;
	const char	*description;
	const char	*namespace;

	int (*init)(void);
	int (*create_context)(TALLOC_CTX *, const char *, void **);
	int (*delete_context)(void *);
	int (*release_record)(void *, uint64_t, uint8_t);
	int (*get_path)(void *, uint64_t, uint8_t, char **);
	/* folders semantic */
	int (*op_mkdir)(void *, uint64_t, uint64_t, struct SRow *);
	int (*op_rmdir)(void *, uint64_t, uint64_t);
	int (*op_opendir)(void *, uint64_t, uint64_t);
	int (*op_closedir)(void *);
	int (*op_readdir_count)(void *, uint64_t, uint8_t, uint32_t *);
	int (*op_get_table_property)(void *, uint64_t, uint8_t, enum table_query_type, uint32_t, uint32_t, void **);
	/* message semantics */
	int (*op_openmessage)(void *, uint64_t, uint64_t, struct mapistore_message *);
	int (*op_createmessage)(void *, uint64_t, uint64_t, uint8_t);
	int (*op_savechangesmessage)(void *, uint64_t, uint8_t);
	int (*op_submitmessage)(void *, uint64_t, uint8_t);
	int (*op_getprops)(void *, uint64_t, uint8_t, struct SPropTagArray *, struct SRow *);
	int (*op_get_fid_by_name)(void *, uint64_t, const char *, uint64_t *);
	int (*op_setprops)(void *, uint64_t, uint8_t, struct SRow *);
	int (*op_set_property_from_fd)(void *, uint64_t, uint8_t, uint32_t, int);
	int (*op_get_property_into_fd)(void *, uint64_t, uint8_t, uint32_t, int);
	int (*op_modifyrecipients)(void *, uint64_t, struct ModifyRecipientRow *, uint16_t);
        int (*op_deletemessage)(void *, uint64_t, uint64_t, uint8_t flags);
	/* restriction semantics */
	int (*op_set_restrictions)(void *, uint64_t, uint8_t, struct mapi_SRestriction *, uint8_t *);
        /* sort order */
	int (*op_set_sort_order)(void *, uint64_t, uint8_t, struct SSortOrderSet *, uint8_t *);

	int (*op_get_folders_list)(void *, uint64_t fmid, struct indexing_folders_list **folders_list);

        /***
            proof of concept
        */

        /** oxcmsg operations */
        /** oxcstor operations */
        struct {
                int (*release)(void *);
        } store;

        /* note: the mid parameter will be replaced with a message object here once we have a "pocop_open_message"... */
        struct {
                int (*get_attachment_table)(void *, uint64_t, void **, uint32_t *);
                int (*get_attachment)(void *, uint64_t, uint32_t, void **);
                int (*create_attachment)(void *, uint64_t, uint32_t *, void **);
        } message;

        /** oxctabl operations */
        struct {
                int (*set_columns)(void *, uint16_t, enum MAPITAGS *);
                int (*get_row)(void *, enum table_query_type, uint32_t, struct mapistore_property_data *);
        } table;

        /** oxcprpt operations */
        struct {
                int (*get_properties)(void *, uint16_t, enum MAPITAGS *, struct mapistore_property_data *);
                int (*set_properties)(void *, struct SRow *);
        } properties;
};

struct indexing_context_list;

struct backend_context {
	const struct mapistore_backend	*backend;
	void				*private_data;
	struct indexing_context_list	*indexing;
	uint32_t			context_id;
	uint32_t			ref_count;
	char				*uri;
};

struct backend_context_list {
	struct backend_context		*ctx;
	struct backend_context_list	*prev;
	struct backend_context_list	*next;
};

struct processing_context;

struct mapistore_context {
	struct processing_context	*processing_ctx;
	struct backend_context_list    	*context_list;
	struct indexing_context_list	*indexing_list;
	void				*nprops_ctx;
};

#ifndef __BEGIN_DECLS
#ifdef __cplusplus
#define __BEGIN_DECLS		extern "C" {
#define __END_DECLS		}
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif
#endif

__BEGIN_DECLS

/* definitions from mapistore_interface.c */
struct mapistore_context *mapistore_init(TALLOC_CTX *, const char *);
int mapistore_release(struct mapistore_context *);
int mapistore_add_context(struct mapistore_context *, const char *, uint32_t *);
int mapistore_add_context_ref_count(struct mapistore_context *, uint32_t);
int mapistore_del_context(struct mapistore_context *, uint32_t);
int mapistore_release_record(struct mapistore_context *, uint32_t, uint64_t, uint8_t);
int mapistore_search_context_by_uri(struct mapistore_context *, const char *, uint32_t *);
const char *mapistore_errstr(int);
int mapistore_add_context_indexing(struct mapistore_context *, const char *, uint32_t);
int mapistore_opendir(struct mapistore_context *, uint32_t, uint64_t, uint64_t);
int mapistore_closedir(struct mapistore_context *mstore_ctx, uint32_t, uint64_t);
int mapistore_mkdir(struct mapistore_context *, uint32_t, uint64_t, uint64_t, struct SRow *);
int mapistore_rmdir(struct mapistore_context *, uint32_t, uint64_t, uint64_t, uint8_t);
int mapistore_get_folder_count(struct mapistore_context *, uint32_t, uint64_t, uint32_t *);
int mapistore_get_message_count(struct mapistore_context *, uint32_t, uint64_t, uint8_t, uint32_t *);
int mapistore_get_table_property(struct mapistore_context *, uint32_t, uint8_t, enum table_query_type, uint64_t, uint32_t, uint32_t, void **);
int mapistore_openmessage(struct mapistore_context *, uint32_t, uint64_t, uint64_t, struct mapistore_message *);
int mapistore_createmessage(struct mapistore_context *, uint32_t, uint64_t, uint64_t, uint8_t);
int mapistore_savechangesmessage(struct mapistore_context *, uint32_t, uint64_t, uint8_t);
int mapistore_submitmessage(struct mapistore_context *, uint32_t, uint64_t, uint8_t);
int mapistore_getprops(struct mapistore_context *, uint32_t, uint64_t, uint8_t, struct SPropTagArray *, struct SRow *);
int mapistore_get_fid_by_name(struct mapistore_context *, uint32_t, uint64_t, const char *, uint64_t*);
int mapistore_setprops(struct mapistore_context *, uint32_t, uint64_t, uint8_t, struct SRow *);
int mapistore_set_property_from_fd(struct mapistore_context *, uint32_t, uint64_t, uint8_t, uint32_t, int);
int mapistore_get_property_into_fd(struct mapistore_context *, uint32_t, uint64_t, uint8_t, uint32_t, int);
int mapistore_modifyrecipients(struct mapistore_context *, uint32_t, uint64_t, struct ModifyRecipientRow*, uint16_t);
int mapistore_get_child_fids(struct mapistore_context *, uint32_t, uint64_t, uint64_t **, uint32_t *);
int mapistore_deletemessage(struct mapistore_context *, uint32_t, uint64_t, uint64_t, uint8_t);
int mapistore_get_folders_list(struct mapistore_context *, uint64_t, struct indexing_folders_list **);
int mapistore_set_restrictions(struct mapistore_context *, uint32_t, uint64_t, uint8_t, struct mapi_SRestriction *, uint8_t *);
int mapistore_set_sort_order(struct mapistore_context *, uint32_t, uint64_t, uint8_t, struct SSortOrderSet *, uint8_t *);

/* proof of concept */
int mapistore_pocop_get_attachment_table(struct mapistore_context *, uint32_t, uint64_t mid, void **, uint32_t *);
int mapistore_pocop_get_attachment(struct mapistore_context *, uint32_t, uint64_t mid, uint32_t, void **);
int mapistore_pocop_create_attachment(struct mapistore_context *, uint32_t, uint64_t mid, uint32_t *, void **);

int mapistore_pocop_set_table_columns(struct mapistore_context *, uint32_t, void *, uint16_t, enum MAPITAGS *);
int mapistore_pocop_get_table_row(struct mapistore_context *, uint32_t, void *, enum table_query_type, uint32_t, struct mapistore_property_data *);

int mapistore_pocop_get_properties(struct mapistore_context *, uint32_t, void *, uint16_t, enum MAPITAGS *, struct mapistore_property_data *);
int mapistore_pocop_set_properties(struct mapistore_context *, uint32_t, void *, struct SRow *);

int mapistore_pocop_release(struct mapistore_context *, uint32_t, void *);

/* definitions from mapistore_processing.c */
int mapistore_set_mapping_path(const char *);

/* definitions from mapistore_backend.c */
extern int	mapistore_backend_register(const void *);
const char	*mapistore_backend_get_installdir(void);
init_backend_fn	*mapistore_backend_load(TALLOC_CTX *, const char *);
struct backend_context *mapistore_backend_lookup(struct backend_context_list *, uint32_t);
struct backend_context *mapistore_backend_lookup_by_uri(struct backend_context_list *, const char *);
bool		mapistore_backend_run_init(init_backend_fn *);

/* definitions from mapistore_indexing.c */
int mapistore_indexing_add(struct mapistore_context *, const char *);
int mapistore_indexing_del(struct mapistore_context *, const char *);
int mapistore_indexing_get_folder_list(struct mapistore_context *, const char *, uint64_t, struct indexing_folders_list **);
int mapistore_indexing_record_add_fid(struct mapistore_context *, uint32_t, uint64_t);
int mapistore_indexing_record_del_fid(struct mapistore_context *, uint32_t, uint64_t, uint8_t);
int mapistore_indexing_record_add_mid(struct mapistore_context *, uint32_t, uint64_t);
int mapistore_indexing_record_del_mid(struct mapistore_context *, uint32_t, uint64_t, uint8_t);

/* definitions from mapistore_namedprops.c */
int mapistore_namedprops_get_mapped_id(void *ldb_ctx, struct MAPINAMEID, uint16_t *);

__END_DECLS

#endif	/* ! __MAPISTORE_H */
