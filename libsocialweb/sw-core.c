/*
 * libsocialweb - social data store
 * Copyright (C) 2008 - 2009 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <gio/gio.h>
#include <dbus/dbus-glib-lowlevel.h>
#include "sw-core.h"
#include "sw-utils.h"
#include "sw-online.h"
#include "sw-banned.h"
#include "sw-debug.h"
#include "sw-item.h"

#include "sw-client-monitor.h"

#include "sw-core-ginterface.h"


static void core_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (SwCore, sw_core, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (SW_TYPE_CORE_IFACE,
                                                core_iface_init));

#define GET_PRIVATE(o)                                                  \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_CORE, SwCorePrivate))

struct _SwCorePrivate {
  DBusGConnection *connection;
  /* Hash of service name to GType */
  GHashTable *service_types;
  /* Hash of service name to SwServiceProxies on the bus */
  GHashTable *bus_services;
  /* Hash of service name-parameter hash to SwService instance. */
  GHashTable *active_services;
  /* Hash of banned UID strings to a senitel (TODO: to timestamp) */
  GHashTable *banned_uids;
  /* Monitor for modules directory */
  GFileMonitor *modules_monitor;
};

typedef const gchar *(*SwModuleGetNameFunc)(void);
typedef const GType (*SwModuleGetTypeFunc)(void);

static void
get_services (SwCoreIface *self, DBusGMethodInvocation *context)
{
  SwCore *core = SW_CORE (self);
  SwCorePrivate *priv = core->priv;
  GPtrArray *array;
  GList *l;

  array = g_ptr_array_new ();

  l = g_hash_table_get_keys (priv->service_types);
  while (l) {
    g_ptr_array_add (array, l->data);
    l = g_list_delete_link (l, l);
  }
  g_ptr_array_add (array, NULL);

  sw_core_iface_return_from_get_services (context, (const gpointer)array->pdata);

  g_ptr_array_free (array, TRUE);
}

typedef struct {
  SwCore *core;
  char *key;
} WeakServiceData;

/* Online notifications */
static void
is_online (SwCoreIface *self, DBusGMethodInvocation *context)
{
  dbus_g_method_return (context, sw_is_online ());
}

static void
online_changed (gboolean online, gpointer user_data)
{
  SwCore *core = SW_CORE (user_data);

  sw_core_iface_emit_online_changed (core, online);
}

static void
load_module (SwCore *core, const char *file)
{
  SwCorePrivate *priv = core->priv;
  GModule *service_module;
  const gchar *service_name;
  GType service_type;
  gpointer sym;
  SwService *service;
  gchar *path;
  GError *error = NULL;

  service_module = g_module_open (file, G_MODULE_BIND_LOCAL);
  if (service_module == NULL) {
    g_critical (G_STRLOC ": error opening module: %s",
                g_module_error());
    g_module_close (service_module);
    return;
  }

  if (!g_module_symbol (service_module, "sw_module_get_name", &sym)) {
    g_critical ("Cannot get symbol sw_module_get_name: %s",
                g_module_error());
    g_module_close (service_module);
    return;
  } else {
    service_name = (*(SwModuleGetNameFunc)sym)();
  }

  /* Module already loaded */
  if (g_hash_table_lookup (priv->service_types, service_name))
  {
    SW_DEBUG (CORE, "Module %s already loaded.", service_name);
    g_module_close (service_module);
    return;
  }

  if (!g_module_symbol (service_module, "sw_module_get_type", &sym)) {
    g_critical ("Cannot get symbol sw_module_get_type: %s",
                g_module_error());
    g_module_close (service_module);
    return;
  } else {
    service_type = (*(SwModuleGetTypeFunc)sym)();
  }

  if (service_name && service_type) {
    g_module_make_resident (service_module);

    /* Create an instance and add it to the bus */
    service = g_object_new (service_type, NULL);

    if (G_IS_INITABLE (service)) {
      if (!g_initable_init (G_INITABLE (service), NULL, &error)) {
        g_message ("Cannot import %s: %s", service_name, error->message);
        g_error_free (error);
        g_object_unref (service);
        return;
      }
    }

    /* Add to the service name -> type hash */
    g_hash_table_insert (priv->service_types,
                         (char*)service_name,
                         GINT_TO_POINTER (service_type));

    g_hash_table_insert (priv->bus_services,
                         (gchar *)service_name,
                         service);
    path = g_strdup_printf ("/org/gnome/libsocialweb/Service/%s",
                            service_name);
    dbus_g_connection_register_g_object (priv->connection,
                                         path,
                                         (GObject*)service);
    g_free (path);
    g_message ("Imported module: %s", service_name);
  }
}

static void
load_modules_from_dir (SwCore *core)
{
  GFile *services_dir_file;
  GFileEnumerator *enumerator;
  GError *error = NULL;
  GFileInfo *fi;

  services_dir_file = g_file_new_for_path (SOCIALWEB_SERVICES_MODULES_DIR);

  enumerator = g_file_enumerate_children (services_dir_file,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME,
                                          G_FILE_QUERY_INFO_NONE,
                                          NULL,
                                          &error);
  g_object_unref (services_dir_file);

  if (!enumerator)
  {
    g_critical (G_STRLOC ": error whilst enumerating directory children: %s",
                error->message);
    g_clear_error (&error);
    return;
  }

  while ((fi = g_file_enumerator_next_file (enumerator, NULL, &error)) != NULL)
  {
    if (g_str_has_suffix (g_file_info_get_name (fi), ".so"))
    {
      char *module_path;

      module_path = g_build_filename (SOCIALWEB_SERVICES_MODULES_DIR,
                                      g_file_info_get_name (fi),
                                      NULL);
      load_module (core, module_path);
      g_free (module_path);
    }

    g_object_unref (fi);
  }

  if (error)
  {
    g_critical (G_STRLOC ": error whilst enumerating directory children: %s",
                error->message);
    g_clear_error (&error);
  }

  g_object_unref (enumerator);
}

static void
_modules_monitor_changed_cb (GFileMonitor      *monitor,
                             GFile             *file,
                             GFile             *other_file,
                             GFileMonitorEvent  event,
                             SwCore            *core)
{
  if (event == G_FILE_MONITOR_EVENT_CREATED)
  {
    load_modules_from_dir (core);
  }
}

static void
setup_modules_monitor (SwCore *core)
{
  SwCorePrivate *priv = core->priv;
  GFile *file;
  GError *error = NULL;

  file = g_file_new_for_path (SOCIALWEB_SERVICES_MODULES_DIR);

  priv->modules_monitor = g_file_monitor (file,
                                          G_FILE_MONITOR_NONE,
                                          NULL,
                                          &error);

  if (error)
  {
    g_critical (G_STRLOC ": error setting up modules directory monitor: %s",
                error->message);
    g_clear_error (&error);
  } else {
    g_signal_connect (priv->modules_monitor,
                      "changed",
                      (GCallback)_modules_monitor_changed_cb,
                      core);
  }

  g_object_unref (file);
}

static void
load_modules_from_string (SwCore *core, const char *s)
{
  char **modules, **basename;

  modules = g_strsplit (s, ",", 0);

  for (basename = modules; *basename; basename++) {
    char *name, *path;

    name = g_strconcat ("lib", *basename, NULL);
    path = g_build_filename (SOCIALWEB_SERVICES_MODULES_DIR, name, NULL);

    load_module (core, path);

    g_free (path);
    g_free (name);
  }

  g_strfreev (modules);
}

static void
sw_core_constructed (GObject *object)
{
  SwCore *core = SW_CORE (object);
  SwCorePrivate *priv = core->priv;
  GError *error = NULL;
  const char *modules;

  priv->banned_uids = sw_ban_load (NULL);

  priv->connection = dbus_g_bus_get (DBUS_BUS_STARTER, &error);
  if (error) {
    g_warning ("Failed to open connection to DBus: %s\n", error->message);
    g_error_free (error);
  }

  dbus_g_connection_register_g_object (priv->connection,
                                       "/org/gnome/libsocialweb",
                                       object);

  sw_client_monitor_init (priv->connection);

  modules = g_getenv ("SW_MODULES");
  if (modules) {
    load_modules_from_string (core, modules);
  } else {
    setup_modules_monitor (core);
    load_modules_from_dir (core);
  }

  sw_online_add_notify (online_changed, object);
}

static void
sw_core_dispose (GObject *object)
{
  SwCore *core = SW_CORE (object);
  SwCorePrivate *priv = core->priv;

  if (priv->modules_monitor)
  {
    g_object_unref (priv->modules_monitor);
    priv->modules_monitor = NULL;
  }

  G_OBJECT_CLASS (sw_core_parent_class)->dispose (object);
}

static void
sw_core_finalize (GObject *object)
{
  sw_online_remove_notify (online_changed, object);

  G_OBJECT_CLASS (sw_core_parent_class)->finalize (object);
}

static void
core_iface_init (gpointer g_iface, gpointer iface_data)
{
  SwCoreIfaceClass *klass = (SwCoreIfaceClass*)g_iface;

  sw_core_iface_implement_get_services (klass, get_services);
  sw_core_iface_implement_is_online (klass, is_online);
}

static void
sw_core_class_init (SwCoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SwCorePrivate));

  object_class->constructed = sw_core_constructed;
  object_class->dispose = sw_core_dispose;
  object_class->finalize = sw_core_finalize;
}

static void
sw_core_init (SwCore *self)
{
  SwCorePrivate *priv = GET_PRIVATE (self);

  self->priv = priv;

  priv->service_types = g_hash_table_new (g_str_hash, g_str_equal);

  priv->bus_services = g_hash_table_new_full (g_str_hash, g_str_equal,
                                              NULL, g_object_unref);

  priv->active_services = g_hash_table_new_full (g_str_hash, g_str_equal,
                                              g_free, g_object_unref);
}

SwCore*
sw_core_new (void)
{
  return g_object_new (SW_TYPE_CORE, NULL);
}

void
sw_core_run (SwCore *core)
{
  GMainLoop *loop;

  g_return_if_fail (SW_IS_CORE (core));

  loop = g_main_loop_new (NULL, TRUE);
  g_main_loop_run (loop);
  g_main_loop_unref (loop);
}

gboolean
sw_core_is_item_banned (SwCore *core,
                        SwItem *item)
{
  const char *id;

  g_return_val_if_fail (SW_IS_CORE (core), FALSE);
  g_return_val_if_fail (SW_IS_ITEM (item), FALSE);

  id = sw_item_get (item, "id");

  return g_hash_table_lookup (core->priv->banned_uids, id) != NULL;
}

SwCore *
sw_core_dup_singleton (void)
{
  static SwCore *core = NULL;

  if (!core)
  {
    core = sw_core_new ();
    g_object_add_weak_pointer (G_OBJECT (core), (gpointer)&core);

    return core;
  } else {
    return g_object_ref (core);
  }
}

DBusGConnection *
sw_core_get_connection (SwCore *core)
{
  SwCorePrivate *priv = GET_PRIVATE (core);

  return priv->connection;
}
