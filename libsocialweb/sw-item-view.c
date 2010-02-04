/*
 * libsocialweb - social data store
 * Copyright (C) 2008 - 2009 Intel Corporation.
 *
 * Author: Rob Bradford <rob@linux.intel.com>
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

#include "sw-item-view.h"
#include "sw-item-view-ginterface.h"

#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-core.h>

static void sw_item_view_iface_init (gpointer g_iface, gpointer iface_data);
G_DEFINE_TYPE_WITH_CODE (SwItemView, sw_item_view, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (SW_TYPE_ITEM_VIEW_IFACE,
                                                sw_item_view_iface_init));

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_ITEM_VIEW, SwItemViewPrivate))

typedef struct _SwItemViewPrivate SwItemViewPrivate;

struct _SwItemViewPrivate {
  SwService *service;
  gchar *object_path;
  SwSet *current_items_set;
};

enum
{
  PROP_0,
  PROP_SERVICE,
  PROP_OBJECT_PATH
};

static void
sw_item_view_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  SwItemViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_SERVICE:
      g_value_set_object (value, priv->service);
      break;
    case PROP_OBJECT_PATH:
      g_value_set_string (value, priv->object_path);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sw_item_view_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  SwItemViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_SERVICE:
      priv->service = g_value_dup_object (value);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sw_item_view_dispose (GObject *object)
{
  SwItemViewPrivate *priv = GET_PRIVATE (object);

  if (priv->service)
  {
    g_object_unref (priv->service);
    priv->service = NULL;
  }

  if (priv->current_items_set)
  {
    sw_set_unref (priv->current_items_set);
    priv->current_items_set = NULL;
  }

  G_OBJECT_CLASS (sw_item_view_parent_class)->dispose (object);
}

static void
sw_item_view_finalize (GObject *object)
{
  SwItemViewPrivate *priv = GET_PRIVATE (object);

  g_free (priv->object_path);

  G_OBJECT_CLASS (sw_item_view_parent_class)->finalize (object);
}

static gchar *
_make_object_path (SwItemView *item_view)
{
  SwItemViewPrivate *priv = GET_PRIVATE (item_view);
  gchar *path;
  static gint count = 0;

  path = g_strdup_printf ("/org/moblin/libsocialweb/%s/View%d",
                          sw_service_get_name (priv->service),
                          count);

  count++;

  return path;
}

static void
sw_item_view_constructed (GObject *object)
{
  SwItemView *item_view = SW_ITEM_VIEW (object);
  SwItemViewPrivate *priv = GET_PRIVATE (object);
  SwCore *core;

  core = sw_core_dup_singleton ();

  priv->object_path = _make_object_path (item_view);
  dbus_g_connection_register_g_object (sw_core_get_connection (core),
                                       priv->object_path,
                                       G_OBJECT (item_view));
  g_object_unref (core);

  if (G_OBJECT_CLASS (sw_item_view_parent_class)->constructed)
    G_OBJECT_CLASS (sw_item_view_parent_class)->constructed (object);
}

static void
sw_item_view_class_init (SwItemViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (SwItemViewPrivate));

  object_class->get_property = sw_item_view_get_property;
  object_class->set_property = sw_item_view_set_property;
  object_class->dispose = sw_item_view_dispose;
  object_class->finalize = sw_item_view_finalize;
  object_class->constructed = sw_item_view_constructed;

  pspec = g_param_spec_object ("service",
                               "service",
                               "The service this view is using",
                               SW_TYPE_SERVICE,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_SERVICE, pspec);

  pspec = g_param_spec_string ("object-path",
                               "Object path",
                               "The object path of this view",
                               NULL,
                               G_PARAM_READABLE);
  g_object_class_install_property (object_class, PROP_OBJECT_PATH, pspec);
}

static void
sw_item_view_init (SwItemView *self)
{
  SwItemViewPrivate *priv = GET_PRIVATE (self);

  priv->current_items_set = sw_item_set_new ();
}

static void
sw_item_view_start (SwItemViewIface       *iface,
                    DBusGMethodInvocation *context)
{
  SwItemView *item_view = SW_ITEM_VIEW (iface);

  if (SW_ITEM_VIEW_GET_CLASS (iface)->start)
    SW_ITEM_VIEW_GET_CLASS (iface)->start (item_view);

  sw_item_view_iface_return_from_start (context);
}

static void
sw_item_view_refresh (SwItemViewIface       *iface,
                      DBusGMethodInvocation *context)
{
  SwItemView *item_view = SW_ITEM_VIEW (iface);

  if (SW_ITEM_VIEW_GET_CLASS (iface)->refresh)
    SW_ITEM_VIEW_GET_CLASS (iface)->refresh (item_view);

  sw_item_view_iface_return_from_refresh (context);
}

static void
sw_item_view_stop (SwItemViewIface       *iface,
                   DBusGMethodInvocation *context)
{
  SwItemView *item_view = SW_ITEM_VIEW (iface);

  if (SW_ITEM_VIEW_GET_CLASS (iface)->stop)
    SW_ITEM_VIEW_GET_CLASS (iface)->stop (item_view);

  sw_item_view_iface_return_from_stop (context);
}

static void
sw_item_view_close (SwItemViewIface       *iface,
                    DBusGMethodInvocation *context)
{
  SwItemView *item_view = SW_ITEM_VIEW (iface);

  if (SW_ITEM_VIEW_GET_CLASS (iface)->close)
    SW_ITEM_VIEW_GET_CLASS (iface)->close (item_view);

  sw_item_view_iface_return_from_close (context);
}

static void
sw_item_view_iface_init (gpointer g_iface,
                         gpointer iface_data)
{
  SwItemViewIfaceClass *klass = (SwItemViewIfaceClass*)g_iface;
  sw_item_view_iface_implement_start (klass, sw_item_view_start);
  sw_item_view_iface_implement_refresh (klass, sw_item_view_refresh);
  sw_item_view_iface_implement_stop (klass, sw_item_view_stop);
  sw_item_view_iface_implement_close (klass, sw_item_view_close);
}

/**
 * sw_item_view_add_item
 * @item_view: A #SwItemView
 * @item: A #SwItem
 *
 * Add a single item in the #SwItemView. This will cause a signal to be
 * emitted across the bus.
 */
void
sw_item_view_add_item (SwItemView *item_view,
                       SwItem     *item)
{
  GValueArray *value_array;
  GPtrArray *ptr_array;

  ptr_array = g_ptr_array_new ();

  value_array = _sw_item_to_value_array (item);
  g_ptr_array_add (ptr_array, value_array);

  sw_item_view_iface_emit_items_added (item_view,
                                       ptr_array);
}

/**
 * sw_item_view_update_item
 * @item_view: A #SwItemView
 * @item: A #SwItem
 *
 * Update a single item in the #SwItemView. This will cause a signal to be
 * emitted across the bus.
 */
void
sw_item_view_update_item (SwItemView *item_view,
                          SwItem     *item)
{
  GValueArray *value_array;
  GPtrArray *ptr_array;

  ptr_array = g_ptr_array_new ();

  value_array = _sw_item_to_value_array (item);
  g_ptr_array_add (ptr_array, value_array);

  sw_item_view_iface_emit_items_changed (item_view,
                                         ptr_array);
}

/**
 * sw_item_view_remove_item
 * @item_view: A #SwItemView
 * @item: A #SwItem
 *
 * Remove a single item to the #SwItemView. This will cause a signal to be
 * emitted across the bus.
 */
void
sw_item_view_remove_item (SwItemView *item_view,
                          SwItem     *item)
{
  GValueArray *value_array;
  GPtrArray *ptr_array;

  ptr_array = g_ptr_array_new ();

  value_array = _sw_item_to_value_array (item);
  g_ptr_array_add (ptr_array, value_array);

  sw_item_view_iface_emit_items_removed (item_view,
                                         ptr_array);
}

/**
 * sw_item_view_add_items
 * @item_view: A #SwItemView
 * @items: A list of #SwItem objects
 *
 * Add the items supplied in the list from the #SwItemView. In many
 * cases what you actually want is sw_item_view_remove_from_set() or
 * sw_item_view_set_from_set(). This will cause signal emissions over the
 * bus.
 *
 * This is used in the implementation of sw_item_view_remove_from_set()
 */
void
sw_item_view_add_items (SwItemView *item_view,
                        GList      *items)
{
  GValueArray *value_array;
  GPtrArray *ptr_array;
  GList *l;

  ptr_array = g_ptr_array_new ();

  for (l = items; l; l = l->next)
  {
    value_array = _sw_item_to_value_array (SW_ITEM (l->data));
    g_ptr_array_add (ptr_array, value_array);
  }

  sw_item_view_iface_emit_items_added (item_view,
                                       ptr_array);
}

/**
 * sw_item_view_update_items
 * @item_view: A #SwItemView
 * @items: A list of #SwItem objects that need updating
 *
 * Update the items supplied in the list in the #SwItemView. This is
 * will cause signal emissions over the bus.
 */
void
sw_item_view_update_items (SwItemView *item_view,
                           GList      *items)
{
  GValueArray *value_array;
  GPtrArray *ptr_array;
  GList *l;

  ptr_array = g_ptr_array_new ();

  for (l = items; l; l = l->next)
  {
    value_array = _sw_item_to_value_array (SW_ITEM (l->data));
    g_ptr_array_add (ptr_array, value_array);
  }

  sw_item_view_iface_emit_items_changed (item_view,
                                         ptr_array);
}

/**
 * sw_item_view_remove_items
 * @item_view: A #SwItemView
 * @items: A list of #SwItem objects
 *
 * Remove the items supplied in the list from the #SwItemView. In many
 * cases what you actually want is sw_item_view_remove_from_set() or
 * sw_item_view_set_from_set(). This will cause signal emissions over the
 * bus.
 *
 * This is used in the implementation of sw_item_view_remove_from_set()
 */
void
sw_item_view_remove_items (SwItemView *item_view,
                           GList      *items)
{
  GValueArray *value_array;
  GPtrArray *ptr_array;
  GList *l;
  SwItem *item;

  ptr_array = g_ptr_array_new ();

  for (l = items; l; l = l->next)
  {
    item = SW_ITEM (l->data);

    value_array = g_value_array_new (2);

    value_array = g_value_array_append (value_array, NULL);
    g_value_init (g_value_array_get_nth (value_array, 0), G_TYPE_STRING);
    g_value_set_string (g_value_array_get_nth (value_array, 0),
                        sw_service_get_name (sw_item_get_service (item)));

    value_array = g_value_array_append (value_array, NULL);
    g_value_init (g_value_array_get_nth (value_array, 1), G_TYPE_STRING);
    g_value_set_string (g_value_array_get_nth (value_array, 1),
                        sw_item_get (item, "id"));

    g_ptr_array_add (ptr_array, value_array);
  }

  sw_item_view_iface_emit_items_removed (item_view,
                                         ptr_array);
}

/**
 * sw_item_view_get_object_path
 * @item_view: A #SwItemView
 *
 * Since #SwItemView is responsible for constructing the object path and
 * registering the object on the bus. This function is necessary for
 * #SwCore to be able to return the object path as the result of a
 * function to open a view.
 *
 * Returns: A string providing the object path.
 */
const gchar *
sw_item_view_get_object_path (SwItemView *item_view)
{
  SwItemViewPrivate *priv = GET_PRIVATE (item_view);

  return priv->object_path;
}

/**
 * sw_item_view_get_service
 * @item_view: A #SwItemView
 *
 * Returns: The #SwService that #SwItemView is for
 */
SwService *
sw_item_view_get_service (SwItemView *item_view)
{
  SwItemViewPrivate *priv = GET_PRIVATE (item_view);

  return priv->service;
}

/**
 * sw_item_view_add_from_set
 * @item_view: A #SwItemView
 * @set: A #SwSet
 *
 * Add the items that are in the supplied set to the view.
 *
 * This is used in the implementation of sw_item_view_set_from_set()
 */
void
sw_item_view_add_from_set (SwItemView *item_view,
                           SwSet      *set)
{
  SwItemViewPrivate *priv = GET_PRIVATE (item_view);
  GList *items;

  sw_set_add_from (priv->current_items_set, set);
  items = sw_set_as_list (set);

  sw_item_view_add_items (item_view, items);
  g_list_free (items);
}

/**
 * sw_item_view_remove_from_set
 * @item_view: A #SwItemView
 * @set: A #SwSet
 *
 * Remove the items that are in the supplied set from the view.
 *
 * This is used in the implementation of sw_item_view_set_from_set()
 */
void
sw_item_view_remove_from_set (SwItemView *item_view,
                              SwSet      *set)
{
  SwItemViewPrivate *priv = GET_PRIVATE (item_view);
  GList *items;

  sw_set_remove_from (priv->current_items_set, set);

  items = sw_set_as_list (set);

  sw_item_view_remove_items (item_view, items);
  g_list_free (items);
}

/**
 * sw_item_view_set_from_set
 * @item_view: A #SwItemView
 * @set: A #SwSet
 *
 * Updates what the view contains based on the given #SwSet. Removed
 * signals will be fired for any items that were in the view but that are not
 * present in the supplied set. Conversely any items that are new will cause
 * signals to be fired indicating their addition.
 *
 * This implemented by maintaining a set inside the #SwItemView
 */
void
sw_item_view_set_from_set (SwItemView *item_view,
                           SwSet      *set)
{
  SwItemViewPrivate *priv = GET_PRIVATE (item_view);
  SwSet *added_items, *removed_items;

  if (sw_set_is_empty (priv->current_items_set))
  {
    sw_item_view_add_from_set (item_view, set);
  } else {
    removed_items = sw_set_difference (priv->current_items_set, set);
    added_items = sw_set_difference (set, priv->current_items_set);

    sw_item_view_remove_from_set (item_view, removed_items);
    sw_item_view_add_from_set (item_view, added_items);

    sw_set_unref (removed_items);
    sw_set_unref (added_items);
  }
}