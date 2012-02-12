/*
 * libsocialweb - social data store
 * Copyright (C) 2008 - 2009 Intel Corporation.
 * Copyright (C) 2011 Collabora Ltd.
 *
 * Author: Thomas Bollmeier <tbollmeier@web.de>
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

#include "flickr-auth-manager.h"

#include <libsoup/soup.h>
#include <rest/rest-xml-parser.h>
#include "flickr-auth-error.h"

#define FLICKR_OAUTH_URL "http://www.flickr.com/services/oauth/"
#define FLICKR_REST_API_URL "http://api.flickr.com/services/rest/"

typedef struct _CheckProxyCallbackData
{
  FlickrCheckProxyCallback callback;
  OAuthProxy *proxy;
  gpointer user_data;
} CheckProxyCallbackData;

typedef struct _GetAccessCallbackData
{
  FlickrAuthorizeCallback auth_callback;
  FlickrAccessGrantedCallback access_callback;
  gpointer user_data;
} GetAccessCallbackData;

/* ===== private methods ===== */

static void
flickr_auth_mgr_on_proxy_checked (RestProxyCall * call,
                                  const GError * error,
                                  GObject * auth_manager,
                                  gpointer callback_data);

static void
flickr_auth_mgr_on_token_requested (OAuthProxy * proxy,
                                    const GError * error,
                                    GObject * auth_manager,
                                    gpointer user_data);

static void
flickr_auth_mgr_on_access_granted (OAuthProxy * proxy,
                                   const GError * error,
                                   GObject * auth_manager,
                                   gpointer user_data);

static gboolean
flickr_auth_mgr_is_token_valid (RestProxyCall * call,
                                GError ** error);

void flickr_auth_mgr_init (FlickrAuthManager * self);

void flickr_auth_mgr_dispose (GObject * obj);

void flickr_auth_mgr_finalize (GObject * obj);

static void
flickr_auth_mgr_free_proxy_check_cb_data (CheckProxyCallbackData * data)
{
  g_object_unref (data->proxy);
  g_free (data);
}

/* ===== properties ===== */

static void
flickr_auth_mgr_set_property (GObject * object,
			      guint property_id,
			      const GValue * value,
            GParamSpec * param_spec);

static void
flickr_auth_mgr_get_property (GObject * object,
			      guint property_id,
			      GValue * value,
            GParamSpec * param_spec);

/* ===== class initialization ===== */

static void
flickr_auth_mgr_class_init (FlickrAuthManagerClass * cls)
{

  GObjectClass *gobj_class = G_OBJECT_CLASS (cls);

  /* init class members ... */

  gobj_class->dispose = flickr_auth_mgr_dispose;
  gobj_class->finalize = flickr_auth_mgr_finalize;
  gobj_class->set_property = flickr_auth_mgr_set_property;
  gobj_class->get_property = flickr_auth_mgr_get_property;

}

/* ===== type initialization ==== */

static void
flickr_auth_mgr_instance_init (GTypeInstance * obj, gpointer cls)
{
}

GType
flickr_auth_mgr_get_type ()
{

  static GType type_id = 0;

  if (type_id == 0)
    {

      const GTypeInfo class_info = {
        sizeof (FlickrAuthManagerClass),
        NULL,			/* base initializer */
        NULL,			/* base finalizer */
        (GClassInitFunc) flickr_auth_mgr_class_init,
        NULL,			/* class finalizer */
        NULL,			/* class data */
        sizeof (FlickrAuthManager),
        0,
        flickr_auth_mgr_instance_init
      };

      type_id = g_type_register_static (G_TYPE_OBJECT,
					"FlickrAuthManager", &class_info, 0);
    }

  return type_id;

}

/* ===== implementation ===== */

/**
 * flickr_auth_mgr_new: 
 *
 *
 * Return value: a new #FlickrAuthManager instance
 */
FlickrAuthManager *
flickr_auth_mgr_new ()
{

  FlickrAuthManager *obj = g_object_new (FLICKR_TYPE_AUTH_MANAGER, NULL);

  return obj;

}

void
flickr_auth_mgr_dispose (GObject * obj)
{
  /* unref ... */
}

void
flickr_auth_mgr_finalize (GObject * obj)
{

}

void
flickr_auth_mgr_set_property (GObject * obj,
			      guint property_id,
			      const GValue * value,
                              GParamSpec * param_spec)
{

  G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, param_spec);

}

void
flickr_auth_mgr_get_property (GObject * obj,
			      guint property_id,
			      GValue * value,
                              GParamSpec * param_spec)
{

  G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, param_spec);

}

/* ===== public methods ===== */

/**
 * flickr_auth_mgr_check_proxy_async: 
 *
 * @self: a #FlickrAuthManager instance
 * @proxy: OAuthProxy for REST communication
 * @callback: Callback to handle check result
 * @user_data: User specified data that will be passed to callback
 * @error: a GError** instance or NULL
 */
void
flickr_auth_mgr_check_proxy_async (FlickrAuthManager * self,
				   OAuthProxy * proxy,
				   FlickrCheckProxyCallback callback,
				   gpointer user_data,
           GError ** error)
{

  gchar *url_format;
  RestProxy *check_proxy;
  gboolean proxy_created = FALSE;

  /* Check URL of proxy first. If it has the REST API URL it can be
     used directly. Otherwise we need a separate proxy for the call. */
  g_object_get (G_OBJECT (proxy), "url-format", &url_format, NULL);

  if (g_strcmp0 (url_format, FLICKR_REST_API_URL) == 0)
    {
      check_proxy = (RestProxy *) proxy;
    }
  else
    {

      gchar *consumer_key;
      gchar *consumer_secret;
      gchar *token;
      gchar *token_secret;

      g_object_get (G_OBJECT (proxy),
		    "consumer-key", &consumer_key,
		    "consumer-secret", &consumer_secret,
		    "token", &token, 
        "token-secret", &token_secret, 
        NULL);

      check_proxy = oauth_proxy_new_with_token (consumer_key,
                                                consumer_secret,
                                                token,
                                                token_secret,
                                                FLICKR_REST_API_URL,
                                                FALSE	/* <-- no binding */
                                               );
      proxy_created = TRUE;

      g_free (consumer_key);
      g_free (consumer_secret);
      g_free (token);
      g_free (token_secret);

    }

  g_free (url_format);

  RestProxyCall *call;
  CheckProxyCallbackData *cb_data = g_new0 (CheckProxyCallbackData, 1);
  GError *err = NULL;

  call = rest_proxy_new_call (check_proxy);
  rest_proxy_call_add_params (call,
    "method", "flickr.auth.oauth.checkToken", 
    NULL);

  cb_data->callback = callback;
  cb_data->proxy = g_object_ref (proxy);
  cb_data->user_data = user_data;

  rest_proxy_call_async (call,
                         flickr_auth_mgr_on_proxy_checked,
                         G_OBJECT (self),
                         cb_data, /* <-- free in callback */
                         &err);

  if (err)
    {
      flickr_auth_mgr_free_proxy_check_cb_data (cb_data);
      g_propagate_error (error, err);
    }

  if (proxy_created)
    {
      g_object_unref (check_proxy);
    }

}

/**
 * flickr_auth_mgr_get_access_token_async: 
 *
 * @self: a #FlickrAuthManager instance
 * @consumer_key: OAuth consumer key
 * @consumer_secret: OAuth consumer secret
 * @auth_callback: Callback to ask user for authorization
 * @access_callback: Callback to handle result of access process
 * @user_data: User data passed to (both) callbacks
 * @error: a GError** instance or NULL
 */
void
flickr_auth_mgr_get_access_token_async (FlickrAuthManager * self,
					const gchar * consumer_key,
					const gchar * consumer_secret,
					FlickrAuthorizeCallback auth_callback,
					FlickrAccessGrantedCallback
					access_callback, 
          gpointer user_data,
					GError ** error)
{

  OAuthProxy *proxy;
  gboolean NO_BINDING = FALSE;
  GetAccessCallbackData *cb_data = NULL;
  GError *err = NULL;

  g_return_if_fail (auth_callback != NULL);
  g_return_if_fail (access_callback != NULL);

  proxy = OAUTH_PROXY (oauth_proxy_new (consumer_key,
					consumer_secret,
					FLICKR_OAUTH_URL,
          NO_BINDING));

  cb_data = g_new0 (GetAccessCallbackData, 1);
  cb_data->auth_callback = auth_callback;
  cb_data->access_callback = access_callback;
  cb_data->user_data = user_data;

  oauth_proxy_request_token_async (proxy,
                                   "request_token",
                                   "oob", /* <-- OAuth 1.0a */
                                   flickr_auth_mgr_on_token_requested,
                                   G_OBJECT (self),
                                   cb_data,
                                   &err);

  if (err)
    {
      g_propagate_error (error, err);
      g_free (cb_data);
      g_object_unref (proxy);
    }

}

/* ===== private methods ===== */

static void
flickr_auth_mgr_on_proxy_checked (RestProxyCall * call,
                                  const GError * error,
                                  GObject * auth_manager,
                                  gpointer callback_data)
{

  CheckProxyCallbackData *cb_data = (CheckProxyCallbackData *) callback_data;
  gboolean proxy_valid;
  GError *err = NULL;

  proxy_valid = flickr_auth_mgr_is_token_valid (call, &err);

  cb_data->callback (proxy_valid, err, cb_data->proxy, cb_data->user_data);

  flickr_auth_mgr_free_proxy_check_cb_data (cb_data);
  if (err)
    {
      g_error_free (err);
    }
  g_object_unref (call);

}

static void
flickr_auth_mgr_on_token_requested (OAuthProxy * proxy,
                                    const GError * error,
                                    GObject * auth_manager,
                                    gpointer user_data)
{

  if (error)
    {
      g_free (user_data);
      g_object_unref (proxy);
      return;
    }

  GString *auth_url;
  GetAccessCallbackData *cb_data = (GetAccessCallbackData *) user_data;
  gchar *verifier;
  GError *err = NULL;

  auth_url = g_string_new (g_strdup (FLICKR_OAUTH_URL));
  g_string_append (auth_url, "authorize?oauth_token=");
  g_string_append (auth_url, oauth_proxy_get_token (proxy));

  /* Call authorization callback which might involve
     a user interaction (e.g. manual navigation to the AUTH_URL page)
     and entering a verifier code into some entry field in a dialog: */
  verifier = cb_data->auth_callback (auth_url->str, cb_data->user_data);

  g_string_free (auth_url, TRUE);

  if (verifier) {

      oauth_proxy_access_token_async (proxy,
				      "access_token",
				      verifier,
				      flickr_auth_mgr_on_access_granted,
				      auth_manager,
              cb_data,
              &err);

      if (err) {
        g_error_free (err);
        g_free (cb_data);
        g_object_unref (proxy);
      }
      
      g_free (verifier);

  } else {
    
    g_free (cb_data);
    g_object_unref (proxy);

  }

}

static void
flickr_auth_mgr_on_access_granted (OAuthProxy * proxy,
                                   const GError * error,
                                   GObject * auth_manager,
                                   gpointer user_data)
{

  if (error) {
    g_free (user_data);
    g_object_unref (proxy);
    return;
  }

  GetAccessCallbackData *cb_data = (GetAccessCallbackData *) user_data;
  gchar *consumer_key;
  gchar *consumer_secret;
  gchar *token;
  gchar *token_secret;

  g_object_get (G_OBJECT (proxy),
		"consumer-key", &consumer_key,
		"consumer-secret", &consumer_secret,
		"token", &token,
    "token-secret", &token_secret,
    NULL);

  cb_data->access_callback (consumer_key,
    consumer_secret,
    token, 
    token_secret,
    cb_data->user_data);

  g_free (consumer_key);
  g_free (consumer_secret);
  g_free (token);
  g_free (token_secret);

  g_free (cb_data);
  g_object_unref (proxy);

}

static gboolean
flickr_auth_mgr_is_token_valid (RestProxyCall * call, GError ** error)
{

  gboolean is_valid = TRUE;
  static RestXmlParser *parser = NULL;
  RestXmlNode *node = NULL;

  if (call == NULL) {
    return FALSE;
  }

  if (!SOUP_STATUS_IS_SUCCESSFUL (rest_proxy_call_get_status_code (call))) {
    
    if (error) {
      *error = g_error_new (FLICKR_AUTH_ERROR,
				FLICKR_AUTH_ERROR_FLICKR_ERROR,
				G_STRLOC ": error from Flickr: %s (%d)",
				rest_proxy_call_get_status_message (call),
				rest_proxy_call_get_status_code (call));
	 }
   is_valid = FALSE;
   goto exit;
  
  }

  if (!parser) {
    parser = rest_xml_parser_new ();
  }

  node = rest_xml_parser_parse_from_data (parser,
					  rest_proxy_call_get_payload (call),
					  rest_proxy_call_get_payload_length
					  (call));

  /* Invalid XML, or incorrect root */
  if (node == NULL || !g_str_equal (node->name, "rsp")) {

    if (error) {
      *error = g_error_new (FLICKR_AUTH_ERROR,
				FLICKR_AUTH_ERROR_FLICKR_ERROR,
				G_STRLOC ": cannot make Flickr call");
    }
    
    /* TODO: display the payload if its short */
    is_valid = FALSE;
    goto exit;
  
  }

  if (g_strcmp0 (rest_xml_node_get_attr (node, "stat"), "ok") != 0) {

    RestXmlNode *err;
    err = rest_xml_node_find (node, "err");
    if (err && error)	{
      *error = g_error_new (FLICKR_AUTH_ERROR,
        FLICKR_AUTH_ERROR_FLICKR_ERROR,
				G_STRLOC ": cannot make Flickr call: %s",
				rest_xml_node_get_attr (err, "msg"));
    }
    is_valid = FALSE;
    goto exit;
  
  }

  /* TODO: check permissions as well (we could have been granted read access only) */

exit:

  if (node) {
    rest_xml_node_unref (node);
  }

  return is_valid;

}
