#include "edbus_private_types.h"
#include "edbus_private.h"

#define DBUS_ANNOTATION(name, value) \
        "<annotation" \
        " name=\"org.freedesktop.DBus." name "\"" \
        " value=\"" value "\"" \
        "/>"

#define DBUS_ANNOTATION_DEPRECATED  DBUS_ANNOTATION("Deprecated", "true")
#define DBUS_ANNOTATION_NOREPLY     DBUS_ANNOTATION("Method.NoReply", "true")

#ifndef DBUS_ERROR_UNKNOWN_INTERFACE
# define DBUS_ERROR_UNKNOWN_INTERFACE          "org.freedesktop.DBus.Error.UnknownInterface"
#endif

#ifndef DBUS_ERROR_UNKNOWN_PROPERTY
# define DBUS_ERROR_UNKNOWN_PROPERTY           "org.freedesktop.DBus.Error.UnknownProperty"
#endif

#ifndef DBUS_ERROR_PROPERTY_READ_ONLY
# define DBUS_ERROR_PROPERTY_READ_ONLY         "org.freedesktop.DBus.Error.PropertyReadOnly"
#endif

#define EDBUS_SERVICE_INTERFACE_CHECK(obj)                         \
  do                                                            \
    {                                                           \
       EINA_SAFETY_ON_NULL_RETURN(obj);                         \
       if (!EINA_MAGIC_CHECK(obj, EDBUS_SERVICE_INTERFACE_MAGIC))  \
         {                                                      \
            EINA_MAGIC_FAIL(obj, EDBUS_SERVICE_INTERFACE_MAGIC);   \
            return;                                             \
         }                                                      \
    }                                                           \
  while (0)

#define EDBUS_SERVICE_INTERFACE_CHECK_RETVAL(obj, retval)                  \
  do                                                                    \
    {                                                                   \
       EINA_SAFETY_ON_NULL_RETURN_VAL(obj, retval);                     \
       if (!EINA_MAGIC_CHECK(obj, EDBUS_SERVICE_INTERFACE_MAGIC))          \
         {                                                              \
            EINA_MAGIC_FAIL(obj, EDBUS_SERVICE_INTERFACE_MAGIC);           \
            return retval;                                              \
         }                                                              \
    }                                                                   \
  while (0)


static void _object_unregister(DBusConnection *conn, void *user_data);
static DBusHandlerResult _object_handler(DBusConnection *conn, DBusMessage *message, void *user_data);
static void _object_free(EDBus_Service_Object *obj);
static void _interface_free(EDBus_Service_Interface *interface);
static void _on_connection_free(void *data, const void *dead_pointer);

static DBusObjectPathVTable vtable = {
  _object_unregister,
  _object_handler,
  NULL,
  NULL,
  NULL,
  NULL
};

EDBus_Service_Interface *introspectable;
EDBus_Service_Interface *properties_iface;
EDBus_Service_Interface *objmanager;

static inline void
_introspect_arguments_append(Eina_Strbuf *buf, const EDBus_Arg_Info *args,
                             const char *direction)
{
   for (; args && args->signature; args++)
     {
        if (args->name && args->name[0])
          eina_strbuf_append_printf(buf, "<arg type=\"%s\" name=\"%s\"",
                                    args->signature, args->name);
        else
          eina_strbuf_append_printf(buf, "<arg type=\"%s\"", args->signature);

        if (direction)
          eina_strbuf_append_printf(buf, " direction=\"%s\" />", direction);
        else
          eina_strbuf_append(buf, " />");
     }
}

static inline void
_introspect_append_signal(Eina_Strbuf *buf, const EDBus_Signal *sig)
{
   eina_strbuf_append_printf(buf, "<signal name=\"%s\"", sig->name);

   if (!sig->flags && !(sig->args && sig->args->signature))
     {
        eina_strbuf_append(buf, " />");
        return;
     }

   eina_strbuf_append(buf, ">");

   if (sig->flags & EDBUS_SIGNAL_FLAG_DEPRECATED)
     eina_strbuf_append(buf, DBUS_ANNOTATION_DEPRECATED);

   _introspect_arguments_append(buf, sig->args, NULL);

   eina_strbuf_append(buf, "</signal>");
}

static inline void
_instrospect_append_property(Eina_Strbuf *buf, const EDBus_Property *prop, const EDBus_Service_Interface *iface)
{
   eina_strbuf_append_printf(buf, "<property name=\"%s\" type=\"%s\" access=\"",
                             prop->name, prop->type);

   if (iface->get_func || prop->get_func)
     eina_strbuf_append(buf, "read");

   if (iface->set_func || prop->set_func)
     eina_strbuf_append(buf, "write");

   if (!prop->flags)
     {
        eina_strbuf_append(buf, "\" />");
        return;
     }

   eina_strbuf_append(buf, "\">");

   if (prop->flags & EDBUS_PROPERTY_FLAG_DEPRECATED)
     eina_strbuf_append(buf, DBUS_ANNOTATION_DEPRECATED);

   eina_strbuf_append(buf, "</property>");
}

static inline void
_introspect_append_method(Eina_Strbuf *buf, const EDBus_Method *method)
{
   eina_strbuf_append_printf(buf, "<method name=\"%s\">", method->member);

   if (method->flags & EDBUS_METHOD_FLAG_DEPRECATED)
     eina_strbuf_append(buf, DBUS_ANNOTATION_DEPRECATED);

   if (method->flags & EDBUS_METHOD_FLAG_NOREPLY)
     eina_strbuf_append(buf, DBUS_ANNOTATION_NOREPLY);

   _introspect_arguments_append(buf, method->in, "in");
   _introspect_arguments_append(buf, method->out, "out");
   eina_strbuf_append(buf, "</method>");
}

typedef struct _Property
{
   const EDBus_Property *property;
   Eina_Bool is_invalidate:1;
} Property;

static void
_introspect_append_interface(Eina_Strbuf *buf, EDBus_Service_Interface *iface)
{
   const EDBus_Method *method;
   Property *prop;
   Eina_Iterator *iterator;
   unsigned short i;

   eina_strbuf_append_printf(buf, "<interface name=\"%s\">", iface->name);

   iterator = eina_hash_iterator_data_new(iface->methods);
   EINA_ITERATOR_FOREACH(iterator, method)
     _introspect_append_method(buf, method);
   eina_iterator_free(iterator);

   for (i = 0; i < eina_array_count(iface->sign_of_signals); i++)
     _introspect_append_signal(buf, &iface->signals[i]);

   iterator = eina_hash_iterator_data_new(iface->properties);
   EINA_ITERATOR_FOREACH(iterator, prop)
     _instrospect_append_property(buf, prop->property, iface);
   eina_iterator_free(iterator);

   eina_strbuf_append(buf, "</interface>");
}

static EDBus_Message *
_cb_property_get(const EDBus_Service_Interface *piface, const EDBus_Message *msg)
{
   const char *propname, *iface_name;
   EDBus_Service_Object *obj = piface->obj;
   EDBus_Service_Interface *iface;
   Property *prop;
   EDBus_Message *reply, *error_reply = NULL;
   EDBus_Message_Iter *main_iter, *variant;
   Eina_Bool ret;
   EDBus_Property_Get_Cb getter = NULL;

   if (!edbus_message_arguments_get(msg, "ss", &iface_name, &propname))
     return NULL;

   iface = eina_hash_find(obj->interfaces, iface_name);
   if (!iface)
     return edbus_message_error_new(msg, DBUS_ERROR_UNKNOWN_INTERFACE,
                                    "Interface not found.");

   prop = eina_hash_find(iface->properties, propname);
   if (!prop || prop->is_invalidate) goto not_found;

   if (prop->property->get_func)
     getter = prop->property->get_func;
   else if (iface->get_func)
     getter = iface->get_func;

   if (!getter) goto not_found;

   reply = edbus_message_method_return_new(msg);
   EINA_SAFETY_ON_NULL_RETURN_VAL(reply, NULL);

   main_iter = edbus_message_iter_get(reply);
   variant = edbus_message_iter_container_new(main_iter, 'v',
                                              prop->property->type);

   ret = getter(iface, propname, variant, msg, &error_reply);

   if (ret)
     {
        edbus_message_iter_container_close(main_iter, variant);
        return reply;
     }

   edbus_message_unref(reply);
   return error_reply;

not_found:
   return edbus_message_error_new(msg, DBUS_ERROR_UNKNOWN_PROPERTY,
                                  "Property not found.");
}

static Eina_Bool
_props_getall(EDBus_Service_Interface *iface, Eina_Iterator *iterator, EDBus_Message_Iter *dict, const EDBus_Message *input_msg, EDBus_Message **error_reply)
{
   Property *prop;
   EINA_ITERATOR_FOREACH(iterator, prop)
     {
        EDBus_Message_Iter *entry, *var;
        Eina_Bool ret;
        EDBus_Property_Get_Cb getter = NULL;

        if (prop->property->get_func)
          getter = prop->property->get_func;
        else if (iface->get_func)
          getter = iface->get_func;

        if (!getter || prop->is_invalidate)
          continue;

        if (!edbus_message_iter_arguments_append(dict, "{sv}", &entry))
          continue;

        edbus_message_iter_basic_append(entry, 's', prop->property->name);
        var = edbus_message_iter_container_new(entry, 'v',
                                               prop->property->type);

        ret = getter(iface, prop->property->name, var, input_msg, error_reply);
        if (!ret)
          return EINA_FALSE;

        edbus_message_iter_container_close(entry, var);
        edbus_message_iter_container_close(dict, entry);
     }
   return EINA_TRUE;
}

static EDBus_Message *
_cb_property_getall(const EDBus_Service_Interface *piface, const EDBus_Message *msg)
{
   const char *iface_name;
   EDBus_Service_Object *obj = piface->obj;
   EDBus_Service_Interface *iface;
   Eina_Iterator *iterator;
   EDBus_Message *reply, *error_reply;
   EDBus_Message_Iter *main_iter, *dict;

   if (!edbus_message_arguments_get(msg, "s", &iface_name))
     return NULL;

   iface = eina_hash_find(obj->interfaces, iface_name);
   if (!iface)
     return edbus_message_error_new(msg, DBUS_ERROR_UNKNOWN_INTERFACE,
                                    "Interface not found.");

   reply = edbus_message_method_return_new(msg);
   EINA_SAFETY_ON_NULL_RETURN_VAL(reply, NULL);
   main_iter = edbus_message_iter_get(reply);
   if (!edbus_message_iter_arguments_append(main_iter, "a{sv}", &dict))
     {
        edbus_message_unref(reply);
        return NULL;
     }

   iterator = eina_hash_iterator_data_new(iface->properties);
   if (!_props_getall(iface, iterator, dict, msg, &error_reply))
     {
        edbus_message_unref(reply);
        eina_iterator_free(iterator);
        return error_reply;
     }
   edbus_message_iter_container_close(main_iter, dict);

   eina_iterator_free(iterator);
   return reply;
}

static EDBus_Message *
_cb_property_set(const EDBus_Service_Interface *piface, const EDBus_Message *msg)
{
   const char *propname, *iface_name;
   EDBus_Service_Object *obj = piface->obj;
   EDBus_Service_Interface *iface;
   Property *prop;
   EDBus_Message *reply;
   EDBus_Message_Iter *variant;
   EDBus_Property_Set_Cb setter = NULL;

   if (!edbus_message_arguments_get(msg, "ssv", &iface_name, &propname, &variant))
     return NULL;

   iface = eina_hash_find(obj->interfaces, iface_name);
   if (!iface)
     return edbus_message_error_new(msg, DBUS_ERROR_UNKNOWN_INTERFACE,
                                    "Interface not found.");

   prop = eina_hash_find(iface->properties, propname);
   if (!prop || prop->is_invalidate)
     return edbus_message_error_new(msg, DBUS_ERROR_UNKNOWN_PROPERTY,
                                    "Property not found.");

   if (prop->property->set_func)
     setter = prop->property->set_func;
   else if (iface->set_func)
     setter = iface->set_func;

   if (!setter)
     return edbus_message_error_new(msg, DBUS_ERROR_PROPERTY_READ_ONLY,
                                    "This property is read only");

   reply = setter(iface, propname, variant, msg);
   return reply;
}

static EDBus_Message *
cb_introspect(const EDBus_Service_Interface *_iface, const EDBus_Message *message)
{
   EDBus_Service_Object *obj = _iface->obj;
   EDBus_Message *reply = edbus_message_method_return_new(message);
   if (obj->introspection_dirty || !obj->introspection_data)
     {
        Eina_Iterator *iterator;
        EDBus_Service_Interface *iface;
        EDBus_Service_Object *child;
        size_t baselen;

        if (obj->introspection_data)
          eina_strbuf_reset(obj->introspection_data);
        else
          obj->introspection_data = eina_strbuf_new();
        EINA_SAFETY_ON_NULL_RETURN_VAL(obj->introspection_data, NULL);

        eina_strbuf_append(obj->introspection_data, "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\" \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">");
        eina_strbuf_append_printf(obj->introspection_data,
                                  "<node name=\"%s\">", obj->path);

        iterator = eina_hash_iterator_data_new(obj->interfaces);
        EINA_ITERATOR_FOREACH(iterator, iface)
          _introspect_append_interface(obj->introspection_data, iface);
        eina_iterator_free(iterator);

        baselen = strlen(obj->path);
        /* account for the last '/' */
        if (baselen != 1)
          baselen++;

        EINA_INLIST_FOREACH(obj->children, child)
           eina_strbuf_append_printf(obj->introspection_data,
                                     "<node name=\"%s\" />",
                                     child->path + baselen);

        eina_strbuf_append(obj->introspection_data, "</node>");
        obj->introspection_dirty = EINA_FALSE;
     }

   edbus_message_arguments_append(reply, "s", eina_strbuf_string_get(obj->introspection_data));
   return reply;
}

static const EDBus_Method introspect = {
   "Introspect", NULL, EDBUS_ARGS({ "s", "xml" }), cb_introspect
};

static void
_introspectable_create(void)
{
   introspectable = calloc(1, sizeof(EDBus_Service_Interface));
   EINA_SAFETY_ON_NULL_RETURN(introspectable);

   EINA_MAGIC_SET(introspectable, EDBUS_SERVICE_INTERFACE_MAGIC);
   introspectable->sign_of_signals = eina_array_new(1);
   introspectable->properties = eina_hash_string_small_new(NULL);
   introspectable->name = EDBUS_FDO_INTERFACE_INTROSPECTABLE;
   introspectable->methods = eina_hash_string_small_new(NULL);

   eina_hash_add(introspectable->methods, introspect.member, &introspect);
}

static void
_default_interfaces_free(void)
{
   eina_hash_free(introspectable->methods);
   eina_hash_free(introspectable->properties);
   eina_array_free(introspectable->sign_of_signals);
   free(introspectable);

   eina_hash_free(properties_iface->methods);
   eina_hash_free(properties_iface->properties);
   eina_array_free(properties_iface->sign_of_signals);
   free(properties_iface);

   eina_hash_free(objmanager->methods);
   eina_hash_free(objmanager->properties);
   eina_array_free(objmanager->sign_of_signals);
   free(objmanager);
}

static const EDBus_Method _property_methods[] = {
   {
    "Get", EDBUS_ARGS({"s", "interface"}, {"s", "property"}),
    EDBUS_ARGS({"v", "value"}), _cb_property_get
   },
   {
    "Set", EDBUS_ARGS({"s", "interface"}, {"s", "property"}, {"v", "value"}),
    NULL, _cb_property_set
   },
   {
    "GetAll", EDBUS_ARGS({"s", "interface"}), EDBUS_ARGS({"a{sv}", "props"}),
    _cb_property_getall
   }
};

static const EDBus_Signal _properties_signals[] = {
   {
    "PropertiesChanged",
    EDBUS_ARGS({"s", "interface"}, {"a{sv}", "changed_properties"}, {"as", "invalidated_properties"})
   }
};

static void
_properties_create(void)
{
   properties_iface = calloc(1, sizeof(EDBus_Service_Interface));
   if (!properties_iface) return;

   properties_iface->sign_of_signals = eina_array_new(1);
   properties_iface->properties =  eina_hash_string_small_new(NULL);
   properties_iface->name = EDBUS_FDO_INTERFACE_PROPERTIES;
   properties_iface->methods = eina_hash_string_small_new(NULL);
   EINA_MAGIC_SET(properties_iface, EDBUS_SERVICE_INTERFACE_MAGIC);

   eina_hash_add(properties_iface->methods, _property_methods[0].member,
                 &_property_methods[0]);
   eina_hash_add(properties_iface->methods, _property_methods[1].member,
                 &_property_methods[1]);
   eina_hash_add(properties_iface->methods, _property_methods[2].member,
                 &_property_methods[2]);

   properties_iface->signals = _properties_signals;
   eina_array_push(properties_iface->sign_of_signals, "sa{sv}as");
}

static Eina_Bool
_propmgr_iface_props_append(EDBus_Service_Interface *iface, EDBus_Message_Iter *array)
{
   EDBus_Message_Iter *iface_entry, *props_array;
   Eina_Iterator *iterator;
   EDBus_Message *error_msg;

   edbus_message_iter_arguments_append(array, "{sa{sv}}", &iface_entry);

   edbus_message_iter_arguments_append(iface_entry, "sa{sv}", iface->name, &props_array);
   iterator = eina_hash_iterator_data_new(iface->properties);
   if (!_props_getall(iface, iterator, props_array, NULL, &error_msg))
     {
        ERR("Error reply was set without pass any input message.");
        edbus_message_unref(error_msg);
        eina_iterator_free(iterator);
        return EINA_FALSE;
     }
   eina_iterator_free(iterator);
   edbus_message_iter_container_close(iface_entry, props_array);
   edbus_message_iter_container_close(array, iface_entry);
   return EINA_TRUE;
}

static Eina_Bool
_managed_obj_append(EDBus_Service_Object *obj, EDBus_Message_Iter *array, Eina_Bool first)
{
   EDBus_Message_Iter *obj_entry, *array_interface;
   Eina_Iterator *iface_iter;
   EDBus_Service_Interface *children_iface;
   EDBus_Service_Object *children;

   if (first) goto foreach;
   if (obj->has_objectmanager) return EINA_TRUE;

   edbus_message_iter_arguments_append(array, "{oa{sa{sv}}}", &obj_entry);
   edbus_message_iter_arguments_append(obj_entry, "oa{sa{sv}}", obj->path,
                                    &array_interface);
   iface_iter = eina_hash_iterator_data_new(obj->interfaces);
   EINA_ITERATOR_FOREACH(iface_iter, children_iface)
     {
        Eina_Bool ret;
        ret = _propmgr_iface_props_append(children_iface, array_interface);
        if (ret) continue;

        eina_iterator_free(iface_iter);
        return EINA_FALSE;
     }
   eina_iterator_free(iface_iter);
   edbus_message_iter_container_close(obj_entry, array_interface);
   edbus_message_iter_container_close(array, obj_entry);

foreach:
   EINA_INLIST_FOREACH(obj->children, children)
     {
        Eina_Bool ret;
        ret = _managed_obj_append(children, array, EINA_FALSE);
        if (!ret) return EINA_FALSE;
     }
   return EINA_TRUE;
}

static EDBus_Message *
_cb_managed_objects(const EDBus_Service_Interface *iface, const EDBus_Message *msg)
{
   EDBus_Message *reply = edbus_message_method_return_new(msg);
   EDBus_Message_Iter *array_path, *main_iter;
   Eina_Bool ret;

   EINA_SAFETY_ON_NULL_RETURN_VAL(reply, NULL);
   main_iter =  edbus_message_iter_get(reply);
   edbus_message_iter_arguments_append(main_iter, "a{oa{sa{sv}}}", &array_path);

   ret = _managed_obj_append(iface->obj, array_path, EINA_TRUE);
   if (!ret)
     {
        edbus_message_unref(reply);
        return edbus_message_error_new(msg, "org.freedesktop.DBus.Error",
                                       "Irrecoverable error happen");
     }

   edbus_message_iter_container_close(main_iter, array_path);
   return reply;
}

static EDBus_Method get_managed_objects = {
   "GetManagedObjects", NULL, EDBUS_ARGS({"a{oa{sa{sv}}}", "objects"}),
   _cb_managed_objects
};

static const EDBus_Signal _object_manager_signals[] = {
   {
    "InterfacesAdded", EDBUS_ARGS({"o", "object"}, {"a{sa{sv}}", "interfaces"})
   },
   {
    "InterfacesRemoved", EDBUS_ARGS({"o", "object"}, {"as", "interfaces"})
   }
};

static void
_object_manager_create(void)
{
   objmanager = calloc(1, sizeof(EDBus_Service_Interface));
   if (!objmanager) return;

   EINA_MAGIC_SET(objmanager, EDBUS_SERVICE_INTERFACE_MAGIC);
   objmanager->sign_of_signals = eina_array_new(1);
   objmanager->properties = eina_hash_string_small_new(NULL);
   objmanager->name = EDBUS_FDO_INTERFACE_OBJECT_MANAGER;
   objmanager->methods = eina_hash_string_small_new(NULL);

   eina_hash_add(objmanager->methods, get_managed_objects.member,
                 &get_managed_objects);

   objmanager->signals = _object_manager_signals;
   eina_array_push(objmanager->sign_of_signals, "oa{sa{sv}}");
   eina_array_push(objmanager->sign_of_signals, "oas");
}

Eina_Bool
edbus_service_init(void)
{
   _introspectable_create();
   EINA_SAFETY_ON_NULL_RETURN_VAL(introspectable, EINA_FALSE);
   _properties_create();
   EINA_SAFETY_ON_NULL_RETURN_VAL(properties_iface, EINA_FALSE);
   _object_manager_create();
   EINA_SAFETY_ON_NULL_RETURN_VAL(objmanager, EINA_FALSE);

   return EINA_TRUE;
}

void
edbus_service_shutdown(void)
{
   _default_interfaces_free();
}

static EDBus_Service_Object *
_edbus_service_object_parent_find(EDBus_Service_Object *obj)
{
   EDBus_Service_Object *parent = NULL;
   size_t len = strlen(obj->path);
   char *path = strdup(obj->path);
   char *slash;

   for (slash = path[len] != '/' ? &path[len - 1] : &path[len - 2];
        slash > path; slash--)
     {
        if (*slash != '/')
          continue;

        *slash = '\0';

        if (dbus_connection_get_object_path_data(obj->conn->dbus_conn,
                                     path, (void **) &parent) && parent != NULL)
          break;
     }

   free(path);
   return parent;
}

static EDBus_Service_Object *
_edbus_service_object_add(EDBus_Connection *conn, const char *path)
{
   EDBus_Service_Object *obj, *rootobj;
   Eina_Inlist *safe;
   size_t pathlen;

   obj = calloc(1, sizeof(EDBus_Service_Object));
   EINA_SAFETY_ON_NULL_RETURN_VAL(obj, NULL);

   if (!dbus_connection_register_object_path(conn->dbus_conn, path, &vtable,
                                             obj))
     {
        free(obj);
        return NULL;
     }

   obj->conn = conn;
   obj->path = eina_stringshare_add(path);
   obj->interfaces = eina_hash_string_superfast_new(NULL);
   edbus_connection_free_cb_add(conn, _on_connection_free, obj);

   eina_hash_add(obj->interfaces, introspectable->name, introspectable);
   eina_hash_add(obj->interfaces, properties_iface->name, properties_iface);

   obj->parent = _edbus_service_object_parent_find(obj);
   if (obj->parent)
     {
        obj->parent->children = eina_inlist_append(obj->parent->children,
                                                   EINA_INLIST_GET(obj));
        return obj;
     }

   /*
    * If there wasn't any object above us, check if anyone in conn->root_obj
    * should become our child and append ourselves there.
    */
   pathlen = strlen(obj->path);
   EINA_INLIST_FOREACH_SAFE(conn->root_objs, safe, rootobj)
     {
        if (strncmp(obj->path,  rootobj->path, pathlen) != 0)
          continue;

        if (rootobj->path[pathlen] != '/' && pathlen > 1)
          continue;

        conn->root_objs = eina_inlist_remove(conn->root_objs,
                                             EINA_INLIST_GET(rootobj));
        obj->children = eina_inlist_append(obj->children,
                                           EINA_INLIST_GET(rootobj));
        rootobj->parent = obj;
     }
   conn->root_objs = eina_inlist_append(conn->root_objs, EINA_INLIST_GET(obj));

   return obj;
}

static void
_props_free(void *data)
{
   Property *p = data;
   free(p);
}

struct iface_remove_data {
   const char *obj_path;
   const char *iface;
};

static Eina_Bool
_iface_changed_send(void *data)
{
   EDBus_Service_Object *parent = data;

   while (parent->iface_added)
     {
        EDBus_Service_Interface *iface, *next_iface;
        EDBus_Message *msg;
        EDBus_Message_Iter *array_iface, *main_iter;
        Eina_List *l, *l2;

        iface = eina_list_data_get(parent->iface_added);
        parent->iface_added = eina_list_remove_list(parent->iface_added,
                                                    parent->iface_added);

        msg = edbus_message_signal_new(parent->path,
                                       EDBUS_FDO_INTERFACE_OBJECT_MANAGER,
                                       "InterfacesAdded");
        if (!msg)
          {
             ERR("msg == NULL");
             continue;
          }
        main_iter = edbus_message_iter_get(msg);
        edbus_message_iter_arguments_append(main_iter, "oa{sa{sv}}",
                                         iface->obj->path, &array_iface);
        if (!_propmgr_iface_props_append(iface, array_iface))
          goto error;
        EINA_LIST_FOREACH_SAFE(parent->iface_added, l, l2, next_iface)
          {
             if (iface->obj->path != next_iface->obj->path)
               continue;
             parent->iface_added = eina_list_remove(parent->iface_added,
                                                    next_iface);
             if (!_propmgr_iface_props_append(next_iface, array_iface))
               goto error;
          }

        edbus_message_iter_container_close(main_iter, array_iface);
        edbus_connection_send(parent->conn, msg, NULL, NULL, -1);
        continue;
error:
        ERR("Error appending InterfacesAdded to msg.");
        edbus_message_unref(msg);
     }

   while (parent->iface_removed)
     {
        EDBus_Message *msg;
        EDBus_Message_Iter *array_iface, *main_iter;
        struct iface_remove_data *iface_data, *iface_data_next;
        Eina_List *l, *l2;

        iface_data = eina_list_data_get(parent->iface_removed);
        parent->iface_removed = eina_list_remove_list(parent->iface_removed,
                                                      parent->iface_removed);

        msg = edbus_message_signal_new(parent->path,
                                       EDBUS_FDO_INTERFACE_OBJECT_MANAGER,
                                       "InterfacesRemoved");
        EINA_SAFETY_ON_NULL_GOTO(msg, error2);
        main_iter = edbus_message_iter_get(msg);

        edbus_message_iter_arguments_append(main_iter, "oas", iface_data->obj_path,
                                         &array_iface);
        edbus_message_iter_basic_append(array_iface, 's', iface_data->iface);

        EINA_LIST_FOREACH_SAFE(parent->iface_removed, l, l2, iface_data_next)
          {
             if (iface_data->obj_path != iface_data_next->obj_path)
               continue;
             parent->iface_removed = eina_list_remove(parent->iface_removed,
                                                      iface_data_next);
             edbus_message_iter_basic_append(array_iface, 's',
                                             iface_data_next->iface);
             eina_stringshare_del(iface_data_next->iface);
             eina_stringshare_del(iface_data_next->obj_path);
             free(iface_data_next);
          }
        edbus_message_iter_container_close(main_iter, array_iface);
        edbus_connection_send(parent->conn, msg, NULL, NULL, -1);
error2:
        eina_stringshare_del(iface_data->iface);
        eina_stringshare_del(iface_data->obj_path);
        free(iface_data);
     }

   parent->idler_iface_changed = NULL;
   return EINA_FALSE;
}

static EDBus_Service_Object *
_find_object_manager_parent(EDBus_Service_Object *obj)
{
   if (!obj->parent)
     return NULL;
   if (obj->parent->has_objectmanager)
     return obj->parent;
   return _find_object_manager_parent(obj->parent);
}

static EDBus_Service_Interface *
_edbus_service_interface_add(EDBus_Service_Object *obj, const char *interface)
{
   EDBus_Service_Interface *iface;
   EDBus_Service_Object *parent;

   iface = eina_hash_find(obj->interfaces, interface);
   if (iface) return iface;

   iface = calloc(1, sizeof(EDBus_Service_Interface));
   EINA_SAFETY_ON_NULL_RETURN_VAL(iface, NULL);

   EINA_MAGIC_SET(iface, EDBUS_SERVICE_INTERFACE_MAGIC);
   iface->name = eina_stringshare_add(interface);
   iface->methods = eina_hash_string_superfast_new(NULL);
   iface->properties = eina_hash_string_superfast_new(_props_free);
   iface->obj = obj;
   eina_hash_add(obj->interfaces, iface->name, iface);

   parent = _find_object_manager_parent(obj);
   if (parent)
     {
        if (!parent->idler_iface_changed)
          parent->idler_iface_changed = ecore_idler_add(_iface_changed_send,
                                                        parent);
        parent->iface_added = eina_list_append(parent->iface_added, iface);
     }
   return iface;
}

static Eina_Bool
_have_signature(const EDBus_Arg_Info *args, EDBus_Message *msg)
{
   const char *sig = dbus_message_get_signature(msg->dbus_msg);
   const char *p = NULL;

   for (; args && args->signature && *sig; args++)
     {
        p = args->signature;
        for (; *sig && *p; sig++, p++)
          {
             if (*p != *sig)
               return EINA_FALSE;
          }
     }

   if (*sig || (p && *p) || (args && args->signature))
     return EINA_FALSE;

   return EINA_TRUE;
}

static Eina_Bool
_edbus_service_method_add(EDBus_Service_Interface *interface,
                          const EDBus_Method *method)
{
   EINA_SAFETY_ON_TRUE_RETURN_VAL(!!eina_hash_find(interface->methods,
                                  method->member), EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(method->member, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(method->cb, EINA_FALSE);

   eina_hash_add(interface->methods, method->member, method);
   return EINA_TRUE;
}

static Eina_Bool
_edbus_service_property_add(EDBus_Service_Interface *interface,
                            const EDBus_Property *property)
{
   Property *p;
   EINA_SAFETY_ON_TRUE_RETURN_VAL(!!eina_hash_find(interface->properties,
                                  property->name), EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(property->type, EINA_FALSE);
   EINA_SAFETY_ON_FALSE_RETURN_VAL(
      dbus_signature_validate_single(property->type, NULL), EINA_FALSE);

   p = calloc(1, sizeof(Property));
   EINA_SAFETY_ON_NULL_RETURN_VAL(p, EINA_FALSE);
   p->property = property;

   return eina_hash_add(interface->properties, property->name, p);
}

/* Check if all signals in desc have valid signatures and return an Eina_Array
 * with each of them. Return NULL if any of the signatures is invalid */
static inline Eina_Array *
_edbus_service_interface_desc_signals_signatures_get(
   const EDBus_Service_Interface_Desc *desc)
{
   const EDBus_Signal *sig;
   Eina_Strbuf *buf = eina_strbuf_new();
   Eina_Array *signatures = eina_array_new(1);

   EINA_SAFETY_ON_NULL_RETURN_VAL(buf, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(signatures, NULL);

   for (sig = desc->signals; sig && sig->name; sig++)
     {
        const EDBus_Arg_Info *arg;

        eina_strbuf_reset(buf);
        for (arg = sig->args; arg && arg->signature; arg++)
          eina_strbuf_append(buf, arg->signature);

        if (!dbus_signature_validate(eina_strbuf_string_get(buf), NULL))
          {
             ERR("Signal with invalid signature: interface=%s signal=%s",
                 desc->interface, sig->name);
             goto fail_signature;
          }

	eina_array_push(signatures,
                        eina_stringshare_add(eina_strbuf_string_get(buf)));
     }
   eina_strbuf_free(buf);

   return signatures;

fail_signature:
   eina_strbuf_free(buf);
   eina_array_free(signatures);
   return NULL;
}

EAPI EDBus_Service_Interface *
edbus_service_interface_register(EDBus_Connection *conn, const char *path, const EDBus_Service_Interface_Desc *desc)
{
   EDBus_Service_Object *obj;
   EDBus_Service_Interface *iface;
   const EDBus_Method *method;
   const EDBus_Property *property;
   Eina_Array *signatures;

   EINA_SAFETY_ON_NULL_RETURN_VAL(conn, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(path, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(desc, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(desc->interface, EINA_FALSE);

   if (!dbus_connection_get_object_path_data(conn->dbus_conn, path,
                                             (void*)&obj))
     {
        ERR("Invalid object path");
        return NULL;
     }

   signatures = _edbus_service_interface_desc_signals_signatures_get(desc);
   if (!signatures)
     return NULL;

   if (!obj)
     obj = _edbus_service_object_add(conn, path);
   else
     obj->introspection_dirty = EINA_TRUE;
   EINA_SAFETY_ON_NULL_GOTO(obj, fail);

   iface = _edbus_service_interface_add(obj, desc->interface);
   if (!iface)
     goto fail;

   for (method = desc->methods; method && method->member; method++)
     _edbus_service_method_add(iface, method);

   iface->signals = desc->signals;
   iface->sign_of_signals = signatures;

   for (property = desc->properties; property && property->name; property++)
     _edbus_service_property_add(iface, property);

   iface->get_func = desc->default_get;
   iface->set_func = desc->default_set;

   return iface;

fail:
   eina_array_free(signatures);

   if (obj && (eina_hash_population(obj->interfaces) < 2))
     _object_free(obj);

   return NULL;
}

static void
_interface_free(EDBus_Service_Interface *interface)
{
   const char *sig;
   EDBus_Service_Object *parent;
   if (interface == introspectable || interface == properties_iface ||
       interface == objmanager)
     return;

   eina_hash_free(interface->methods);
   while ((sig = eina_array_pop(interface->sign_of_signals)))
     eina_stringshare_del(sig);
   eina_array_free(interface->sign_of_signals);
   eina_hash_free(interface->properties);
   if (interface->props_changed)
     eina_array_free(interface->props_changed);
   if (interface->idler_propschanged)
     ecore_idler_del(interface->idler_propschanged);
   if (interface->prop_invalidated)
     eina_array_free(interface->prop_invalidated);

   parent = _find_object_manager_parent(interface->obj);
   if (parent)
     {
        struct iface_remove_data *data;

        data = malloc(sizeof(struct iface_remove_data));
        EINA_SAFETY_ON_NULL_GOTO(data, end);
        data->obj_path = eina_stringshare_add(interface->obj->path);
        data->iface = eina_stringshare_add(interface->name);

        if (!parent->idler_iface_changed)
          parent->idler_iface_changed = ecore_idler_add(_iface_changed_send,
                                                        parent);
        parent->iface_removed = eina_list_append(parent->iface_removed, data);
        parent->iface_added = eina_list_remove(parent->iface_added, interface);
     }
end:
   eina_stringshare_del(interface->name);
   free(interface);
}

static void
_object_free(EDBus_Service_Object *obj)
{
   Eina_Iterator *iterator;
   EDBus_Service_Interface *iface;
   struct iface_remove_data *data;

   iterator = eina_hash_iterator_data_new(obj->interfaces);
   EINA_ITERATOR_FOREACH(iterator, iface)
     _interface_free(iface);

   while (obj->children)
     {
        EDBus_Service_Object *child;
        child = EINA_INLIST_CONTAINER_GET(obj->children, EDBus_Service_Object);
        obj->children = eina_inlist_remove(obj->children, obj->children);
        if (obj->parent)
          {
             obj->parent->children = eina_inlist_append(obj->parent->children,
							EINA_INLIST_GET(child));
             child->parent = obj->parent;
          }
        else
          {
             obj->conn->root_objs = eina_inlist_append(obj->conn->root_objs,
                                                       EINA_INLIST_GET(child));
             child->parent = NULL;
          }
     }
   if (obj->parent)
     obj->parent->children = eina_inlist_remove(obj->parent->children,
						EINA_INLIST_GET(obj));
   else
     obj->conn->root_objs = eina_inlist_remove(obj->conn->root_objs,
                                               EINA_INLIST_GET(obj));

   edbus_data_del_all(&obj->data);

   EINA_LIST_FREE(obj->iface_removed, data)
     {
        eina_stringshare_del(data->iface);
        eina_stringshare_del(data->obj_path);
        free(data);
     }
   eina_list_free(obj->iface_added);
   if (obj->idler_iface_changed)
     ecore_idler_del(obj->idler_iface_changed);
   eina_hash_free(obj->interfaces);
   eina_iterator_free(iterator);
   if (obj->introspection_data)
     eina_strbuf_free(obj->introspection_data);
   eina_stringshare_del(obj->path);
   free(obj);
}

static void
_on_connection_free(void *data, const void *dead_pointer)
{
   EDBus_Service_Object *obj = data;
   dbus_connection_unregister_object_path(obj->conn->dbus_conn, obj->path);
}

EAPI void
edbus_service_interface_unregister(EDBus_Service_Interface *iface)
{
   EDBUS_SERVICE_INTERFACE_CHECK(iface);
   if (!eina_hash_find(iface->obj->interfaces, objmanager->name))
     {
        //properties + introspectable + iface that user wants unregister
        if (eina_hash_population(iface->obj->interfaces) < 4)
          edbus_service_object_unregister(iface);
        return;
     }
   eina_hash_del(iface->obj->interfaces, NULL, iface);
   iface->obj->introspection_dirty = EINA_TRUE;
   _interface_free(iface);
}

EAPI void
edbus_service_object_unregister(EDBus_Service_Interface *iface)
{
   EDBUS_SERVICE_INTERFACE_CHECK(iface);
   /*
    * It will be freed when _object_unregister() is called
    * by libdbus.
    */
   edbus_connection_free_cb_del(iface->obj->conn, _on_connection_free, iface->obj);
   dbus_connection_unregister_object_path(iface->obj->conn->dbus_conn, iface->obj->path);
}

static void
_object_unregister(DBusConnection *conn, void *user_data)
{
   EDBus_Service_Object *obj = user_data;
   _object_free(obj);
}

static DBusHandlerResult
_object_handler(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
   EDBus_Service_Object *obj;
   EDBus_Service_Interface *iface;
   const EDBus_Method *method;
   EDBus_Message *edbus_msg;
   EDBus_Message *reply;

   obj = user_data;
   if (!obj) return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

   DBG("Connection@%p Got message:\n"
          "  Type: %s\n"
          "  Path: %s\n"
          "  Interface: %s\n"
          "  Member: %s\n"
          "  Sender: %s", obj->conn,
          dbus_message_type_to_string(dbus_message_get_type(msg)),
          dbus_message_get_path(msg),
          dbus_message_get_interface(msg),
          dbus_message_get_member(msg),
          dbus_message_get_sender(msg));

   iface = eina_hash_find(obj->interfaces, dbus_message_get_interface(msg));
   if (!iface) return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

   method = eina_hash_find(iface->methods, dbus_message_get_member(msg));
   if (!method) return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

   edbus_msg = edbus_message_new(EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(edbus_msg, DBUS_HANDLER_RESULT_NEED_MEMORY);
   edbus_msg->dbus_msg = msg;
   dbus_message_iter_init(edbus_msg->dbus_msg, &edbus_msg->iterator->dbus_iterator);

   if (!_have_signature(method->in, edbus_msg))
     {
        reply = edbus_message_error_new(edbus_msg,
                                        DBUS_ERROR_INVALID_SIGNATURE,
                                        "See introspectable to know the expected signature");
     }
   else
     {
        if (iface->obj)
          reply = method->cb(iface, edbus_msg);
        else
          {
             //if iface does have obj it is some of FreeDesktop interfaces:
             //Introspectable, Properties...
             EDBus_Service_Interface *cpy;
             cpy = calloc(1, sizeof(EDBus_Service_Interface));
             if (!cpy)
	       {
	          dbus_message_ref(edbus_msg->dbus_msg);
                  edbus_message_unref(edbus_msg);
                  return DBUS_HANDLER_RESULT_NEED_MEMORY;
               }
             cpy->obj = obj;
             reply = method->cb(cpy, edbus_msg);
             free(cpy);
          }
     }

   dbus_message_ref(edbus_msg->dbus_msg);
   edbus_message_unref(edbus_msg);
   if (!reply) return DBUS_HANDLER_RESULT_HANDLED;

   _edbus_connection_send(obj->conn, reply, NULL, NULL, -1);
   edbus_message_unref(reply);

   return DBUS_HANDLER_RESULT_HANDLED;
}

EAPI EDBus_Connection *
edbus_service_connection_get(const EDBus_Service_Interface *iface)
{
   EDBUS_SERVICE_INTERFACE_CHECK_RETVAL(iface, NULL);
   return iface->obj->conn;
}

EAPI const char *
edbus_service_object_path_get(const EDBus_Service_Interface *iface)
{
   EDBUS_SERVICE_INTERFACE_CHECK_RETVAL(iface, NULL);
   return iface->obj->path;
}

EAPI EDBus_Message *
edbus_service_signal_new(const EDBus_Service_Interface *iface, unsigned int signal_id)
{
   unsigned size;
   EDBUS_SERVICE_INTERFACE_CHECK_RETVAL(iface, EINA_FALSE);
   size = eina_array_count(iface->sign_of_signals);
   EINA_SAFETY_ON_FALSE_RETURN_VAL(signal_id < size, EINA_FALSE);

   return edbus_message_signal_new(iface->obj->path, iface->name,
                                   iface->signals[signal_id].name);
}

EAPI Eina_Bool
edbus_service_signal_emit(const EDBus_Service_Interface *iface, unsigned int signal_id, ...)
{
   EDBus_Message *sig;
   va_list ap;
   Eina_Bool r;
   const char *signature;
   unsigned size;

   EDBUS_SERVICE_INTERFACE_CHECK_RETVAL(iface, EINA_FALSE);
   size = eina_array_count(iface->sign_of_signals);
   EINA_SAFETY_ON_FALSE_RETURN_VAL(signal_id < size, EINA_FALSE);

   sig = edbus_service_signal_new(iface, signal_id);
   EINA_SAFETY_ON_NULL_RETURN_VAL(sig, EINA_FALSE);

   signature = eina_array_data_get(iface->sign_of_signals, signal_id);
   va_start(ap, signal_id);
   r = edbus_message_arguments_vappend(sig, signature, ap);
   va_end(ap);
   EINA_SAFETY_ON_FALSE_RETURN_VAL(r, EINA_FALSE);

   edbus_service_signal_send(iface, sig);
   return EINA_TRUE;
}

EAPI Eina_Bool
edbus_service_signal_send(const EDBus_Service_Interface *iface, EDBus_Message *signal_msg)
{
   EDBUS_SERVICE_INTERFACE_CHECK_RETVAL(iface, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(signal_msg, EINA_FALSE);
   _edbus_connection_send(iface->obj->conn, signal_msg, NULL, NULL, -1);
   edbus_message_unref(signal_msg);
   return EINA_TRUE;
}

EAPI void
edbus_service_object_data_set(EDBus_Service_Interface *iface, const char *key, const void *data)
{
   EDBUS_SERVICE_INTERFACE_CHECK(iface);
   EINA_SAFETY_ON_NULL_RETURN(key);
   EINA_SAFETY_ON_NULL_RETURN(data);
   edbus_data_set(&(iface->obj->data), key, data);
}

EAPI void *
edbus_service_object_data_get(const EDBus_Service_Interface *iface, const char *key)
{
   EDBUS_SERVICE_INTERFACE_CHECK_RETVAL(iface, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(key, NULL);
   return edbus_data_get(&(((EDBus_Service_Object *)iface->obj)->data), key);
}

EAPI void *
edbus_service_object_data_del(EDBus_Service_Interface *iface, const char *key)
{
   EDBUS_SERVICE_INTERFACE_CHECK_RETVAL(iface, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(key, NULL);
   return edbus_data_del(&(((EDBus_Service_Object *)iface->obj)->data), key);
}

static Eina_Bool
_idler_propschanged(void *data)
{
   EDBus_Service_Interface *iface = data;
   EDBus_Message *msg;
   EDBus_Message_Iter *main_iter, *dict, *array_invalidate;
   Eina_Hash *added = NULL;
   Property *prop;

   iface->idler_propschanged = NULL;

   added = eina_hash_string_small_new(NULL);
   msg = edbus_message_signal_new(iface->obj->path, properties_iface->name,
                                  properties_iface->signals[0].name);
   EINA_SAFETY_ON_NULL_GOTO(msg, error);

   main_iter = edbus_message_iter_get(msg);
   if (!edbus_message_iter_arguments_append(main_iter, "sa{sv}", iface->name, &dict))
     {
        edbus_message_unref(msg);
        goto error;
     }

   if (!iface->props_changed)
     goto invalidate;
   while ((prop = eina_array_pop(iface->props_changed)))
     {
        EDBus_Message_Iter *entry, *var;
        EDBus_Message *error_reply = NULL;
        Eina_Bool ret;
        EDBus_Property_Get_Cb getter = NULL;

        if (eina_hash_find(added, prop->property->name))
          continue;
        eina_hash_add(added, prop->property->name, prop);

        if (prop->property->get_func)
          getter = prop->property->get_func;
        else if (iface->get_func)
          getter = iface->get_func;

        if (!getter || prop->is_invalidate)
          continue;

        EINA_SAFETY_ON_FALSE_GOTO(
                edbus_message_iter_arguments_append(dict, "{sv}", &entry), error);

        edbus_message_iter_basic_append(entry, 's', prop->property->name);
        var = edbus_message_iter_container_new(entry, 'v',
                                               prop->property->type);

        ret = getter(iface, prop->property->name, var, NULL, &error_reply);
        if (!ret)
          {
             edbus_message_unref(msg);
             if (error_reply)
               {
                  ERR("Error reply was set without pass any input message.");
                  edbus_message_unref(error_reply);
               }
             ERR("Getter of property %s returned error.", prop->property->name);
             goto error;
          }

        edbus_message_iter_container_close(entry, var);
        edbus_message_iter_container_close(dict, entry);
     }
invalidate:
   edbus_message_iter_container_close(main_iter, dict);

   edbus_message_iter_arguments_append(main_iter, "as", &array_invalidate);

   if (!iface->prop_invalidated)
     goto end;
   while ((prop = eina_array_pop(iface->prop_invalidated)))
     {
        if (!prop->is_invalidate)
          continue;
        edbus_message_iter_basic_append(array_invalidate, 's',
                                        prop->property->name);
     }
end:
   edbus_message_iter_container_close(main_iter, array_invalidate);

   edbus_service_signal_send(iface, msg);
error:
   if (added)
     eina_hash_free(added);
   if (iface->props_changed)
     eina_array_flush(iface->props_changed);
   if (iface->prop_invalidated)
     eina_array_flush(iface->prop_invalidated);
   return ECORE_CALLBACK_CANCEL;
}

EAPI Eina_Bool
edbus_service_property_changed(const EDBus_Service_Interface *interface, const char *name)
{
   Property *prop;
   EDBus_Service_Interface *iface = (EDBus_Service_Interface *)interface;
   EDBUS_SERVICE_INTERFACE_CHECK_RETVAL(iface, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(name, EINA_FALSE);

   prop = eina_hash_find(iface->properties, name);
   EINA_SAFETY_ON_NULL_RETURN_VAL(prop, EINA_FALSE);

   if (!iface->idler_propschanged)
     iface->idler_propschanged = ecore_idler_add(_idler_propschanged, iface);
   if (!iface->props_changed)
     iface->props_changed = eina_array_new(1);

   return eina_array_push(iface->props_changed, prop);
}

EAPI Eina_Bool
edbus_service_property_invalidate_set(const EDBus_Service_Interface *interface, const char *name, Eina_Bool is_invalidate)
{
   Property *prop;
   EDBus_Service_Interface *iface = (EDBus_Service_Interface *) interface;

   EDBUS_SERVICE_INTERFACE_CHECK_RETVAL(iface, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(name, EINA_FALSE);

   prop = eina_hash_find(iface->properties, name);
   EINA_SAFETY_ON_NULL_RETURN_VAL(prop, EINA_FALSE);

   if (prop->is_invalidate == is_invalidate)
     return EINA_TRUE;

   prop->is_invalidate = is_invalidate;

   if (!iface->idler_propschanged)
     iface->idler_propschanged = ecore_idler_add(_idler_propschanged, iface);

   if (is_invalidate)
     {
        if (!iface->props_changed)
          iface->props_changed = eina_array_new(1);
        return eina_array_push(iface->props_changed, prop);
     }

   if (!iface->prop_invalidated)
     iface->prop_invalidated = eina_array_new(1);
   return eina_array_push(iface->prop_invalidated, prop);
}

EAPI Eina_Bool
edbus_service_object_manager_attach(EDBus_Service_Interface *iface)
{
   EDBus_Service_Object *obj;
   EDBUS_SERVICE_INTERFACE_CHECK_RETVAL(iface, EINA_FALSE);

   obj = iface->obj;
   if (!eina_hash_find(obj->interfaces, objmanager->name))
     if (!eina_hash_add(obj->interfaces, objmanager->name, objmanager))
       return EINA_FALSE;

   obj->has_objectmanager = EINA_TRUE;
   obj->introspection_dirty = EINA_TRUE;
   return EINA_TRUE;
}

EAPI Eina_Bool
edbus_service_object_manager_detach(EDBus_Service_Interface *iface)
{
   EDBus_Service_Object *obj;
   Eina_Bool ret;

   EDBUS_SERVICE_INTERFACE_CHECK_RETVAL(iface, EINA_FALSE);
   obj = iface->obj;
   ret = eina_hash_del(obj->interfaces, objmanager->name, NULL);
   obj->has_objectmanager = EINA_FALSE;
   obj->introspection_dirty = EINA_TRUE;
   //properties + introspectable
   if (eina_hash_population(iface->obj->interfaces) < 3)
     edbus_service_object_unregister(iface);
   return ret;
}
