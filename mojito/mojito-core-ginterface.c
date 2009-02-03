#include "mojito-core-ginterface.h"

#include "mojito-marshals.h"

static const DBusGObjectInfo _mojito_core_iface_object_info;

struct _MojitoCoreIfaceClass {
    GTypeInterface parent_class;
    mojito_core_iface_get_services_impl get_services;
    mojito_core_iface_open_view_impl open_view;
};

static void mojito_core_iface_base_init (gpointer klass);

GType
mojito_core_iface_get_type (void)
{
  static GType type = 0;

  if (G_UNLIKELY (type == 0))
    {
      static const GTypeInfo info = {
        sizeof (MojitoCoreIfaceClass),
        mojito_core_iface_base_init, /* base_init */
        NULL, /* base_finalize */
        NULL, /* class_init */
        NULL, /* class_finalize */
        NULL, /* class_data */
        0,
        0, /* n_preallocs */
        NULL /* instance_init */
      };

      type = g_type_register_static (G_TYPE_INTERFACE,
          "MojitoCoreIface", &info, 0);
    }

  return type;
}

/**
 * mojito_core_iface_get_services_impl:
 * @self: The object implementing this interface
 * @context: Used to return values or throw an error
 *
 * The signature of an implementation of the D-Bus method
 * GetServices on interface com.intel.Mojito.
 */
static void
mojito_core_iface_get_services (MojitoCoreIface *self,
    DBusGMethodInvocation *context)
{
  mojito_core_iface_get_services_impl impl = (MOJITO_CORE_IFACE_GET_CLASS (self)->get_services);

  if (impl != NULL)
    {
      (impl) (self,
        context);
    }
  else
    {
      GError e = { DBUS_GERROR, 
           DBUS_GERROR_UNKNOWN_METHOD,
           "Method not implemented" };

      dbus_g_method_return_error (context, &e);
    }
}

/**
 * mojito_core_iface_implement_get_services:
 * @klass: A class whose instances implement this interface
 * @impl: A callback used to implement the GetServices D-Bus method
 *
 * Register an implementation for the GetServices method in the vtable
 * of an implementation of this interface. To be called from
 * the interface init function.
 */
void
mojito_core_iface_implement_get_services (MojitoCoreIfaceClass *klass, mojito_core_iface_get_services_impl impl)
{
  klass->get_services = impl;
}

/**
 * mojito_core_iface_open_view_impl:
 * @self: The object implementing this interface
 * @in_services: const gchar ** (FIXME, generate documentation)
 * @in_count: guint  (FIXME, generate documentation)
 * @context: Used to return values or throw an error
 *
 * The signature of an implementation of the D-Bus method
 * OpenView on interface com.intel.Mojito.
 */
static void
mojito_core_iface_open_view (MojitoCoreIface *self,
    const gchar **in_services,
    guint in_count,
    DBusGMethodInvocation *context)
{
  mojito_core_iface_open_view_impl impl = (MOJITO_CORE_IFACE_GET_CLASS (self)->open_view);

  if (impl != NULL)
    {
      (impl) (self,
        in_services,
        in_count,
        context);
    }
  else
    {
      GError e = { DBUS_GERROR, 
           DBUS_GERROR_UNKNOWN_METHOD,
           "Method not implemented" };

      dbus_g_method_return_error (context, &e);
    }
}

/**
 * mojito_core_iface_implement_open_view:
 * @klass: A class whose instances implement this interface
 * @impl: A callback used to implement the OpenView D-Bus method
 *
 * Register an implementation for the OpenView method in the vtable
 * of an implementation of this interface. To be called from
 * the interface init function.
 */
void
mojito_core_iface_implement_open_view (MojitoCoreIfaceClass *klass, mojito_core_iface_open_view_impl impl)
{
  klass->open_view = impl;
}

static inline void
mojito_core_iface_base_init_once (gpointer klass G_GNUC_UNUSED)
{
  dbus_g_object_type_install_info (mojito_core_iface_get_type (),
      &_mojito_core_iface_object_info);
}
static void
mojito_core_iface_base_init (gpointer klass)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      initialized = TRUE;
      mojito_core_iface_base_init_once (klass);
    }
}
static const DBusGMethodInfo _mojito_core_iface_methods[] = {
  { (GCallback) mojito_core_iface_get_services, g_cclosure_marshal_VOID__POINTER, 0 },
  { (GCallback) mojito_core_iface_open_view, mojito_marshal_VOID__BOXED_UINT_POINTER, 50 },
};

static const DBusGObjectInfo _mojito_core_iface_object_info = {
  0,
  _mojito_core_iface_methods,
  2,
"com.intel.Mojito\0GetServices\0A\0services\0O\0F\0N\0as\0\0com.intel.Mojito\0OpenView\0A\0services\0I\0as\0count\0I\0u\0view\0O\0F\0N\0o\0\0\0",
"\0\0",
"\0\0",
};


