/*
 * libsocialweb - social data store
 * Copyright (C) 2010 Intel Corporation.
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

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <gnome-keyring.h>

#include <libsocialweb/sw-service.h>
#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-web.h>
#include <libsocialweb/sw-call-list.h>
#include <libsocialweb/sw-debug.h>
#include <libsocialweb/sw-client-monitor.h>
#include <libsocialweb/sw-online.h>
#include <libsocialweb-keyfob/sw-keyfob.h>
#include <libsocialweb-keystore/sw-keystore.h>

#include <rest/oauth-proxy.h>
#include <rest/rest-xml-parser.h>

#include <interfaces/sw-query-ginterface.h>
#include <interfaces/sw-collections-ginterface.h>
#include <interfaces/sw-video-upload-ginterface.h>

#include "vimeo.h"
#include "vimeo-item-view.h"

static void initable_iface_init (gpointer g_iface, gpointer iface_data);
static void query_iface_init (gpointer g_iface, gpointer iface_data);
static void collections_iface_init (gpointer g_iface, gpointer iface_data);
static void video_upload_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (SwServiceVimeo, sw_service_vimeo, SW_TYPE_SERVICE,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                initable_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_COLLECTIONS_IFACE,
                                                collections_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_VIDEO_UPLOAD_IFACE,
                                                video_upload_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_QUERY_IFACE,
                                                query_iface_init));
#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_SERVICE_VIMEO, SwServiceVimeoPrivate))

#define ALBUM_PREFIX "vimeo-"

typedef struct {
  gchar *title;
  gchar *description;
  gchar *real_id;
} VimeoAlbumPlaceholder;

struct _SwServiceVimeoPrivate {
  RestProxy *proxy;
  RestProxy *simple_proxy;
  RestProxy *upload_proxy;

  gboolean configured;
  gboolean inited;
  gchar *username;

  GHashTable *album_placeholders;
};

static const char *
get_name (SwService *service)
{
  return "vimeo";
}

enum {
  COLLECTION = 1,
  PHOTO = 2,
  VIDEO = 4
} typedef MediaType;

static const char **
get_static_caps (SwService *service)
{
  static const char * caps[] = {
    HAS_QUERY_IFACE,
    HAS_BANISHABLE_IFACE,
    HAS_COLLECTIONS_IFACE,

    NULL
  };

  return caps;
}

static const char **
get_dynamic_caps (SwService *service)
{
  SwServiceVimeoPrivate *priv = SW_SERVICE_VIMEO (service)->priv;

  static const char *configured_caps[] = {
    IS_CONFIGURED,
    NULL
  };

  static const char *authorized_caps[] = {
    IS_CONFIGURED,
    CREDENTIALS_VALID,
    NULL
  };

  static const char *no_caps[] = { NULL };

  if (priv->username != NULL)
    return authorized_caps;
  else if (priv->configured)
    return configured_caps;
  else
    return no_caps;
}

static VimeoAlbumPlaceholder *
album_placeholder_new (const gchar *title,
                       const gchar *description)
{
  VimeoAlbumPlaceholder *placeholder = g_slice_new0 (VimeoAlbumPlaceholder);
  placeholder->title = g_strdup (title);
  placeholder->description = g_strdup (description);

  return placeholder;
}

static void
album_placeholder_free (VimeoAlbumPlaceholder *placeholder)
{
  g_free (placeholder->title);
  g_free (placeholder->description);
  g_free (placeholder->real_id);
  g_slice_free (VimeoAlbumPlaceholder, placeholder);
}

static RestXmlNode *
node_from_call (RestProxyCall *call, GError **error)
{
  static RestXmlParser *parser = NULL;
  RestXmlNode *node;

  if (call == NULL)
    return NULL;

  if (parser == NULL)
    parser = rest_xml_parser_new ();

  if (!SOUP_STATUS_IS_SUCCESSFUL (rest_proxy_call_get_status_code (call))) {
    g_set_error (error, SW_SERVICE_ERROR, SW_SERVICE_ERROR_REMOTE_ERROR,
                 "HTTP error: %s (%d)",
                 rest_proxy_call_get_status_message (call),
                 rest_proxy_call_get_status_code (call));
    return NULL;
  }

  node = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));

  if (node == NULL || (g_str_equal (node->name, "rsp") &&
                       g_strcmp0 (rest_xml_node_get_attr (node, "stat"), "ok") != 0)) {
    RestXmlNode *err;
    err = rest_xml_node_find (node, "err");
    g_set_error (error, SW_SERVICE_ERROR, SW_SERVICE_ERROR_REMOTE_ERROR,
                 "remote Vimeo error: %s", err ?
                 rest_xml_node_get_attr (err, "msg") : "unknown");
    rest_xml_node_unref (node);
    return NULL;
  }

  return node;
}

static void
_check_access_token_cb (RestProxyCall *call,
                        const GError  *error,
                        GObject       *weak_object,
                        gpointer       user_data)
{
  RestXmlNode *root;
  GError *err = NULL;
  SwServiceVimeo *self = SW_SERVICE_VIMEO (weak_object);
  SwService *service = SW_SERVICE (self);
  SwServiceVimeoPrivate *priv = self->priv;

  root = node_from_call (call, &err);

  if (root == NULL) {
    g_assert (err != NULL);
    SW_DEBUG (VIMEO, "Invalid access token: %s", err->message);
    g_error_free (err);
  } else {
    RestXmlNode *username = rest_xml_node_find (root, "username");

    if (username != NULL) {
      const gchar *host = g_getenv ("SW_LOCAL_VIMEO");
      priv->username = g_strdup (username->content);
      rest_proxy_bind (priv->simple_proxy, host ? host : "vimeo.com",
                       priv->username);
    }

    rest_xml_node_unref (root);
  }

  sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));
}

static const gchar *
get_child_contents (RestXmlNode *root,
                   const gchar *element_name)
{
  RestXmlNode *node = rest_xml_node_find (root, element_name);

  g_return_val_if_fail (root != NULL, NULL);

  node = rest_xml_node_find (root, element_name);
  if (node == NULL)
    return NULL;

  return node->content;
}

static void
_simple_rest_async_run (RestProxy *proxy,
                        const gchar *function,
                        RestProxyCallAsyncCallback callback,
                        GObject                      *weak_object,
                        gpointer                      userdata,
                        GError                      **error,
                        ...)
{
  va_list params;
  RestProxyCall *call = rest_proxy_new_call (proxy);
  rest_proxy_call_set_function (call, function);

  va_start (params, error);
  rest_proxy_call_add_params_from_valist (call, params);
  va_end (params);

  rest_proxy_call_async (call,
                         callback,
                         weak_object,
                         userdata,
                         error);

  g_object_unref (call);

}

static const gchar *
get_child_attribute (RestXmlNode *root,
                    const gchar *element_name,
                    const gchar *attr_name)
{
  RestXmlNode *node = rest_xml_node_find (root, element_name);

  if (node == NULL)
    return NULL;

  return g_hash_table_lookup (node->attrs, attr_name);
}

static void
got_tokens_cb (RestProxy *proxy, gboolean got_tokens, gpointer user_data)
{
  SwServiceVimeo *self = (SwServiceVimeo *) user_data;
  SwService *service = SW_SERVICE (self);
  SwServiceVimeoPrivate *priv = self->priv;

  priv->configured = got_tokens;

  SW_DEBUG (VIMEO, "Got tokens: %s", got_tokens ? "yes" : "no");

  if (got_tokens) {
    RestProxyCall *call;
    OAuthProxy *upload_proxy = OAUTH_PROXY (priv->proxy);
    const gchar *host = g_getenv ("SW_LOCAL_VIMEO");

    /* For local testing with a mock Vimeo service. */
    if (host != NULL) {
      gchar *url = g_strdup_printf ("http://%s/", host);
      g_object_set (priv->proxy, "url-format", url, NULL);
      g_free (url);
    }

    oauth_proxy_set_token (OAUTH_PROXY (priv->upload_proxy),
                           oauth_proxy_get_token (upload_proxy));
    oauth_proxy_set_token_secret (OAUTH_PROXY (priv->upload_proxy),
                                  oauth_proxy_get_token_secret (upload_proxy));

    call = rest_proxy_new_call (priv->proxy);
    rest_proxy_call_set_function(call, "api/rest/v2");

    rest_proxy_call_add_param (call, "method", "vimeo.test.login");

    rest_proxy_call_async (call,
                           _check_access_token_cb,
                           G_OBJECT (self),
                           NULL,
                           NULL);

    g_object_unref (call);
  }

  sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));
}

static void
online_notify (gboolean online, gpointer user_data)
{
  SwServiceVimeo *self = (SwServiceVimeo *) user_data;
  SwService *service = SW_SERVICE (self);
  SwServiceVimeoPrivate *priv = self->priv;

  if (online) {
    sw_keyfob_oauth ((OAuthProxy *) priv->proxy, got_tokens_cb, self);
  } else {
    g_free (priv->username);
    priv->username = NULL;
    sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));
  }
}

/*
 * The credentials have been updated (or we're starting up), so fetch them from
 * the keyring.
 */
static void
refresh_credentials (SwServiceVimeo *self)
{
  SwService *service = SW_SERVICE (self);
  SwServiceVimeoPrivate *priv = self->priv;

  SW_DEBUG (VIMEO, "Credentials updated");

  priv->configured = FALSE;

  sw_service_emit_user_changed (service);

  online_notify (FALSE, service);
  online_notify (TRUE, service);
}

/*
 * SwService:credentials_updated vfunc implementation
 */
static void
credentials_updated (SwService *service)
{
  refresh_credentials (SW_SERVICE_VIMEO (service));
}

static void
sw_service_vimeo_dispose (GObject *object)
{
  SwServiceVimeoPrivate *priv = ((SwServiceVimeo*)object)->priv;

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  if (priv->simple_proxy) {
    g_object_unref (priv->simple_proxy);
    priv->simple_proxy = NULL;
  }

  if (priv->upload_proxy) {
    g_object_unref (priv->upload_proxy);
    priv->upload_proxy = NULL;
  }

  g_free (priv->username);

  g_hash_table_unref (priv->album_placeholders);

  G_OBJECT_CLASS (sw_service_vimeo_parent_class)->dispose (object);
}


/* Query interface */

static const gchar *valid_queries[] = { "feed", "own" };

static gboolean
_check_query_validity (const gchar *query)
{
  gint i = 0;

  for (i = 0; i < G_N_ELEMENTS (valid_queries); i++) {
    if (g_str_equal (query, valid_queries[i]))
      return TRUE;
  }

  return FALSE;
}

static void
_vimeo_query_open_view (SwQueryIface          *self,
                        const gchar           *query,
                        GHashTable            *params,
                        DBusGMethodInvocation *context)
{
  SwServiceVimeoPrivate *priv = GET_PRIVATE (self);
  SwItemView *item_view;
  const gchar *object_path;

  if (!_check_query_validity (query))
  {
    dbus_g_method_return_error (context,
                                g_error_new (SW_SERVICE_ERROR,
                                             SW_SERVICE_ERROR_INVALID_QUERY,
                                             "Query '%s' is invalid",
                                             query));
    return;
  }

  item_view = g_object_new (SW_TYPE_VIMEO_ITEM_VIEW,
                            "proxy", priv->simple_proxy,
                            "service", self,
                            "query", query,
                            "params", params,
                            NULL);

  /* Ensure the object gets disposed when the client goes away */
  sw_client_monitor_add (dbus_g_method_get_sender (context),
                         (GObject *)item_view);


  object_path = sw_item_view_get_object_path (item_view);
  sw_query_iface_return_from_open_view (context,
                                        object_path);
}

/* Collections Interface */

typedef struct {
  DBusGMethodInvocation *dbus_context;
  gchar *album_id;
  SwServiceVimeo *vimeo;
} VimeoAlbumOpCtx;

static VimeoAlbumOpCtx *
album_op_ctx_new (DBusGMethodInvocation *dbus_context,
                  const gchar *album_id,
                  SwServiceVimeo *vimeo)
{
  VimeoAlbumOpCtx *ctx = g_slice_new0 (VimeoAlbumOpCtx);
  ctx->dbus_context = dbus_context;
  ctx->album_id = g_strdup (album_id);
  ctx->vimeo = vimeo;

  return ctx;
}


static void
album_op_ctx_free (VimeoAlbumOpCtx *ctx)
{
  g_free (ctx->album_id);
  g_slice_free (VimeoAlbumOpCtx, ctx);
}

static GValueArray *
_extract_collection_details_from_xml (RestXmlNode *album)
{
  GValueArray *value_array;
  GHashTable *attribs = g_hash_table_new (g_str_hash, g_str_equal);
  GValue *value = NULL;
  gint64 count = 0;

  value_array = g_value_array_new (5);

  value_array = g_value_array_append (value_array, NULL);
  value = g_value_array_get_nth (value_array, 0);
  g_value_init (value, G_TYPE_STRING);
  g_value_take_string (value,
                       g_strdup_printf ("%s%s",
                                        ALBUM_PREFIX,
                                        get_child_contents (album, "id")));

  value_array = g_value_array_append (value_array, NULL);
  value = g_value_array_get_nth (value_array, 1);
  g_value_init (value, G_TYPE_STRING);
  g_value_set_static_string (value, get_child_contents (album, "title"));

  value_array = g_value_array_append (value_array, NULL);
  value = g_value_array_get_nth (value_array, 2);
  g_value_init (value, G_TYPE_STRING);
  g_value_set_static_string (value, "");

  value_array = g_value_array_append (value_array, NULL);
  value = g_value_array_get_nth (value_array, 3);
  g_value_init (value, G_TYPE_UINT);
  g_value_set_uint (value, VIDEO);

  count = g_ascii_strtoll (get_child_contents (album, "total_videos"), NULL,
                           10);

  value_array = g_value_array_append (value_array, NULL);
  value = g_value_array_get_nth (value_array, 4);
  g_value_init (value, G_TYPE_INT);
  g_value_set_int (value, count);

  g_hash_table_insert (attribs, "description",
                       (gpointer) get_child_contents (album, "description"));
  g_hash_table_insert (attribs, "url",
                       (gpointer)get_child_contents (album, "url"));
  g_hash_table_insert (attribs, "x-vimeo-created-on",
                       (gpointer) get_child_contents (album, "created_on"));
  g_hash_table_insert (attribs, "x-vimeo-last-modified",
                       (gpointer) get_child_contents (album, "last_modified"));
  g_hash_table_insert (attribs, "x-vimeo-thumbnail-small",
                       (gpointer) get_child_contents (album,
                                                      "thumbnail_small"));
  g_hash_table_insert (attribs, "x-vimeo-thumbnail-medium",
                       (gpointer) get_child_contents (album,
                                                      "thumbnail_medium"));
  g_hash_table_insert (attribs, "x-vimeo-thumbnail-large",
                       (gpointer) get_child_contents (album,
                                                      "thumbnail_large"));

  value_array = g_value_array_append (value_array, NULL);
  value = g_value_array_get_nth (value_array, 5);
  g_value_init (value, dbus_g_type_get_map ("GHashTable",
          G_TYPE_STRING,
          G_TYPE_STRING));
  g_value_take_boxed (value, attribs);

  return value_array;
}

static GValueArray *
_create_collection_details_from_placeholder (const gchar *placeholder_id,
                                             const VimeoAlbumPlaceholder *placeholder)
{
  GValueArray *value_array;
  GHashTable *attribs = g_hash_table_new (g_str_hash, g_str_equal);
  GValue *value = NULL;

  value_array = g_value_array_new (5);

  value_array = g_value_array_append (value_array, NULL);
  value = g_value_array_get_nth (value_array, 0);
  g_value_init (value, G_TYPE_STRING);
  g_value_set_static_string (value, placeholder_id);

  value_array = g_value_array_append (value_array, NULL);
  value = g_value_array_get_nth (value_array, 1);
  g_value_init (value, G_TYPE_STRING);
  g_value_set_static_string (value, placeholder->title);

  value_array = g_value_array_append (value_array, NULL);
  value = g_value_array_get_nth (value_array, 2);
  g_value_init (value, G_TYPE_STRING);
  g_value_set_static_string (value, "");

  value_array = g_value_array_append (value_array, NULL);
  value = g_value_array_get_nth (value_array, 3);
  g_value_init (value, G_TYPE_UINT);
  g_value_set_uint (value, VIDEO);

  value_array = g_value_array_append (value_array, NULL);
  value = g_value_array_get_nth (value_array, 4);
  g_value_init (value, G_TYPE_INT);
  g_value_set_int (value, 0);

  g_hash_table_insert (attribs, "description", placeholder->description);

  value_array = g_value_array_append (value_array, NULL);
  value = g_value_array_get_nth (value_array, 5);
  g_value_init (value, dbus_g_type_get_map ("GHashTable",
          G_TYPE_STRING,
          G_TYPE_STRING));
  g_value_take_boxed (value, attribs);

  return value_array;
}

static void
_list_albums_cb (RestProxyCall *call,
                 const GError  *error,
                 GObject       *weak_object,
                 gpointer       user_data)
{
  SwServiceVimeo *self = SW_SERVICE_VIMEO (weak_object);
  DBusGMethodInvocation *context = (DBusGMethodInvocation *) user_data;
  RestXmlNode *root = NULL;
  RestXmlNode *album;
  GPtrArray *rv = NULL;
  GError *err = NULL;
  GHashTableIter iter;
  gpointer key, value;

  if (error != NULL)
    err = g_error_new (SW_SERVICE_ERROR, SW_SERVICE_ERROR_REMOTE_ERROR,
                       "rest call failed: %s", error->message);

  if (err == NULL)
    root = node_from_call (call, &err);

  if (err != NULL) {
    dbus_g_method_return_error (context, err);
    g_error_free (err);
    goto OUT;
  }

  rv = g_ptr_array_new_with_free_func ((GDestroyNotify )g_value_array_free);

  album = rest_xml_node_find (root, "album");

  while (album != NULL) {
    g_ptr_array_add (rv, _extract_collection_details_from_xml (album));
    album = album->next;
  }

  g_hash_table_iter_init (&iter, self->priv->album_placeholders);

  while (g_hash_table_iter_next (&iter, &key, &value)) {
    const gchar *placeholder_id = key;
    const VimeoAlbumPlaceholder *placeholder = value;
    if (placeholder->real_id == NULL)
      g_ptr_array_add (rv, _create_collection_details_from_placeholder (placeholder_id,
                                                                        placeholder));
  }

  sw_collections_iface_return_from_get_list (context, rv);

 OUT:
  if (rv != NULL)
    g_ptr_array_free (rv, TRUE);

  if (root != NULL)
    rest_xml_node_unref (root);
}

static void
_get_album_details_cb (RestProxyCall *call,
                       const GError  *error,
                       GObject       *weak_object,
                       gpointer       user_data)
{
  VimeoAlbumOpCtx *ctx = (VimeoAlbumOpCtx *) user_data;
  RestXmlNode *root = NULL;
  RestXmlNode *album;
  GValueArray *collection_details = NULL;
  GError *err = NULL;

  if (error != NULL)
    err = g_error_new (SW_SERVICE_ERROR, SW_SERVICE_ERROR_REMOTE_ERROR,
                       "rest call failed: %s", error->message);

  if (err == NULL)
    root = node_from_call (call, &err);

  if (err != NULL) {
    dbus_g_method_return_error (ctx->dbus_context, err);
    g_error_free (err);
    goto OUT;
  }

  album = rest_xml_node_find (root, "album");

  while (album != NULL && collection_details == NULL) {
    const gchar *album_id = rest_xml_node_find (album, "id")->content;
    if (g_strcmp0 (album_id, ctx->album_id + strlen(ALBUM_PREFIX)) == 0)
      collection_details = _extract_collection_details_from_xml (album);
    album = album->next;
  }

  if (collection_details == NULL) {
    err = g_error_new (SW_SERVICE_ERROR, SW_SERVICE_ERROR_REMOTE_ERROR,
                       "Album id '%s' not found", ctx->album_id);
    dbus_g_method_return_error (ctx->dbus_context, err);
    g_error_free (err);
    goto OUT;
  }

  sw_collections_iface_return_from_get_details (ctx->dbus_context, collection_details);

 OUT:
  album_op_ctx_free (ctx);

  if (collection_details != NULL)
    g_value_array_free (collection_details);

  if (root != NULL)
    rest_xml_node_unref (root);
}

static void
_vimeo_collections_get_list (SwCollectionsIface *self,
                             DBusGMethodInvocation *context)
{
  SwServiceVimeo *vimeo = SW_SERVICE_VIMEO (self);
  SwServiceVimeoPrivate *priv = vimeo->priv;
  RestProxyCall *call;

  g_return_if_fail (priv->simple_proxy != NULL);

  call = rest_proxy_new_call (priv->simple_proxy);
  rest_proxy_call_set_function (call, "albums.xml");

  rest_proxy_call_async (call,
                         (RestProxyCallAsyncCallback) _list_albums_cb,
                         (GObject *) vimeo,
                         context,
                         NULL);

  g_object_unref (call);
}

static void
_vimeo_collections_get_details (SwCollectionsIface *self,
                                const gchar *collection_id,
                                DBusGMethodInvocation *context)
{
  SwServiceVimeo *vimeo = SW_SERVICE_VIMEO (self);
  SwServiceVimeoPrivate *priv = vimeo->priv;
  RestProxyCall *call;
  VimeoAlbumOpCtx *ctx = NULL;
  VimeoAlbumPlaceholder *placeholder;

  g_return_if_fail (priv->simple_proxy != NULL);

  placeholder = g_hash_table_lookup (priv->album_placeholders, collection_id);

  if (placeholder != NULL) {
    if (placeholder->real_id == NULL) {
      GValueArray *rv = _create_collection_details_from_placeholder (collection_id,
                                                                     placeholder);
      sw_collections_iface_return_from_get_details (context, rv);
      g_value_array_free (rv);

      return;
    } else {
      ctx = album_op_ctx_new (context, placeholder->real_id, vimeo);
    }
  } else {
    ctx = album_op_ctx_new (context, collection_id, vimeo);
  }

  call = rest_proxy_new_call (priv->simple_proxy);
  rest_proxy_call_set_function (call, "albums.xml");

  rest_proxy_call_async (call,
                         (RestProxyCallAsyncCallback) _get_album_details_cb,
                         (GObject *) vimeo,
                         ctx,
                         NULL);

  g_object_unref (call);
}

static void
_vimeo_collections_create (SwCollectionsIface *self,
                                 const gchar *collection_name,
                                 MediaType supported_types,
                                 const gchar *collection_parent,
                                 GHashTable *extra_parameters,
                                 DBusGMethodInvocation *context)
{
  SwServiceVimeo *vimeo = SW_SERVICE_VIMEO (self);
  SwServiceVimeoPrivate *priv = vimeo->priv;
  gchar *placeholder_id = g_strdup_printf ("%splaceholder-%u", ALBUM_PREFIX,
                                           g_random_int ());
  VimeoAlbumPlaceholder *placeholder =
    album_placeholder_new (collection_name,
                           g_hash_table_lookup (extra_parameters, "description"));

  g_hash_table_insert (priv->album_placeholders, placeholder_id, placeholder);

  sw_collections_iface_return_from_create (context, placeholder_id);
}

static void
collections_iface_init (gpointer g_iface,
                        gpointer iface_data)
{
  SwCollectionsIfaceClass *klass = (SwCollectionsIfaceClass *) g_iface;

  sw_collections_iface_implement_get_list (klass,
                                           _vimeo_collections_get_list);

  sw_collections_iface_implement_get_details (klass,
                                              _vimeo_collections_get_details);
  sw_collections_iface_implement_create (klass,
                                         _vimeo_collections_create);
}

/* Video upload interface */

typedef struct {
  gint opid;
  GMappedFile *mapped_file;
  gchar *filename;
  gchar *ticket_id;
  gchar *video_id;
  gchar *title;
  gchar *description;
  gchar *collection_id;
} VimeoUploadCtx;

static void _upload_get_quota_cb (RestProxyCall *call,
                                  const GError  *error,
                                  GObject       *weak_object,
                                  gpointer       user_data);
static void _upload_get_ticket_cb (RestProxyCall *call,
                                   const GError  *error,
                                   GObject       *weak_object,
                                   gpointer       user_data);
static void _upload_video (SwServiceVimeo *self,
                           const gchar    *upload_endpoint,
                           VimeoUploadCtx *ctx);
static void _upload_file_cb (RestProxyCall *call,
                             const GError  *error,
                             GObject       *weak_object,
                             gpointer       user_data);
static void _upload_verify_cb (RestProxyCall *call,
                               const GError  *error,
                               GObject       *weak_object,
                               gpointer       user_data);
static void _upload_complete_cb (RestProxyCall *call,
                                 const GError  *error,
                                 GObject       *weak_object,
                                 gpointer       user_data);
static void _set_title_cb (RestProxyCall *call,
                           const GError  *error,
                           GObject       *weak_object,
                           gpointer       user_data);
static void _set_description_cb (RestProxyCall *call,
                                 const GError  *error,
                                 GObject       *weak_object,
                                 gpointer       user_data);
static void _add_video_to_album (SwServiceVimeo *self, VimeoUploadCtx *ctx);
static void _add_to_album_cb (RestProxyCall *call,
                              const GError  *error,
                              GObject       *weak_object,
                              gpointer       user_data);
static void _create_album_cb (RestProxyCall *call,
                              const GError  *error,
                              GObject       *weak_object,
                              gpointer       user_data);
static void _upload_completed (SwServiceVimeo *self, VimeoUploadCtx *ctx);

#define UPLOAD_ERROR(format...)                                              \
  {                                                                        \
    gchar *_message = g_strdup_printf (format);                     \
    sw_video_upload_iface_emit_video_upload_progress (self, ctx->opid, -1, \
                                                      _message);           \
    SW_DEBUG (VIMEO, "Error: %s", _message); \
    g_free (_message);                                                     \
  }

static void
vimeo_upload_context_free (VimeoUploadCtx *ctx) {
  g_free (ctx->filename);
  g_free (ctx->ticket_id);
  g_free (ctx->video_id);
  g_free (ctx->title);
  g_free (ctx->description);
  g_free (ctx->collection_id);
  g_mapped_file_unref (ctx->mapped_file);

  g_slice_free (VimeoUploadCtx, ctx);
}

VimeoUploadCtx *
vimeo_upload_context_new (const gchar *filename,
                          const gchar *title,
                          const gchar *description,
                          const gchar *collection_id,
                          GError **error)
{
  GMappedFile *mapped_file = g_mapped_file_new (filename, FALSE, error);

  if (*error != NULL) {
    return NULL;
  } else {
    VimeoUploadCtx *ctx = g_slice_new0 (VimeoUploadCtx);

    ctx->mapped_file = mapped_file;
    ctx->opid = sw_next_opid ();
    ctx->filename = g_strdup (filename);
    ctx->title = g_strdup (title);
    ctx->description = g_strdup (description);
    ctx->collection_id = g_strdup (collection_id);

    return ctx;
  }
}

static void
_vimeo_upload_video (SwVideoUploadIface    *self,
                     const gchar           *filename,
                     GHashTable            *fields,
                     DBusGMethodInvocation *context)
{
  SwServiceVimeoPrivate *priv = SW_SERVICE_VIMEO (self)->priv;
  GError *error = NULL;
  VimeoUploadCtx *ctx;

  ctx = vimeo_upload_context_new (filename,
                                  g_hash_table_lookup (fields, "title"),
                                  g_hash_table_lookup (fields, "description"),
                                  g_hash_table_lookup (fields, "collection"),
                                  &error);
  if (error != NULL) {
    dbus_g_method_return_error (context, error);
    g_error_free (error);
    return;
  }

  _simple_rest_async_run (priv->proxy, "api/rest/v2",
                          _upload_get_quota_cb, G_OBJECT (self), ctx, NULL,
                          "method", "vimeo.videos.upload.getQuota",
                          NULL);

  sw_video_upload_iface_return_from_upload_video (context, ctx->opid);
}

static void
_upload_get_quota_cb (RestProxyCall *call,
                      const GError  *error,
                      GObject       *weak_object,
                      gpointer       user_data)
{
  VimeoUploadCtx *ctx = (VimeoUploadCtx *) user_data;
  SwServiceVimeo *self = SW_SERVICE_VIMEO (weak_object);
  SwServiceVimeoPrivate *priv = self->priv;
  GError *err = NULL;
  RestXmlNode *root = NULL;
  const gchar *free_space;
  gint64 free_quota;

  if (error != NULL) {
    UPLOAD_ERROR ("%s", error->message);
    goto OUT;
  }

  root = node_from_call (call, &err);

  if (err != NULL) {
    UPLOAD_ERROR ("%s", err->message);
    g_error_free (err);
    goto OUT;
  }

  free_space = get_child_attribute (root, "upload_space", "free");

  if (free_space == NULL) {
    UPLOAD_ERROR ("Malformed respose, can't get free space: \n%s",
                  rest_proxy_call_get_payload (call));
    goto OUT;
  }

  free_quota = g_ascii_strtoll (free_space, NULL, 10);

  if (free_quota < g_mapped_file_get_length (ctx->mapped_file)) {
    UPLOAD_ERROR ("The file is larger than the user's remaining quota, "
                  "need %li, but only have %li left in quota",
                  g_mapped_file_get_length (ctx->mapped_file),
                  free_quota);
    goto OUT;
  }

  _simple_rest_async_run (priv->proxy, "api/rest/v2",
                          _upload_get_ticket_cb, G_OBJECT (self), ctx, NULL,
                          "method", "vimeo.videos.upload.getTicket",
                          NULL);

 OUT:
  if (root != NULL)
    rest_xml_node_unref (root);
}

static void
_upload_get_ticket_cb (RestProxyCall *call,
                       const GError  *error,
                       GObject       *weak_object,
                       gpointer       user_data)
{
  VimeoUploadCtx *ctx = (VimeoUploadCtx *) user_data;
  SwServiceVimeo *self = SW_SERVICE_VIMEO (weak_object);
  RestXmlNode *root;
  const gchar *upload_endpoint;
  GError *err = NULL;

  root = node_from_call (call, &err);

  if (err != NULL) {
    UPLOAD_ERROR ("%s", err->message);
    g_error_free (err);
    goto OUT;
  }

  ctx->ticket_id = g_strdup (get_child_attribute (root, "ticket", "id"));

  if (ctx->ticket_id == NULL) {
    UPLOAD_ERROR ("Malformed respose, expected ticket id: \n%s",
                  rest_proxy_call_get_payload (call));
    goto OUT;
  }

  upload_endpoint = get_child_attribute (root, "ticket", "endpoint");

  if (upload_endpoint == NULL) {
    UPLOAD_ERROR ("Malformed respose, expected upload_endpoint: \n%s",
                  rest_proxy_call_get_payload (call));
    goto OUT;
  }

  _upload_video (self, upload_endpoint, ctx);

 OUT:
  if (root != NULL)
    rest_xml_node_unref (root);

}

static void
_upload_video (SwServiceVimeo *self,
               const gchar    *upload_endpoint,
               VimeoUploadCtx *ctx)
{
  SwServiceVimeoPrivate *priv = self->priv;
  gchar *basename;
  gchar *content_type;
  RestParam *param;
  RestProxyCall *upload_call;

  rest_proxy_bind (priv->upload_proxy, upload_endpoint);

  upload_call = rest_proxy_new_call (priv->upload_proxy);

  rest_proxy_call_set_method (upload_call, "POST");

  rest_proxy_call_add_param (upload_call, "chunk_id", "0");
  rest_proxy_call_add_param (upload_call, "ticket_id", ctx->ticket_id);

  basename = g_path_get_basename (ctx->filename);
  content_type = g_content_type_guess (
      ctx->filename,
      (const guchar*) g_mapped_file_get_contents (ctx->mapped_file),
      g_mapped_file_get_length (ctx->mapped_file),
      NULL);

  param =
    rest_param_new_with_owner ("file_data",
                               g_mapped_file_get_contents (ctx->mapped_file),
                               g_mapped_file_get_length (ctx->mapped_file),
                               content_type,
                               basename,
                               g_mapped_file_ref (ctx->mapped_file),
                               (GDestroyNotify) g_mapped_file_unref);

  rest_proxy_call_add_param_full (upload_call, param);

  rest_proxy_call_async (upload_call,
                         _upload_file_cb,
                         G_OBJECT (self),
                         ctx,
                         NULL);

  g_object_unref (upload_call);
  g_free (basename);
  g_free (content_type);
}


static void
_upload_file_cb (RestProxyCall *call,
                 const GError  *error,
                 GObject       *weak_object,
                 gpointer       user_data)
{
  VimeoUploadCtx *ctx = (VimeoUploadCtx *) user_data;
  SwServiceVimeo *self = SW_SERVICE_VIMEO (weak_object);
  SwServiceVimeoPrivate *priv = self->priv;

  if (error != NULL) {
    UPLOAD_ERROR("%s", error->message);
  } else {
    _simple_rest_async_run (priv->proxy, "api/rest/v2", _upload_verify_cb,
                            G_OBJECT (self), ctx, NULL,
                            "method", "vimeo.videos.upload.verifyChunks",
                            "ticket_id", ctx->ticket_id,
                            NULL);
  }
}

static void
_upload_verify_cb (RestProxyCall *call,
                   const GError  *error,
                   GObject       *weak_object,
                   gpointer       user_data)
{
  VimeoUploadCtx *ctx = (VimeoUploadCtx *) user_data;
  SwServiceVimeo *self = SW_SERVICE_VIMEO (weak_object);
  SwServiceVimeoPrivate *priv = self->priv;
  RestXmlNode *root;
  const gchar *chunk_size;
  GError *err = NULL;

  root = node_from_call (call, &err);

  if (err != NULL) {
    UPLOAD_ERROR("%s", err->message);
    g_error_free (err);
    goto OUT;
  }

  chunk_size = get_child_attribute (root, "chunk", "size");

  if (chunk_size == NULL) {
    UPLOAD_ERROR ("Malformed respose, expected chunk_size: \n%s",
                  rest_proxy_call_get_payload (call));
    goto OUT;
  }

  if (g_ascii_strtoll (chunk_size, NULL, 10) !=
      g_mapped_file_get_length (ctx->mapped_file)) {
    UPLOAD_ERROR ("Expected upload size is %li, but got %li instead",
                  g_mapped_file_get_length (ctx->mapped_file),
                  g_ascii_strtoll (chunk_size, NULL, 10));
    goto OUT;
  }

  _simple_rest_async_run (priv->proxy, "api/rest/v2",
                          _upload_complete_cb, G_OBJECT (self), ctx, NULL,
                          "method", "vimeo.videos.upload.complete",
                          "ticket_id", ctx->ticket_id,
                          NULL);

 OUT:
  if (root != NULL)
    rest_xml_node_unref (root);
}

static void
_upload_complete_cb (RestProxyCall *call,
                     const GError  *error,
                     GObject       *weak_object,
                     gpointer       user_data)
{
  VimeoUploadCtx *ctx = (VimeoUploadCtx *) user_data;
  SwServiceVimeo *self = SW_SERVICE_VIMEO (weak_object);
  SwServiceVimeoPrivate *priv = self->priv;
  GError *err = NULL;
  RestXmlNode *root = node_from_call (call, &err);

  if (err != NULL) {
    UPLOAD_ERROR ("%s", err->message);
    g_error_free (err);
    goto OUT;
  }

  ctx->video_id = g_strdup (get_child_attribute (root, "ticket", "video_id"));

  SW_DEBUG (VIMEO, "Complete: %s", ctx->video_id);

  if (ctx->title != NULL)
    _simple_rest_async_run (priv->proxy, "api/rest/v2",
                            _set_title_cb, G_OBJECT (self), ctx, NULL,
                            "method", "vimeo.videos.setTitle",
                            "title", ctx->title,
                            "video_id", ctx->video_id,
                            NULL);
  else if (ctx->description != NULL)
    _simple_rest_async_run (priv->proxy, "api/rest/v2",
                            _set_description_cb, G_OBJECT (self), ctx, NULL,
                            "method", "vimeo.videos.setDescription",
                            "description", ctx->description,
                            "video_id", ctx->video_id,
                            NULL);
  else if (ctx->collection_id != NULL)
    _add_video_to_album (self, ctx);
  else
    _upload_completed (self, ctx);

 OUT:
  if (root != NULL)
    rest_xml_node_unref (root);
}

static void
_set_title_cb (RestProxyCall *call,
               const GError  *error,
               GObject       *weak_object,
               gpointer       user_data)
{
  VimeoUploadCtx *ctx = (VimeoUploadCtx *) user_data;
  SwServiceVimeo *self = SW_SERVICE_VIMEO (weak_object);
  SwServiceVimeoPrivate *priv = self->priv;
  GError *err = NULL;
  RestXmlNode *root = node_from_call (call, &err);

  if (err != NULL) {
    UPLOAD_ERROR ("%s", err->message);
    g_error_free (err);
    goto OUT;
  }

  SW_DEBUG (VIMEO, "Success setting title");

  if (ctx->description != NULL)
    _simple_rest_async_run (priv->proxy, "api/rest/v2",
                            _set_description_cb, G_OBJECT (self), ctx, NULL,
                            "method", "vimeo.videos.setDescription",
                            "description", ctx->description,
                            "video_id", ctx->video_id,
                            NULL);
  else if (ctx->collection_id != NULL)
    _add_video_to_album (self, ctx);
  else
    _upload_completed (self, ctx);

 OUT:
  if (root != NULL)
    rest_xml_node_unref (root);
}

static void
_set_description_cb (RestProxyCall *call,
                     const GError  *error,
                     GObject       *weak_object,
                     gpointer       user_data)
{
  VimeoUploadCtx *ctx = (VimeoUploadCtx *) user_data;
  SwServiceVimeo *self = SW_SERVICE_VIMEO (weak_object);
  GError *err = NULL;
  RestXmlNode *root = node_from_call (call, &err);

  if (err != NULL) {
    UPLOAD_ERROR ("%s", err->message);
    g_error_free (err);
    goto OUT;
  }

  SW_DEBUG (VIMEO, "Success setting description");

  if (ctx->collection_id != NULL)
    _add_video_to_album (self, ctx);
  else
    _upload_completed (self, ctx);

 OUT:
  if (root != NULL)
    rest_xml_node_unref (root);
}

static void
_add_video_to_album (SwServiceVimeo *self, VimeoUploadCtx *ctx)
{
  SwServiceVimeoPrivate *priv = self->priv;
  const gchar *real_collection_id = NULL;
  const VimeoAlbumPlaceholder *placeholder;

  g_return_if_fail (ctx->collection_id != NULL);

  placeholder = g_hash_table_lookup (priv->album_placeholders,
                                     ctx->collection_id);

  if (placeholder != NULL)
    real_collection_id = placeholder->real_id;
  else
    real_collection_id = ctx->collection_id;

  if (real_collection_id != NULL) {
    real_collection_id = real_collection_id + strlen (ALBUM_PREFIX);
    _simple_rest_async_run (priv->proxy, "api/rest/v2",
                            _add_to_album_cb, G_OBJECT (self), ctx, NULL,
                            "method", "vimeo.albums.addVideo",
                            "album_id", real_collection_id,
                            "video_id", ctx->video_id,
                            NULL);
  } else {
    RestProxyCall *call;
    g_assert (placeholder != NULL);

    call = rest_proxy_new_call (priv->proxy);

    rest_proxy_call_set_function (call, "api/rest/v2");

    rest_proxy_call_add_params (call,
                                "method", "vimeo.albums.create",
                                "title", placeholder->title,
                                "video_id", ctx->video_id,
                                NULL);

    if (placeholder->description != NULL)
      rest_proxy_call_add_param (call, "description",
                                 placeholder->description);

    rest_proxy_call_async (call, _create_album_cb, G_OBJECT (self), ctx, NULL);

    g_object_unref (call);
  }
}

static void
_add_to_album_cb (RestProxyCall *call,
                  const GError  *error,
                  GObject       *weak_object,
                  gpointer       user_data)
{
  VimeoUploadCtx *ctx = (VimeoUploadCtx *) user_data;
  SwServiceVimeo *self = SW_SERVICE_VIMEO (weak_object);
  GError *err = NULL;
  RestXmlNode *root = node_from_call (call, &err);

  if (err != NULL) {
    UPLOAD_ERROR ("%s", err->message);
    g_error_free (err);
  } else {
    _upload_completed (self, ctx);
  }

  if (root != NULL)
    rest_xml_node_unref (root);
}

static void
_create_album_cb (RestProxyCall *call,
                  const GError  *error,
                  GObject       *weak_object,
                  gpointer       user_data)
{
  VimeoUploadCtx *ctx = (VimeoUploadCtx *) user_data;
  SwServiceVimeo *self = SW_SERVICE_VIMEO (weak_object);
  SwServiceVimeoPrivate *priv = self->priv;
  GError *err = NULL;
  RestXmlNode *root = node_from_call (call, &err);
  VimeoAlbumPlaceholder *placeholder;
  const gchar *album_id;

  if (err != NULL) {
    UPLOAD_ERROR ("%s", err->message);
    g_error_free (err);
    goto OUT;
  }

  placeholder = g_hash_table_lookup (priv->album_placeholders,
                                     ctx->collection_id);

  album_id = get_child_attribute (root, "album", "id");

  SW_DEBUG (VIMEO, "Created album: %s", album_id);

  placeholder->real_id = g_strdup_printf ("%s%s", ALBUM_PREFIX, album_id);

  _upload_completed (self, ctx);

 OUT:
  if (root != NULL)
    rest_xml_node_unref (root);
}

static void
_upload_completed (SwServiceVimeo *self, VimeoUploadCtx *ctx)
{
  sw_video_upload_iface_emit_video_upload_progress (self, ctx->opid, 100, "");
  vimeo_upload_context_free (ctx);
}


/* end of upload call chain */

static void
video_upload_iface_init (gpointer g_iface, gpointer iface_data)
{
  SwVideoUploadIfaceClass *klass = (SwVideoUploadIfaceClass *) g_iface;

  sw_video_upload_iface_implement_upload_video (klass,
                                                _vimeo_upload_video);
}

static void
query_iface_init (gpointer g_iface,
                  gpointer iface_data)
{
  SwQueryIfaceClass *klass = (SwQueryIfaceClass*)g_iface;

  sw_query_iface_implement_open_view (klass, _vimeo_query_open_view);
}

static void
sw_service_vimeo_class_init (SwServiceVimeoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwServiceClass *service_class = SW_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SwServiceVimeoPrivate));

  object_class->dispose = sw_service_vimeo_dispose;

  service_class->get_name = get_name;
  service_class->get_static_caps = get_static_caps;
  service_class->get_dynamic_caps = get_dynamic_caps;
  service_class->credentials_updated = credentials_updated;
}

static gboolean
sw_service_vimeo_initable (GInitable     *initable,
                             GCancellable  *cancellable,
                             GError       **error)
{
  SwServiceVimeo *self = SW_SERVICE_VIMEO (initable);
  SwServiceVimeoPrivate *priv = self->priv;
  const gchar *api_key;
  const gchar *api_secret;

  if (priv->inited)
    return TRUE;

  sw_keystore_get_key_secret ("vimeo", &api_key, &api_secret);

  if (api_key == NULL || api_secret == NULL) {
    g_set_error_literal (error,
                         SW_SERVICE_ERROR,
                         SW_SERVICE_ERROR_NO_KEYS,
                         "No API or secret key configured");
    return FALSE;
  }

  priv->inited = TRUE;

  priv->proxy = g_object_new (OAUTH_TYPE_PROXY,
                              "consumer-key", api_key,
                              "consumer-secret", api_secret,
                              "url-format", "http://vimeo.com/",
                              "disable-cookies", TRUE,
                              NULL);
  priv->upload_proxy = oauth_proxy_new (api_key, api_secret, "%s", TRUE);
  priv->simple_proxy = rest_proxy_new ("http://%s/api/v2/%s/", TRUE);

  priv->album_placeholders =
    g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                           (GDestroyNotify) album_placeholder_free);

  sw_online_add_notify (online_notify, self);
  refresh_credentials (self);

  return TRUE;
}

static void
initable_iface_init (gpointer g_iface, gpointer iface_data)
{
  GInitableIface *klass = (GInitableIface *)g_iface;

  klass->init = sw_service_vimeo_initable;
}

static void
sw_service_vimeo_init (SwServiceVimeo *self)
{
  self->priv = GET_PRIVATE (self);
}
