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

#include "flickr-credentials-manager.h"

/* UserCode source_top { */

#include <rest/oauth-proxy.h>
#include <libsoup/soup.h>
#include <rest/rest-xml-parser.h>
#include <libsocialweb-keystore/sw-keystore.h>
#include <libsocialweb-keyfob/sw-keyfob.h>
#include "flickr-credentials-error.h"

#define FLICKR_REST_API_URL "http://api.flickr.com/services/rest/"
#define FLICKR_SERVICE_NAME "flickr"

#define FREE(data) if (data) { g_free (data); data = NULL; }
#define OBJ_UNREF(obj) if (obj) { g_object_unref (obj); obj = NULL; }

/* } UserCode */

struct _FlickrCredentialsManagerPriv {
    gchar* consumer_key;
    gchar* consumer_secret;
    gchar* token;
    gchar* token_secret;
};

/* ===== interface methods (definition) ===== */

/* Credentials->load */
static void
flickr_credentials_mgr_load_im(FlickrCredentials* obj, FlickrCredentialsLoadCallback callback, 
    gpointer user_data);

/* Credentials->check */
static void
flickr_credentials_mgr_check_im(FlickrCredentials* obj, FlickrCredentialsCheckCallback callback, 
    gpointer user_data);

/* Credentials->get_consumer_key */
static const gchar*
flickr_credentials_mgr_get_consumer_key_im(FlickrCredentials* obj);

/* Credentials->get_consumer_secret */
static const gchar*
flickr_credentials_mgr_get_consumer_secret_im(FlickrCredentials* obj);

/* Credentials->get_token */
static const gchar*
flickr_credentials_mgr_get_token_im(FlickrCredentials* obj);

/* Credentials->get_token_secret */
static const gchar*
flickr_credentials_mgr_get_token_secret_im(FlickrCredentials* obj);

/* ===== private methods ===== */

static void
flickr_credentials_mgr_on_credentials_checked(RestProxyCall* call, const GError* error, GObject* manager, 
    gpointer closure);

static gboolean
flickr_credentials_mgr_credentials_ok(RestProxyCall* call, GError** error);

static void
flickr_credentials_mgr_on_token_looked_up(RestProxy* proxy, gboolean found, gpointer closure);

void
flickr_credentials_mgr_init(FlickrCredentialsManager* self);

void
flickr_credentials_mgr_dispose(GObject* obj);

void
flickr_credentials_mgr_finalize(GObject* obj);

/* UserCode private_methods { */

    /* define further methods...*/

/* } UserCode */

/* ===== properties ===== */

static void
flickr_credentials_mgr_set_property(
    GObject* object,
    guint property_id,
    const GValue* value,
    GParamSpec* param_spec
    );

static void
flickr_credentials_mgr_get_property(
    GObject* object,
    guint property_id,
    GValue* value,
    GParamSpec* param_spec
    );

/* ===== class initialization ===== */

static void
flickr_credentials_mgr_class_init(FlickrCredentialsManagerClass* cls) {
    
    GObjectClass* gobj_class = G_OBJECT_CLASS(cls);
    
    /* Register structure holding private attributes: */
    g_type_class_add_private (cls, sizeof(FlickrCredentialsManagerPriv));
    
    /* UserCode class_init { */
    
        /* init class members ... */
    
    /* } UserCode */
    
    gobj_class->dispose = flickr_credentials_mgr_dispose;
    gobj_class->finalize = flickr_credentials_mgr_finalize;
    gobj_class->set_property = flickr_credentials_mgr_set_property;
    gobj_class->get_property = flickr_credentials_mgr_get_property;
    
    
}

/* ===== interfaces ===== */

static void
flickr_credentials_mgr_flickr_credentials_init(FlickrCredentialsIface* iface) {
    
    iface->load = flickr_credentials_mgr_load_im;
    iface->check = flickr_credentials_mgr_check_im;
    iface->get_consumer_key = flickr_credentials_mgr_get_consumer_key_im;
    iface->get_consumer_secret = flickr_credentials_mgr_get_consumer_secret_im;
    iface->get_token = flickr_credentials_mgr_get_token_im;
    iface->get_token_secret = flickr_credentials_mgr_get_token_secret_im;
    
}

/* UserCode external_interfaces_init { */

    /* Initialize implementation of unmodeled interfaces... */

/* } UserCode */

/* ===== type initialization ==== */

static void
flickr_credentials_mgr_instance_init(
    GTypeInstance* obj,
    gpointer cls
    ) {
    
    FlickrCredentialsManager* self = FLICKR_CREDENTIALS_MANAGER(obj);
    
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
        self,
        FLICKR_TYPE_CREDENTIALS_MANAGER,
        FlickrCredentialsManagerPriv
        );
    
    /* UserCode instance_init { */
    
    self->priv->consumer_key = NULL;
    self->priv->consumer_secret = NULL;
    self->priv->token = NULL;
    self->priv->token_secret = NULL;
        
    /* } UserCode */
    
}

GType flickr_credentials_mgr_get_type() {

    static GType type_id = 0;
    
    if (type_id == 0) {
        
        const GTypeInfo class_info = {
            sizeof(FlickrCredentialsManagerClass),
            NULL, /* base initializer */
            NULL, /* base finalizer */
            (GClassInitFunc) flickr_credentials_mgr_class_init,
            NULL, /* class finalizer */
            NULL, /* class data */
            sizeof(FlickrCredentialsManager),
            0,
            flickr_credentials_mgr_instance_init
            };
        
        const GInterfaceInfo flickr_credentials_info = {
            (GInterfaceInitFunc) flickr_credentials_mgr_flickr_credentials_init,
            NULL,
            NULL
            };
        
        type_id = g_type_register_static(
            G_TYPE_OBJECT,
            "FlickrCredentialsManager",
            &class_info,
            0
            );
        
        g_type_add_interface_static(
            type_id,
            FLICKR_TYPE_CREDENTIALS,
            &flickr_credentials_info
            );
        
    /* UserCode external_interfaces_register { */
    
        /* Register implementation of unmodeled interfaces... */
    
    /* } UserCode */
    
    }
    
    return type_id;

}

/* ===== implementation ===== */

/**
 * flickr_credentials_mgr_new: 
 *
 *
 * Return value: a new #FlickrCredentialsManager instance
 */
FlickrCredentialsManager*
flickr_credentials_mgr_new() {

    FlickrCredentialsManager* obj = g_object_new(FLICKR_TYPE_CREDENTIALS_MANAGER, NULL);
    
    flickr_credentials_mgr_init(obj);
    
    return obj;

}

/**
 * flickr_credentials_mgr_init: (skip)
 *
 */
void
flickr_credentials_mgr_init(FlickrCredentialsManager* self) {
/* UserCode constructor { */

    /* == init your members == */

/* } UserCode */
}

void
flickr_credentials_mgr_dispose(GObject* obj) {
/* UserCode dispose { */

    /* unref ...*/

/* } UserCode */
}

void
flickr_credentials_mgr_finalize(GObject* obj){
    
    /* UserCode finalize { */

    FlickrCredentialsManager *self = FLICKR_CREDENTIALS_MANAGER (obj);
    
    FREE (self->priv->consumer_key)
    FREE (self->priv->consumer_secret)
    FREE (self->priv->token)
    FREE (self->priv->token_secret)
    
    /* } UserCode */
    
}

void
flickr_credentials_mgr_set_property(
    GObject* obj,
    guint property_id,
    const GValue* value,
    GParamSpec* param_spec
    ) {
    
    G_OBJECT_WARN_INVALID_PROPERTY_ID(
        obj,
        property_id,
        param_spec
        );

}

void
flickr_credentials_mgr_get_property(
    GObject* obj,
    guint property_id,
    GValue* value,
    GParamSpec* param_spec
    ) {
    
    G_OBJECT_WARN_INVALID_PROPERTY_ID(
        obj,
        property_id,
        param_spec
        );

}

/* ===== interface methods (implementation) ===== */

static void
flickr_credentials_mgr_load_im(FlickrCredentials* obj, FlickrCredentialsLoadCallback callback, 
    gpointer user_data) {
/* UserCode Credentials_load { */

    FlickrCredentialsManager *self = FLICKR_CREDENTIALS_MANAGER (obj);
    const char *consumer_key = NULL;
    const char *consumer_secret = NULL;
    RestProxy *proxy;

    sw_keystore_get_key_secret (FLICKR_SERVICE_NAME, &consumer_key, &consumer_secret);

    if (consumer_key && consumer_secret) {

        FREE (self->priv->consumer_key)
        FREE (self->priv->consumer_secret)
        FREE (self->priv->token)
        FREE (self->priv->token_secret)

        self->priv->consumer_key = g_strdup (consumer_key);
        self->priv->consumer_secret = g_strdup (consumer_secret);

        proxy = oauth_proxy_new (
            self->priv->consumer_key,
            self->priv->consumer_secret,
            FLICKR_REST_API_URL,
            FALSE /* <-- no binding */
            );

        FlickrCredentialsLoadClosure *closure = flickr_credentials_load_closure_new (
            FLICKR_CREDENTIALS (self),
            callback,
            user_data
            );

        sw_keyfob_oauth (
            OAUTH_PROXY (proxy),
            flickr_credentials_mgr_on_token_looked_up,
            closure
            );
        
    } else {
        
        GError *error = g_error_new(
            FLICKR_CREDENTIALS_ERROR,
            FLICKR_CREDENTIALS_ERROR_NO_KEYS_PROVIDED,
            G_STRLOC ": no consumer key has been provided"
            );

        callback (FLICKR_CREDENTIALS (self), FALSE, error, user_data);

        g_error_free (error);

    }

/* } UserCode */
}

static void
flickr_credentials_mgr_check_im(FlickrCredentials* obj, FlickrCredentialsCheckCallback callback, 
    gpointer user_data) {
/* UserCode Credentials_check { */

    FlickrCredentialsManager *self = FLICKR_CREDENTIALS_MANAGER (obj);
    RestProxy *check_proxy = NULL;
    const gboolean NO_BINDING = FALSE;
    RestProxyCall *call = NULL;
    GError* error = NULL;

    if (!self->priv->consumer_key || 
        !self->priv->consumer_secret ||
        !self->priv->token ||
        !self->priv->token_secret) {

        error = g_error_new(
            FLICKR_CREDENTIALS_ERROR,
            FLICKR_CREDENTIALS_ERROR_NO_KEYS_PROVIDED,
            "No credentials have been provided"
            );
        
        callback (FLICKR_CREDENTIALS (self), FALSE, error, user_data);

        g_error_free (error);

        return; 

    }

    check_proxy = oauth_proxy_new_with_token(
        self->priv->consumer_key,
        self->priv->consumer_secret,
        self->priv->token,
        self->priv->token_secret,
        FLICKR_REST_API_URL,
        NO_BINDING
        );

          
    call = rest_proxy_new_call (check_proxy);
    rest_proxy_call_add_params (
        call,
        "method", "flickr.auth.oauth.checkToken",
        NULL
        );

    FlickrCredentialsCheckClosure *closure = 
        flickr_credentials_check_closure_new (
            FLICKR_CREDENTIALS (self),
            callback,
            user_data
            );

    rest_proxy_call_async(
        call,
        flickr_credentials_mgr_on_credentials_checked,
        G_OBJECT (self),
        closure,
        &error
        );

    if (error) {

        callback (FLICKR_CREDENTIALS (self), FALSE, error, user_data);

        flickr_credentials_check_closure_free (closure);
        g_object_unref (call);

    }

    g_object_unref (check_proxy);
  
/* } UserCode */
}

static const gchar*
flickr_credentials_mgr_get_consumer_key_im(FlickrCredentials* obj) {
/* UserCode Credentials_get_consumer_key { */

    FlickrCredentialsManager *self = FLICKR_CREDENTIALS_MANAGER (obj);

    return self->priv->consumer_key;

/* } UserCode */
}

static const gchar*
flickr_credentials_mgr_get_consumer_secret_im(FlickrCredentials* obj) {
/* UserCode Credentials_get_consumer_secret { */

    FlickrCredentialsManager *self = FLICKR_CREDENTIALS_MANAGER (obj);

    return self->priv->consumer_secret;

/* } UserCode */
}

static const gchar*
flickr_credentials_mgr_get_token_im(FlickrCredentials* obj) {
/* UserCode Credentials_get_token { */

    FlickrCredentialsManager *self = FLICKR_CREDENTIALS_MANAGER (obj);

    return self->priv->token;

/* } UserCode */
}

static const gchar*
flickr_credentials_mgr_get_token_secret_im(FlickrCredentials* obj) {
/* UserCode Credentials_get_token_secret { */

    FlickrCredentialsManager *self = FLICKR_CREDENTIALS_MANAGER (obj);

    return self->priv->token_secret;

/* } UserCode */
}

/* ===== private methods ===== */

static void
flickr_credentials_mgr_on_credentials_checked(RestProxyCall* call, const GError* error, GObject* manager, 
    gpointer closure) {
/* UserCode on_credentials_checked { */

    FlickrCredentialsCheckClosure *data = (FlickrCredentialsCheckClosure *) closure;
    gboolean credentials_ok;
    GError *err = NULL;

    if (!error) {

        credentials_ok = flickr_credentials_mgr_credentials_ok (call, &err);

        data->callback (data->credentials, credentials_ok, err, data->user_data);

        if (err) {
            g_error_free (err);
        }

    } else {

        data->callback (data->credentials, FALSE, error, data->user_data);

    }

    g_object_unref (call);

    flickr_credentials_check_closure_free (data);

/* } UserCode */
}

static gboolean
flickr_credentials_mgr_credentials_ok(RestProxyCall* call, GError** error) {
/* UserCode credentials_ok { */

    gboolean credentials_ok = TRUE;
    static RestXmlParser *parser = NULL;
    RestXmlNode *node = NULL;

    if (call == NULL) {
        return FALSE;
    }

    if (!SOUP_STATUS_IS_SUCCESSFUL (rest_proxy_call_get_status_code (call))) {
        if (error) {
          *error = g_error_new (
            FLICKR_CREDENTIALS_ERROR,
            FLICKR_CREDENTIALS_ERROR_FAILURE,
            G_STRLOC ": error from Flickr: %s (%d)",
            rest_proxy_call_get_status_message (call),
            rest_proxy_call_get_status_code (call)
            );
        }
        credentials_ok = FALSE;
        goto exit;
    }

    if (!parser) {
        parser = rest_xml_parser_new ();
    }

    node = rest_xml_parser_parse_from_data (
        parser,
        rest_proxy_call_get_payload (call),
        rest_proxy_call_get_payload_length (call)
        );

    /* Invalid XML, or incorrect root */
    if (node == NULL || !g_str_equal (node->name, "rsp")) {
        if (error) {
            *error = g_error_new (
                FLICKR_CREDENTIALS_ERROR,
                FLICKR_CREDENTIALS_ERROR_FAILURE,
                G_STRLOC ": cannot make Flickr call"
                );
        }
        /* TODO: display the payload if its short */
        credentials_ok = FALSE;
        goto exit;
    }

    if (g_strcmp0 (rest_xml_node_get_attr (node, "stat"), "ok") != 0) {
        RestXmlNode *err;
        err = rest_xml_node_find (node, "err");
        if (err && error) {
          *error = g_error_new (
            FLICKR_CREDENTIALS_ERROR,
            FLICKR_CREDENTIALS_ERROR_FAILURE,
            G_STRLOC ": cannot make Flickr call: %s",
            rest_xml_node_get_attr (err, "msg")
            );
        }
        credentials_ok = FALSE;
        goto exit;
    }

    /* TODO: check permissions as well (we could have been granted read access only) */

    exit:

    if (node) {
        rest_xml_node_unref (node);
    }

    return credentials_ok;

/* } UserCode */
}

static void
flickr_credentials_mgr_on_token_looked_up(RestProxy* proxy, gboolean found, gpointer closure) {
/* UserCode on_token_looked_up { */

    FlickrCredentialsLoadClosure *data = (FlickrCredentialsLoadClosure *)closure;
    FlickrCredentialsManager *self = FLICKR_CREDENTIALS_MANAGER (data->credentials);
    GError *error = NULL;

    if (found) {

        g_object_get (
            G_OBJECT (proxy),
            "token", &self->priv->token,
            "token-secret", &self->priv->token_secret,
            NULL
            );

    } else {

        error = g_error_new (
            FLICKR_CREDENTIALS_ERROR,
            FLICKR_CREDENTIALS_ERROR_NO_KEYS_PROVIDED,
            G_STRLOC ": no access token could be found for Flickr service"
            );
    }

    data->callback (data->credentials, found, error, data->user_data);

    if (error) {
        g_error_free (error);
    }

    flickr_credentials_load_closure_free (data);

    g_object_unref (proxy);

/* } UserCode */
}

/* UserCode source_bottom { */

/* } UserCode */

