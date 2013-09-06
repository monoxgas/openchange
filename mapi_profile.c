/*
   OpenChange MAPI PHP bindings

   Copyright (C) Zentyal SL. 2013.

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php_mapi.h"

static zend_function_entry mapi_profile_class_functions[] = {
	PHP_ME(MAPIProfile,	__construct,	NULL,	ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(MAPIProfile,	__destruct,	NULL,	ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
	PHP_ME(MAPIProfile,	dump,		NULL,	ZEND_ACC_PUBLIC)
	PHP_ME(MAPIProfile,	logon,		NULL,	ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};

static zend_class_entry		*mapi_profile_ce;
static zend_object_handlers	mapi_profile_object_handlers;

static void  mapi_profile_add_ref(zval *object TSRMLS_DC)
{
	php_printf("profile add ref count: %i -> %i \n", Z_REFCOUNT_P(object),  Z_REFCOUNT_P(object)+1);

	mapi_profile_object_t *store_obj = STORE_OBJECT(mapi_profile_object_t*, object);
//	Z_ADDREF_P(object);
	S_PARENT_ADDREF_P(store_obj);

}

static void mapi_profile_del_ref(zval *object TSRMLS_DC)
{
	if (Z_REFCOUNT_P(object) == 0) return;
	php_printf("profile del ref count: %i -> %i\n", Z_REFCOUNT_P(object), Z_REFCOUNT_P(object)-1);

	Z_DELREF_P(object);
	mapi_profile_object_t *store_obj = STORE_OBJECT(mapi_profile_object_t*, object);
	S_PARENT_DELREF_P(store_obj);
}

static void mapi_profile_free_storage(void *object TSRMLS_DC)
{
	php_printf("profile free\n");

	mapi_profile_object_t *obj = (mapi_profile_object_t *) object;

	if (obj->talloc_ctx) {
		talloc_free(obj->talloc_ctx);
	}
//	Z_DELREF_P( obj->parent);
//	S_PARENT_DELREF_P(obj);

	zend_object_std_dtor(&(obj->std) TSRMLS_CC);
	efree(obj);
}

static zend_object_value mapi_profile_create_handler(zend_class_entry *type TSRMLS_DC)
{
	zval			*tmp;
	zend_object_value	retval;
	mapi_profile_object_t	*obj;

	obj = (mapi_profile_object_t *) emalloc(sizeof(mapi_profile_object_t));
	memset(obj, 0, sizeof(mapi_profile_object_t));

	obj->std.ce = type;

	ALLOC_HASHTABLE(obj->std.properties);
	zend_hash_init(obj->std.properties, 0, NULL, ZVAL_PTR_DTOR, 0);
#if PHP_VERSION_ID < 50399
	zend_hash_copy(obj->std.properties, &type->default_properties,
		       (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *));
#else
	object_properties_init((zend_object *) &(obj->std), type);
#endif
	retval.handle = zend_objects_store_put(obj, NULL, mapi_profile_free_storage,
					       NULL TSRMLS_CC);
	retval.handlers = &mapi_profile_object_handlers;

	return retval;
}

void MAPIProfileRegisterClass(TSRMLS_D)
{
	zend_class_entry	ce;

	INIT_CLASS_ENTRY(ce, "MAPIProfile", mapi_profile_class_functions);
	mapi_profile_ce = zend_register_internal_class(&ce TSRMLS_CC);
	mapi_profile_ce->create_object = mapi_profile_create_handler;
	memcpy(&mapi_profile_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));

//	mapi_profile_object_handlers.add_ref   =  mapi_profile_add_ref;
//	mapi_profile_object_handlers.del_ref   =  mapi_profile_del_ref;
	mapi_profile_object_handlers.clone_obj = NULL;
}

zval *create_profile_object(struct mapi_profile *profile,
			    zval *profile_db,
			    TALLOC_CTX *talloc_ctx TSRMLS_DC)
{
	zval			*php_obj;
	zend_class_entry	**ce;
	mapi_profile_object_t	*obj;

	MAKE_STD_ZVAL(php_obj);

	/* create the MapiProfile instance in return_value zval */
	if (zend_hash_find(EG(class_table),"mapiprofile",
			   sizeof("mapiprofile"),(void**)&ce) == FAILURE) {
		php_error(E_ERROR,"Class MAPIProfile does not exist.");
	}

	object_init_ex(php_obj, *ce);

	obj = (mapi_profile_object_t*) zend_object_store_get_object(php_obj TSRMLS_CC);
	obj->profile = profile;
	obj->parent = profile_db;
//	Z_ADDREF_P(obj->parent);
	php_printf("Create profile obj add ref\n");

//	S_PARENT_ADDREF_P(obj);

	php_printf("Create profile obj END del ref\n");

	obj->talloc_ctx =  talloc_ctx;

	return php_obj;
}

struct mapi_context *profile_get_mapi_context(zval *profile_obj TSRMLS_DC)
{
	mapi_profile_object_t *obj;

	obj = (mapi_profile_object_t *) zend_object_store_get_object(profile_obj TSRMLS_CC);
	return mapi_profile_db_get_mapi_context(obj->parent  TSRMLS_CC);
}

struct mapi_profile *get_profile(zval *php_profile_obj TSRMLS_DC)
{
	mapi_profile_object_t *obj;

	obj = (mapi_profile_object_t *) zend_object_store_get_object(php_profile_obj TSRMLS_CC);
	return obj->profile;
}

PHP_METHOD(MAPIProfile, __construct)
{
	php_error(E_ERROR, "This class cannot be instatiated. Use the getProfile class from MapiProfileDB");
}

PHP_METHOD(MAPIProfile, __destruct)
{
	php_printf("Profile Destruct. Refreces: %i\n\n", Z_REFCOUNT_P(getThis()));
//	mapi_profile_object_t *obj = THIS_STORE_OBJECT(mapi_profile_object_t*);
//	S_PARENT_DELREF_P(obj);
	php_printf("END Profile Destruct\n\n");
//	Z_DTOR_GUARD_P(getThis(), "MAPIProfile object");
}


PHP_METHOD(MAPIProfile, dump)
{
	struct mapi_profile	*profile;
	char			*exchange_version = NULL;

	profile = get_profile(getThis() TSRMLS_CC);

	switch (profile->exchange_version) {
	case 0x0:
		exchange_version = "exchange 2000";
		break;
	case 0x1:
		exchange_version = "exchange 2003/2007";
		break;
	case 0x2:
		exchange_version = "exchange 2010";
		break;
	default:
		php_error(E_ERROR, "Error: unknown Exchange server: %i\n", profile->exchange_version);
	}

	array_init(return_value);
	add_assoc_string(return_value, "profile", (char *) profile->profname, 1);
	add_assoc_string(return_value, "exchange_server", exchange_version, 1);
	add_assoc_string(return_value, "encription", (profile->seal == true) ? "yes" : "no", 1);
	add_assoc_string(return_value, "username", (char *) profile->username, 1);
	add_assoc_string(return_value, "password", (char *) profile->password, 1);
	add_assoc_string(return_value, "mailbox", (char *) profile->mailbox, 1);
	add_assoc_string(return_value, "workstation", (char *) profile->workstation, 1);
	add_assoc_string(return_value, "domain", (char *) profile->domain, 1);
	add_assoc_string(return_value, "server", (char *) profile->server, 1);
}

PHP_METHOD(MAPIProfile, logon)
{
	enum MAPISTATUS		status;
	struct mapi_context	*mapi_ctx;
	struct mapi_profile	*profile;
	struct mapi_session	*session = NULL;
	TALLOC_CTX		*talloc_ctx;
	zval			*this_php_obj;
	zval			*php_obj;
	mapi_profile_object_t	*this_obj;

	this_php_obj = getThis();
	mapi_ctx = profile_get_mapi_context(this_php_obj TSRMLS_CC);
	profile = get_profile(this_php_obj TSRMLS_CC);

	status = MapiLogonEx(mapi_ctx, &session, profile->profname, profile->password);
	if (status != MAPI_E_SUCCESS) {
		php_error(E_ERROR, "MapiLogonEx: %s",  mapi_get_errstr(status));
	}

	talloc_ctx = talloc_named(NULL, 0, "session");
	php_obj = create_session_object(session, this_php_obj, talloc_ctx TSRMLS_CC);

	RETURN_ZVAL(php_obj, 0, 1);
}
