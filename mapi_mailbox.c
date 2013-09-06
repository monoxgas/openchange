#include <php_mapi.h>

static zend_function_entry mapi_mailbox_class_functions[] = {
	PHP_ME(MAPIMailbox,	__construct,	NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(MAPIMailbox,	__destruct,	NULL, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
	PHP_ME(MAPIMailbox,	getName,	NULL, ZEND_ACC_PUBLIC)
	PHP_ME(MAPIMailbox,	setName,	NULL, ZEND_ACC_PUBLIC)
	PHP_ME(MAPIMailbox,	inbox,		NULL, ZEND_ACC_PUBLIC)
	PHP_ME(MAPIMailbox,	calendar, 	NULL, ZEND_ACC_PUBLIC)
	PHP_ME(MAPIMailbox,	contacts,	NULL, ZEND_ACC_PUBLIC)
	PHP_ME(MAPIMailbox,	tasks,		NULL, ZEND_ACC_PUBLIC)
	PHP_ME(MAPIMailbox,	openFolder,	NULL, ZEND_ACC_PUBLIC)

	{ NULL, NULL, NULL }
};

struct itemfolder {
	const uint32_t		olFolder;
	const char		*container_class;
};

struct itemfolder	defaultFolders[] = {
	{olFolderInbox,		"IPF.Mail"},
	{olFolderCalendar,	"IPF.Appointment"},
	{olFolderContacts,	"IPF.Contact"},
	{olFolderTasks,		"IPF.Task"},
	{olFolderNotes,		"IPF.Note"},
	{0 , NULL}
};

zend_class_entry		*mapi_mailbox_ce; // before static
static zend_object_handlers	mapi_mailbox_object_handlers;

static void  mapi_mailbox_add_ref(zval *object TSRMLS_DC)
{
	php_printf("mailbox add ref count: %i\n", Z_REFCOUNT_P(object));

	Z_ADDREF_P(object);
	mapi_mailbox_object_t *store_obj = STORE_OBJECT(mapi_mailbox_object_t*, object);
	S_PARENT_ADDREF_P(store_obj);
}

static void mapi_mailbox_del_ref(zval *object TSRMLS_DC)
{
	php_printf("mailbox del ref count: %i\n", Z_REFCOUNT_P(object));

	Z_DELREF_P(object);
	mapi_mailbox_object_t *store_obj = STORE_OBJECT(mapi_mailbox_object_t*, object);
	S_PARENT_DELREF_P(store_obj);
}

static void mapi_mailbox_free_storage(void *object TSRMLS_DC)
{
	php_printf("Mailbox free\n");
	mapi_mailbox_object_t	*obj;
	obj = (mapi_mailbox_object_t *) object;

	if (obj->talloc_ctx) {
		talloc_free(obj->talloc_ctx);
	}
	mapi_object_release(&(obj->store));

//	S_PARENT_DELREF_P(obj);

	zend_object_std_dtor(&(obj->std) TSRMLS_CC);
	efree(obj);
}


static zend_object_value mapi_mailbox_create_handler(zend_class_entry *type TSRMLS_DC)
{
	zval			*tmp;
	zend_object_value	retval;
	mapi_mailbox_object_t	*obj;

	obj = (mapi_mailbox_object_t *) emalloc(sizeof(mapi_mailbox_object_t));
	memset(obj, 0, sizeof(mapi_mailbox_object_t));

	obj->std.ce = type;

	ALLOC_HASHTABLE(obj->std.properties);
	zend_hash_init(obj->std.properties, 0, NULL, ZVAL_PTR_DTOR, 0);
#if PHP_VERSION_ID < 50399
	zend_hash_copy(obj->std.properties, &type->default_properties,
		       (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *));
#else
	object_properties_init((zend_object *) &(obj->std), type);
#endif
	retval.handle = zend_objects_store_put(obj, NULL, mapi_mailbox_free_storage,
					       NULL TSRMLS_CC);
	retval.handlers = &mapi_mailbox_object_handlers;

	/* MAKE_STD_ZVAL(obj->children_folders); */
	/* array_init(obj->children_folders); */

	return retval;
}



void MAPIMailboxRegisterClass(TSRMLS_D)
{
	zend_class_entry	ce;

	INIT_CLASS_ENTRY(ce, "MAPIMailbox", mapi_mailbox_class_functions);
	mapi_mailbox_ce = zend_register_internal_class(&ce TSRMLS_CC);
	mapi_mailbox_ce->create_object = mapi_mailbox_create_handler;
	memcpy(&mapi_mailbox_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));

//	mapi_mailbox_object_handlers.add_ref   =  mapi_mailbox_add_ref;
//	mapi_mailbox_object_handlers.del_ref   =  mapi_mailbox_del_ref;
	mapi_mailbox_object_handlers.clone_obj = NULL;
}


// static  XXX became static/prvate when get rid of sesion deprecated funcions
void init_message_store(mapi_object_t *store,
			       struct mapi_session *session,
			       bool public_folder, char *username)
{
	enum MAPISTATUS retval;

	mapi_object_init(store);
	if (public_folder == true) {
		retval = OpenPublicFolder(session, store);
		if (retval != MAPI_E_SUCCESS) {
			php_error(E_ERROR, "Open public folder: %s", mapi_get_errstr(retval));
		}
	} else if (username) {
		retval = OpenUserMailbox(session, username, store);
		if (retval != MAPI_E_SUCCESS) {
			php_error(E_ERROR, "Open user mailbox: %s", mapi_get_errstr(retval));
		}
	} else {
		retval = OpenMsgStore(session, store);
		if (retval != MAPI_E_SUCCESS) {
			php_error(E_ERROR, "Open message store: %s",  mapi_get_errstr(retval));
		}
	}
}

zval *create_mailbox_object(zval *php_session, char *username TSRMLS_DC)
{
	zval *new_php_obj;
	mapi_mailbox_object_t *new_obj;
	zend_class_entry	**ce;
	struct mapi_session *session;

	MAKE_STD_ZVAL(new_php_obj);
	/* create the MapiMailbox instance in return_value zval */
	if (zend_hash_find(EG(class_table),"mapimailbox", sizeof("mapimailbox"),(void**)&ce) == FAILURE) {
		php_error(E_ERROR, "Class MAPIMailbox does not exist.");
	}
	object_init_ex(new_php_obj, *ce);

	new_obj = (mapi_mailbox_object_t *) zend_object_store_get_object(new_php_obj TSRMLS_CC);
	new_obj->parent = php_session;
//	Z_ADDREF_P(new_obj->parent);
//	S_PARENT_ADDREF_P(new_obj);
	new_obj->talloc_ctx = talloc_named(NULL, 0, "mailbox");
	new_obj->username = username;

	// open user mailbox
	session = mapi_session_get_session(php_session TSRMLS_CC);
	init_message_store(&(new_obj->store), session, false, username);

	return new_php_obj;
}

PHP_METHOD(MAPIMailbox, __construct)
{
	php_error(E_ERROR, "The mailbox object should not created directly.\n" \
		  "Use the 'mailbox' method in the session object");
}

PHP_METHOD(MAPIMailbox, __destruct)
{
	php_printf("Mailbox Destruct\n\n");
//	mapi_mailbox_object_t *obj = THIS_STORE_OBJECT(mapi_mailbox_object_t*);
//	S_PARENT_DELREF_P(obj);
	php_printf("END Mailbox Destruct\n\n");
//	Z_DTOR_GUARD_P(getThis(), "MAPIMailbox object");
}

PHP_METHOD(MAPIMailbox, getName)
{
	enum MAPISTATUS		retval;
	const char		*mailbox_name = NULL;
	TALLOC_CTX		*this_obj_talloc_ctx;
	TALLOC_CTX		*talloc_ctx;
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	*lpProps;
	uint32_t		cValues;
	zval                    *this_php_obj;
	mapi_mailbox_object_t	*this_obj;

	this_php_obj = getThis();
	this_obj = (mapi_mailbox_object_t *) zend_object_store_get_object(this_php_obj TSRMLS_CC);
	this_obj_talloc_ctx = OBJ_GET_TALLOC_CTX(mapi_mailbox_object_t *, this_php_obj);
	talloc_ctx = talloc_named(this_obj_talloc_ctx, 0, "MAPIMailbox::getName");

	/* Retrieve the mailbox folder name */
	SPropTagArray = set_SPropTagArray(talloc_ctx, 0x1, PR_DISPLAY_NAME_UNICODE);
	retval = GetProps(&this_obj->store, MAPI_UNICODE, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	CHECK_MAPI_RETVAL(retval, "Get mailbox properties");

	/* FIXME: Use accessor instead of direct access */
	if (lpProps[0].value.lpszW) {
		mailbox_name = lpProps[0].value.lpszW;
	} else {
		talloc_free(talloc_ctx);
		RETURN_NULL();
	}

        RETVAL_STRING(mailbox_name, 1);
	talloc_free(talloc_ctx);
}

PHP_METHOD(MAPIMailbox, setName)
{
	php_error(E_ERROR, "Not implemented");
}

PHP_METHOD(MAPIMailbox, inbox)
{
	enum MAPISTATUS  retval;
	uint64_t	id_inbox;
	uint32_t	count;
	zval            *folder;

	zval  *this_php_obj = getThis();
	mapi_mailbox_object_t *this_obj = (mapi_mailbox_object_t *) zend_object_store_get_object(this_php_obj TSRMLS_CC);

	retval = GetReceiveFolder(&this_obj->store, &id_inbox, NULL);
	CHECK_MAPI_RETVAL(retval, "Get receive folder");

	folder = create_folder_object(this_php_obj, id_inbox, "IPF.Note" TSRMLS_CC);

	if (folder == NULL) {
		RETURN_NULL();
	}
	RETURN_ZVAL(folder, 0, 1);
}

static zval *open_folder(zval *php_mailbox, mapi_id_t fid, char *folderType TSRMLS_DC)
{
	zval				*folder;
	folder = create_folder_object(php_mailbox, fid, folderType  TSRMLS_CC);
	return folder;
}

static zval *default_folder_for_item(zval *php_mailbox, char *folderType TSRMLS_DC)
{
	enum MAPISTATUS 		retval;
	uint32_t	 		i;
	uint32_t			olFolder = 0;
	mapi_mailbox_object_t 		*mailbox;
	mapi_id_t			fid;


	for (i = 0; defaultFolders[i].olFolder; i++) {
		if (!strncasecmp(defaultFolders[i].container_class, folderType, strlen(defaultFolders[i].container_class))) {
			olFolder = defaultFolders[i].olFolder;
		}
	}
	if (!olFolder) {
		php_error(E_ERROR, "Cannot found defualt folder for  type %s", folderType);
	}

	mailbox = (mapi_mailbox_object_t *) zend_object_store_get_object(php_mailbox TSRMLS_CC);
	retval = GetDefaultFolder(&(mailbox->store), &fid, olFolder);
	CHECK_MAPI_RETVAL(retval, "GetDefaultFolder for type");

	return open_folder(php_mailbox, fid, folderType TSRMLS_CC);
}

PHP_METHOD(MAPIMailbox, calendar)
{
	zval *folder = default_folder_for_item(getThis(), "IPF.Appointment"  TSRMLS_CC);
	RETURN_ZVAL(folder, 0, 1);
}

PHP_METHOD(MAPIMailbox, contacts)
{
	zval *folder = default_folder_for_item(getThis(), "IPF.Contact"  TSRMLS_CC);
	RETURN_ZVAL(folder, 0, 1);
}

PHP_METHOD(MAPIMailbox, tasks)
{
	zval *folder = default_folder_for_item(getThis(), "IPF.Task"  TSRMLS_CC);
	RETURN_ZVAL(folder, 0, 1);
}

PHP_METHOD(MAPIMailbox, openFolder)
{
	mapi_id_t		folder_id;
	char			*id_str;
	size_t			id_str_len;
	char 		        *folder_type;
	size_t                  folder_type_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss",
				  &id_str, &id_str_len, &folder_type, &folder_type_len) == FAILURE) {
		php_error(E_ERROR, "Missing arguments: (folderId, folderType");
	}
	folder_id = str_to_mapi_id(id_str);
	zval *folder = open_folder(getThis(), folder_id, folder_type TSRMLS_CC);
	if (folder == NULL) {
		RETURN_NULL();
	}

	RETURN_ZVAL(folder, 0, 1);
}
