/*
 * libsocialweb - social data store
 * Copyright (C) 2008 - 2009 Intel Corporation.
 * Copyright (C) 2011 Collabora Ltd.
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
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <libsocialweb/sw-debug.h>
#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-set.h>
#include <libsocialweb/sw-online.h>
#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-web.h>
#include <libsocialweb-keystore/sw-keystore.h>
#include <libsocialweb-keyfob/sw-keyfob.h>
#include <libsocialweb/sw-client-monitor.h>

#include <rest/oauth-proxy.h>

#include <interfaces/sw-contacts-query-ginterface.h>
#include <interfaces/sw-query-ginterface.h>
#include <interfaces/sw-photo-upload-ginterface.h>
#include <interfaces/sw-video-upload-ginterface.h>

#include "flickr-item-view.h"
#include "flickr-contact-view.h"
#include "flickr.h"
#include "flickr-auth-manager.h"

#define FLICKR_REST_API_URL "http://api.flickr.com/services/rest/"
#define FLICKR_UPLOAD_URL "http://api.flickr.com/services/upload/"

static void initable_iface_init (gpointer g_iface, gpointer iface_data);
static void contacts_query_iface_init (gpointer g_iface, gpointer iface_data);
static void query_iface_init (gpointer g_iface, gpointer iface_data);
static void photo_upload_iface_init (gpointer g_iface, gpointer iface_data);
static void video_upload_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (SwServiceFlickr, sw_service_flickr, SW_TYPE_SERVICE,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                initable_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_CONTACTS_QUERY_IFACE,
                                                contacts_query_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_QUERY_IFACE,
                                                query_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_PHOTO_UPLOAD_IFACE,
                                                photo_upload_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_VIDEO_UPLOAD_IFACE,
                                                video_upload_iface_init));

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_SERVICE_FLICKR, SwServiceFlickrPrivate))

struct _SwServiceFlickrPrivate {
  RestProxy *proxy;
  RestProxy *upload_proxy;
  FlickrAuthManager *auth_manager; /* Helper class to handle authorization process */
  gboolean inited; /* For GInitable */
  gboolean configured; /* Set if we have user tokens */
  gboolean authorised; /* Set if the tokens are valid */
};

static const ParameterNameMapping upload_params[] = {
  { "title", "title" },
  { "description", "description" },
  { "x-flickr-is-public", "is_public" },
  { "x-flickr-is-friend", "is_friend" },
  { "x-flickr-is-family", "is_family" },
  { "x-flickr-safety-level", "safety_level" },
  { "x-flickr-content-type", "content_type" },
  { "x-flickr-hidden", "hidden" },
  { "x-flickr-tags", "tags" },
  { NULL, NULL }
};

static const char **
get_static_caps (SwService *service)
{
  static const char * caps[] = {
    CAN_VERIFY_CREDENTIALS,
    HAS_PHOTO_UPLOAD_IFACE,
    HAS_VIDEO_UPLOAD_IFACE,
    HAS_BANISHABLE_IFACE,
    HAS_QUERY_IFACE,
    HAS_CONTACTS_QUERY_IFACE,

    NULL
  };

  return caps;
}

static const char **
get_dynamic_caps (SwService *service)
{
  SwServiceFlickrPrivate *priv = GET_PRIVATE (service);

  static const char *unconfigured_caps[] = {
    NULL,
  };

  static const char *authorised_caps[] = {
    IS_CONFIGURED,
    CREDENTIALS_VALID,
    NULL
  };

  static const char *unauthorised_caps[] = {
    IS_CONFIGURED,
    CREDENTIALS_INVALID,
    NULL
  };

  if (priv->configured) {
    if (priv->authorised) {
      return authorised_caps;
    } else {
      return unauthorised_caps;
    }
  } else {
    return unconfigured_caps;
  }
}

static void
on_proxy_checked (gboolean token_valid,
                  const GError *validation_error,
                  OAuthProxy *proxy,
                  gpointer user_data)
{

  SwServiceFlickr *self = SW_SERVICE_FLICKR (user_data);
  
  self->priv->authorised = token_valid;

  if (token_valid) {
    SW_DEBUG (FLICKR, "checkToken: authorised");
  } else {
    SW_DEBUG (FLICKR, "checkToken: invalid token");
  }

  sw_service_emit_capabilities_changed (SW_SERVICE (self),
                                        get_dynamic_caps (SW_SERVICE (self)));
  
}

static void
got_tokens_cb (RestProxy *proxy,
               gboolean   got_tokens,
               gpointer   user_data)
{
  SwService *service = SW_SERVICE (user_data);
  SwServiceFlickrPrivate *priv = GET_PRIVATE (service);
  
  SW_DEBUG (FLICKR, "Got tokens: %s", got_tokens ? "yes" : "no");

  priv->configured = got_tokens;
  sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));

  if (got_tokens && sw_is_online ()) {
    
    flickr_auth_mgr_check_proxy_async (priv->auth_manager,
                                       OAUTH_PROXY (proxy),
                                       on_proxy_checked,
                                       user_data,
                                       NULL /* TODO: error handling */
                                       );

  }

  /* Drop reference we took for callback */
  g_object_unref (service);
}

static void
credentials_updated (SwService *service)
{
  SwServiceFlickrPrivate *priv = GET_PRIVATE (service);

  priv->configured = FALSE;
  priv->authorised = FALSE;

  sw_keyfob_oauth (OAUTH_PROXY (priv->proxy),
                   got_tokens_cb,
                   g_object_ref (service)); /* ref gets dropped in cb */

  sw_service_emit_user_changed (service);
  sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));
}

static void
online_notify (gboolean online, gpointer user_data)
{
  SwService *service = SW_SERVICE (user_data);
  SwServiceFlickrPrivate *priv = GET_PRIVATE (service);

  SW_DEBUG (FLICKR, "Online: %s", online ? "yes" : "no");

  if (online) {
    got_tokens_cb (priv->proxy, TRUE, g_object_ref (service));
  } else {
    priv->authorised = FALSE;

    sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));
  }
}

static const char *
sw_service_flickr_get_name (SwService *service)
{
  return "flickr";
}

static void
sw_service_flickr_dispose (GObject *object)
{
  SwServiceFlickrPrivate *priv = SW_SERVICE_FLICKR (object)->priv;
  
  if (priv->auth_manager) {
    g_object_unref (priv->auth_manager);
    priv->auth_manager = NULL;
  }

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }
  
  if (priv->upload_proxy) {
    g_object_unref (priv->upload_proxy);
    priv->upload_proxy = NULL;
  }

  G_OBJECT_CLASS (sw_service_flickr_parent_class)->dispose (object);
}

static gboolean
sw_service_flickr_initable (GInitable    *initable,
                            GCancellable *cancellable,
                            GError      **error)
{
  SwServiceFlickr *flickr = SW_SERVICE_FLICKR (initable);
  SwServiceFlickrPrivate *priv = flickr->priv;
  const char *key = NULL, *secret = NULL;
  const gboolean NO_BINDING = FALSE;

  if (priv->inited)
    return TRUE;

  sw_keystore_get_key_secret ("flickr", &key, &secret);
  if (key == NULL || secret == NULL) {
    g_set_error_literal (error,
                         SW_SERVICE_ERROR,
                         SW_SERVICE_ERROR_NO_KEYS,
                         "No API key configured");
    return FALSE;
  }
  
  priv->auth_manager = flickr_auth_mgr_new ();
  priv->proxy = oauth_proxy_new (key, secret, FLICKR_REST_API_URL, NO_BINDING);

  sw_online_add_notify (online_notify, flickr);

  priv->inited = TRUE;

  credentials_updated (SW_SERVICE (flickr));

  return TRUE;
}

static void
initable_iface_init (gpointer g_iface,
                     gpointer iface_data)
{
  GInitableIface *klass = (GInitableIface *)g_iface;

  klass->init = sw_service_flickr_initable;
}

const static gchar *valid_queries[] = { "feed",
                                        "own",
                                        "friends-only",
                                        "x-flickr-search",
                                        NULL };

static const gchar *valid_contact_queries[] = { "people", NULL };


static gboolean
_check_query_validity (const gchar *query, const gchar *list[])
{
  gint i = 0;

  for (i = 0 ; list[i] != NULL ; i++)
  {
    if (g_str_equal (query, list[i]))
      return TRUE;
  }

  return FALSE;
}

static void
_flickr_query_open_view (SwQueryIface          *self,
                         const gchar           *query,
                         GHashTable            *params,
                         DBusGMethodInvocation *context)
{
  SwServiceFlickrPrivate *priv = GET_PRIVATE (self);
  SwItemView *item_view;
  const gchar *object_path;
  
  if (!_check_query_validity (query, valid_queries))
  {
    dbus_g_method_return_error (context,
                                g_error_new (SW_SERVICE_ERROR,
                                             SW_SERVICE_ERROR_INVALID_QUERY,
                                             "Query '%s' is invalid",
                                             query));
    return;
  }

  item_view = g_object_new (SW_TYPE_FLICKR_ITEM_VIEW,
                            "proxy", priv->proxy,
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

static void
_flickr_contacts_query_open_view (SwContactsQueryIface  *self,
                                  const gchar           *query,
                                  GHashTable            *params,
                                  DBusGMethodInvocation *context)
{
  SwServiceFlickrPrivate *priv = GET_PRIVATE (self);
  SwContactView *contact_view;
  const gchar *object_path;

  if (!_check_query_validity (query, valid_contact_queries))
  {
    dbus_g_method_return_error (context,
                                g_error_new (SW_SERVICE_ERROR,
                                             SW_SERVICE_ERROR_INVALID_QUERY,
                                             "Query '%s' is invalid",
                                             query));
    return;
  }

  contact_view = g_object_new (SW_TYPE_FLICKR_CONTACT_VIEW,
                            "proxy", priv->proxy,
                            "service", self,
                            "query", query,
                            "params", params,
                            NULL);

  /* Ensure the object gets disposed when the client goes away */
  sw_client_monitor_add (dbus_g_method_get_sender (context),
                         (GObject *)contact_view);

  object_path = sw_contact_view_get_object_path (contact_view);
  sw_contacts_query_iface_return_from_open_view (context, object_path);
}

static void
contacts_query_iface_init (gpointer g_iface,
                           gpointer iface_data)
{
  SwContactsQueryIfaceClass *klass = (SwContactsQueryIfaceClass*)g_iface;

  sw_contacts_query_iface_implement_open_view (klass,
      _flickr_contacts_query_open_view);
}

static void
query_iface_init (gpointer g_iface,
                  gpointer iface_data)
{
  SwQueryIfaceClass *klass = (SwQueryIfaceClass*)g_iface;

  sw_query_iface_implement_open_view (klass,
                                      _flickr_query_open_view);
}

static void
_create_upload_proxy (SwServiceFlickr *self) {
  
  gchar *consumer_key;
  gchar *consumer_secret;
  gchar *token;
  gchar *token_secret;
  const gboolean NO_BINDING = FALSE;
    
  g_object_get (self->priv->proxy,
                "consumer-key", &consumer_key,
                "consumer-secret", &consumer_secret,
                "token", &token,
                "token-secret", &token_secret,
                NULL);
    
  self->priv->upload_proxy = oauth_proxy_new_with_token (consumer_key,
                                                         consumer_secret,
                                                         token,
                                                         token_secret,
                                                         FLICKR_UPLOAD_URL,
                                                         NO_BINDING);
    
  g_free (consumer_key);
  g_free (consumer_secret);
  g_free (token);
  g_free (token_secret);
  
}

static RestProxyCall *
_create_upload_call (SwServiceFlickr *self,
                     const gchar *filename,
                     GError **error) {
  
  g_return_val_if_fail (filename, NULL);
  
  RestProxyCall *call = NULL;
  GMappedFile *map;
  GError *err = NULL;
  char *basename = NULL, *content_type = NULL;
  RestParam *param;
  
  /* Open the file */
  map = g_mapped_file_new (filename, FALSE, &err);
  if (err) {
    g_propagate_error (error, err);
    return NULL;
  }

  /* Get the file information */
  basename = g_path_get_basename (filename);
  content_type = g_content_type_guess (filename,
                                       (const guchar*) g_mapped_file_get_contents (map),
                                       g_mapped_file_get_length (map),
                                       NULL);

  /* Make the call */
  
  if (!self->priv->upload_proxy) {
    _create_upload_proxy (self);
  }

  call = rest_proxy_new_call (self->priv->upload_proxy);
  
  param = rest_param_new_with_owner ("photo",
                                     g_mapped_file_get_contents (map),
                                     g_mapped_file_get_length (map),
                                     content_type,
                                     basename,
                                     map,
                                     (GDestroyNotify)g_mapped_file_unref);
  rest_proxy_call_add_param_full (call, param);

  g_free (basename);
  g_free (content_type);

  return call;  

}

static gint
_flickr_upload (SwServiceFlickr              *self,
                const gchar                  *filename,
                GHashTable                   *params_in,
                GError                      **error,
                RestProxyCallUploadCallback   callback)
{
  RestProxyCall *call;
  gint opid;
  GError *err = NULL;

  call = _create_upload_call (self, filename, &err);

  if (err) {
    g_propagate_error (error, err);
    return -1;
  }

  sw_service_map_params (upload_params, params_in,
                         (SwServiceSetParamFunc) rest_proxy_call_add_param,
                         call);

  opid = sw_next_opid ();

  rest_proxy_call_upload (call, callback, (GObject *)self,
                          GINT_TO_POINTER (opid), NULL);

  return opid;
}

static void
on_photo_upload_cb (RestProxyCall *call,
                    gsize          total,
                    gsize          uploaded,
                    const GError  *error,
                    GObject       *weak_object,
                    gpointer       user_data)
{
  SwServiceFlickr *flickr = SW_SERVICE_FLICKR (weak_object);
  int opid = GPOINTER_TO_INT (user_data);

  if (error) {
    sw_photo_upload_iface_emit_photo_upload_progress (flickr, opid, -1, error->message);
    /* TODO: clean up */
  } else {
    /* TODO: check flickr error state */
    gint percent = (gdouble) uploaded / (gdouble) total * 100;
    sw_photo_upload_iface_emit_photo_upload_progress (flickr, opid,
                                                      percent, "");
  }
}

static void
_flickr_upload_photo (SwPhotoUploadIface    *self,
                      const gchar           *filename,
                      GHashTable            *params_in,
                      DBusGMethodInvocation *context)
{
  GError *error = NULL;
  int opid;

  opid = _flickr_upload (SW_SERVICE_FLICKR (self), filename, params_in,
                         &error, on_photo_upload_cb);

  if (opid == -1)
    dbus_g_method_return_error (context, error);
  else
    sw_photo_upload_iface_return_from_upload_photo (context, opid);
}


static void
photo_upload_iface_init (gpointer g_iface,
                         gpointer iface_data)
{
  SwPhotoUploadIfaceClass *klass = (SwPhotoUploadIfaceClass *)g_iface;

  sw_photo_upload_iface_implement_upload_photo (klass,
                                                _flickr_upload_photo);
}

static void
on_video_upload_cb (RestProxyCall *call,
                    gsize          total,
                    gsize          uploaded,
                    const GError  *error,
                    GObject       *weak_object,
                    gpointer       user_data)
{
  SwServiceFlickr *flickr = SW_SERVICE_FLICKR (weak_object);
  int opid = GPOINTER_TO_INT (user_data);

  if (error) {
    sw_video_upload_iface_emit_video_upload_progress (flickr, opid, -1, error->message);
    /* TODO: clean up */
  } else {
    /* TODO: check flickr error state */
    gint percent = (gdouble) uploaded / (gdouble) total * 100;
    sw_video_upload_iface_emit_video_upload_progress (flickr, opid,
                                                      percent, "");
  }
}

static void
_flickr_upload_video (SwVideoUploadIface    *self,
                      const gchar           *filename,
                      GHashTable            *params_in,
                      DBusGMethodInvocation *context)
{
  GError *error = NULL;
  int opid;

  opid = _flickr_upload (SW_SERVICE_FLICKR (self), filename, params_in,
                         &error, on_video_upload_cb);

  if (opid == -1)
    dbus_g_method_return_error (context, error);
  else
    sw_video_upload_iface_return_from_upload_video (context, opid);
}


static void
video_upload_iface_init (gpointer g_iface,
                         gpointer iface_data)
{
  SwVideoUploadIfaceClass *klass = (SwVideoUploadIfaceClass *)g_iface;

  sw_video_upload_iface_implement_upload_video (klass,
                                                _flickr_upload_video);
}


static void
sw_service_flickr_class_init (SwServiceFlickrClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwServiceClass *service_class = SW_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SwServiceFlickrPrivate));

  object_class->dispose = sw_service_flickr_dispose;

  service_class->get_name = sw_service_flickr_get_name;
  service_class->credentials_updated = credentials_updated;
  service_class->get_dynamic_caps = get_dynamic_caps;
  service_class->get_static_caps = get_static_caps;
}

static void
sw_service_flickr_init (SwServiceFlickr *self)
{
  SwServiceFlickrPrivate *priv = GET_PRIVATE (self);
  priv->inited = FALSE;
  priv->configured = FALSE;
  priv->authorised = FALSE;
  priv->auth_manager = NULL;
  priv->proxy = NULL;
  priv->upload_proxy = NULL;
}
