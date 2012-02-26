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

#include "flickr-credentials-manager-goa.h"

/* UserCode source_top { */

#include <gio/gio.h>

/* } UserCode */

struct _FlickrCredentialsManagerGOAPriv {
    GDBusConnection* connection;
    gchar* consumer_key;
    gchar* consumer_secret;
    gchar* token;
    gchar* token_secret;
};

/* ===== interface methods (definition) ===== */

/* Credentials->load */
static void
flickr_credentials_mgr_goa_load_im(FlickrCredentials* obj);

/* Credentials->check */
static void
flickr_credentials_mgr_goa_check_im(FlickrCredentials* obj, GError** error);

/* Credentials->get_consumer_key */
static const gchar*
flickr_credentials_mgr_goa_get_consumer_key_im(FlickrCredentials* obj);

/* Credentials->get_consumer_secret */
static const gchar*
flickr_credentials_mgr_goa_get_consumer_secret_im(FlickrCredentials* obj);

/* Credentials->get_token */
static const gchar*
flickr_credentials_mgr_goa_get_token_im(FlickrCredentials* obj);

/* Credentials->get_token_secret */
static const gchar*
flickr_credentials_mgr_goa_get_token_secret_im(FlickrCredentials* obj);

/* ===== private methods ===== */

static void
flickr_credentials_mgr_goa_init_dbus(FlickrCredentialsManagerGOA* self);

static void
flickr_credentials_mgr_goa_clear(FlickrCredentialsManagerGOA* self);

static GDBusProxy*
flickr_credentials_mgr_goa_create_proxy_sync(FlickrCredentialsManagerGOA* self, const gchar* bus_name, 
    const gchar* object_path, const gchar* interface_name);

static void
flickr_credentials_mgr_goa_find_account(FlickrCredentialsManagerGOA* self);

static void
flickr_credentials_mgr_goa_get_access_data(FlickrCredentialsManagerGOA* self, const gchar* object_path, 
    GVariant* interfaces, GError** error);

static void
flickr_credentials_mgr_goa_on_managed_objects(GObject* mgr_proxy, GAsyncResult* result, gpointer user_data);

static void
flickr_credentials_mgr_goa_on_access_token(GObject* proxy, GAsyncResult* result, gpointer user_data);

void
flickr_credentials_mgr_goa_init(FlickrCredentialsManagerGOA* self);

void
flickr_credentials_mgr_goa_dispose(GObject* obj);

void
flickr_credentials_mgr_goa_finalize(GObject* obj);

/* UserCode private_methods { */

    /* define further methods...*/

/* } UserCode */

/* ===== properties ===== */

static void
flickr_credentials_mgr_goa_set_property(
    GObject* object,
    guint property_id,
    const GValue* value,
    GParamSpec* param_spec
    );

static void
flickr_credentials_mgr_goa_get_property(
    GObject* object,
    guint property_id,
    GValue* value,
    GParamSpec* param_spec
    );

/* ===== class initialization ===== */

static void
flickr_credentials_mgr_goa_class_init(FlickrCredentialsManagerGOAClass* cls) {
    
    GObjectClass* gobj_class = G_OBJECT_CLASS(cls);
    
    /* Register structure holding private attributes: */
    g_type_class_add_private (cls, sizeof(FlickrCredentialsManagerGOAPriv));
    
    /* UserCode class_init { */
    
        /* init class members ... */
    
    /* } UserCode */
    
    gobj_class->dispose = flickr_credentials_mgr_goa_dispose;
    gobj_class->finalize = flickr_credentials_mgr_goa_finalize;
    gobj_class->set_property = flickr_credentials_mgr_goa_set_property;
    gobj_class->get_property = flickr_credentials_mgr_goa_get_property;
    
    
}

/* ===== interfaces ===== */

static void
flickr_credentials_mgr_goa_flickr_credentials_init(FlickrCredentialsIface* iface) {
    
    iface->load = flickr_credentials_mgr_goa_load_im;
    iface->check = flickr_credentials_mgr_goa_check_im;
    iface->get_consumer_key = flickr_credentials_mgr_goa_get_consumer_key_im;
    iface->get_consumer_secret = flickr_credentials_mgr_goa_get_consumer_secret_im;
    iface->get_token = flickr_credentials_mgr_goa_get_token_im;
    iface->get_token_secret = flickr_credentials_mgr_goa_get_token_secret_im;
    
}

/* UserCode external_interfaces_init { */

    /* Initialize implementation of unmodeled interfaces... */

/* } UserCode */

/* ===== type initialization ==== */

static void
flickr_credentials_mgr_goa_instance_init(
    GTypeInstance* obj,
    gpointer cls
    ) {
    
    FlickrCredentialsManagerGOA* self = FLICKR_CREDENTIALS_MANAGER_GOA(obj);
    
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
        self,
        FLICKR_TYPE_CREDENTIALS_MANAGER_GOA,
        FlickrCredentialsManagerGOAPriv
        );
    
    /* UserCode instance_init { */
    
    self->priv->connection = NULL;

    self->priv->consumer_key = NULL;
    self->priv->consumer_secret = NULL;
    self->priv->token = NULL;
    self->priv->token_secret = NULL;
    
    
    /* } UserCode */
    
}

GType flickr_credentials_mgr_goa_get_type() {

    static GType type_id = 0;
    
    if (type_id == 0) {
        
        const GTypeInfo class_info = {
            sizeof(FlickrCredentialsManagerGOAClass),
            NULL, /* base initializer */
            NULL, /* base finalizer */
            (GClassInitFunc) flickr_credentials_mgr_goa_class_init,
            NULL, /* class finalizer */
            NULL, /* class data */
            sizeof(FlickrCredentialsManagerGOA),
            0,
            flickr_credentials_mgr_goa_instance_init
            };
        
        const GInterfaceInfo flickr_credentials_info = {
            (GInterfaceInitFunc) flickr_credentials_mgr_goa_flickr_credentials_init,
            NULL,
            NULL
            };
        
        type_id = g_type_register_static(
            G_TYPE_OBJECT,
            "FlickrCredentialsManagerGOA",
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
 * flickr_credentials_mgr_goa_new: 
 *
 *
 * Return value: a new #FlickrCredentialsManagerGOA instance
 */
FlickrCredentialsManagerGOA*
flickr_credentials_mgr_goa_new() {

    FlickrCredentialsManagerGOA* obj = g_object_new(FLICKR_TYPE_CREDENTIALS_MANAGER_GOA, NULL);
    
    flickr_credentials_mgr_goa_init(obj);
    
    return obj;

}

/**
 * flickr_credentials_mgr_goa_init: (skip)
 *
 */
void
flickr_credentials_mgr_goa_init(FlickrCredentialsManagerGOA* self) {
/* UserCode constructor { */

/* } UserCode */
}

void
flickr_credentials_mgr_goa_dispose(GObject* obj) {
/* UserCode dispose { */

    /* unref ...*/

/* } UserCode */
}

void
flickr_credentials_mgr_goa_finalize(GObject* obj){
    
    /* UserCode finalize { */
    
    FlickrCredentialsManagerGOA *self = FLICKR_CREDENTIALS_MANAGER_GOA (obj);

    if (self->priv->connection) {
        g_object_unref (self->priv->connection);
    }

    flickr_credentials_mgr_goa_clear (self);
    
    /* } UserCode */
    
}

void
flickr_credentials_mgr_goa_set_property(
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
flickr_credentials_mgr_goa_get_property(
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
flickr_credentials_mgr_goa_load_im(FlickrCredentials* obj) {
/* UserCode Credentials_load { */

    FlickrCredentialsManagerGOA *self = FLICKR_CREDENTIALS_MANAGER_GOA (obj);

    flickr_credentials_mgr_goa_clear (self);
    flickr_credentials_mgr_goa_find_account (self);    

/* } UserCode */
}

static void
flickr_credentials_mgr_goa_check_im(FlickrCredentials* obj, GError** error) {
/* UserCode Credentials_check { */

    /* TODO: implement... */

/* } UserCode */
}

static const gchar*
flickr_credentials_mgr_goa_get_consumer_key_im(FlickrCredentials* obj) {
/* UserCode Credentials_get_consumer_key { */

    FlickrCredentialsManagerGOA *self = FLICKR_CREDENTIALS_MANAGER_GOA (obj);

    return self->priv->consumer_key;

/* } UserCode */
}

static const gchar*
flickr_credentials_mgr_goa_get_consumer_secret_im(FlickrCredentials* obj) {
/* UserCode Credentials_get_consumer_secret { */

    FlickrCredentialsManagerGOA *self = FLICKR_CREDENTIALS_MANAGER_GOA (obj);

    return self->priv->consumer_secret;

/* } UserCode */
}

static const gchar*
flickr_credentials_mgr_goa_get_token_im(FlickrCredentials* obj) {
/* UserCode Credentials_get_token { */

    FlickrCredentialsManagerGOA *self = FLICKR_CREDENTIALS_MANAGER_GOA (obj);

    return self->priv->token;

/* } UserCode */
}

static const gchar*
flickr_credentials_mgr_goa_get_token_secret_im(FlickrCredentials* obj) {
/* UserCode Credentials_get_token_secret { */

    FlickrCredentialsManagerGOA *self = FLICKR_CREDENTIALS_MANAGER_GOA (obj);

    return self->priv->token_secret;

/* } UserCode */
}

/* ===== private methods ===== */

static void
flickr_credentials_mgr_goa_init_dbus(FlickrCredentialsManagerGOA* self) {
/* UserCode init_dbus { */

    if (self->priv->connection) {
        g_object_unref (self->priv->connection);
    }

    self->priv->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

/* } UserCode */
}

static void
flickr_credentials_mgr_goa_clear(FlickrCredentialsManagerGOA* self) {
/* UserCode clear { */

    if (self->priv->consumer_key) {
        g_free (self->priv->consumer_key);
        self->priv->consumer_key = NULL;
    }
    if (self->priv->consumer_secret) {
        g_free (self->priv->consumer_secret);
        self->priv->consumer_secret = NULL;
    }
    if (self->priv->token) {
        g_free (self->priv->token);
        self->priv->token = NULL;
    }
    if (self->priv->token_secret) {
        g_free (self->priv->token_secret);
        self->priv->token_secret = NULL;
    }

/* } UserCode */
}

static GDBusProxy*
flickr_credentials_mgr_goa_create_proxy_sync(FlickrCredentialsManagerGOA* self, const gchar* bus_name, 
    const gchar* object_path, const gchar* interface_name) {
/* UserCode create_proxy_sync { */

    GDBusProxy* res = NULL;

    res = g_dbus_proxy_new_sync(
        self->priv->connection,
        G_DBUS_PROXY_FLAGS_NONE,
        NULL, /* GDBusInterfaceInfo* */
        bus_name,
        object_path,
        interface_name,
        NULL, /* GCancellable* */
        NULL /* GError** */
        );

    return res;

/* } UserCode */
}

static void
flickr_credentials_mgr_goa_find_account(FlickrCredentialsManagerGOA* self) {
/* UserCode find_account { */

    GDBusProxy* mgr_proxy = NULL;

    if (!self->priv->connection) {
        flickr_credentials_mgr_goa_init_dbus (self);
    }

    mgr_proxy = flickr_credentials_mgr_goa_create_proxy_sync (
        self,
        "org.gnome.OnlineAccounts",
        "/org/gnome/OnlineAccounts", 
        "org.freedesktop.DBus.ObjectManager"
        );

    if (mgr_proxy) {

        g_dbus_proxy_call (
            mgr_proxy,
            "GetManagedObjects",
            NULL, /* GVariant *parameters */
            G_DBUS_CALL_FLAGS_NONE, /*GDBusCallFlags flags */
            -1, /* gint timeout_msec */
            NULL, /* GCancellable *cancellable */
            (GAsyncReadyCallback) flickr_credentials_mgr_goa_on_managed_objects,
            self
            );

    }

/* } UserCode */
}

static void
flickr_credentials_mgr_goa_get_access_data(FlickrCredentialsManagerGOA* self, const gchar* object_path, 
    GVariant* interfaces, GError** error) {
/* UserCode get_access_data { */

    GVariant *oauth = NULL;
    GVariant *consumer_key = NULL;
    GVariant *consumer_secret = NULL;
    GDBusProxy *proxy = NULL;
    gboolean called = FALSE;

    oauth = g_variant_lookup_value (interfaces, "org.gnome.OnlineAccounts.OAuthBased", NULL);
    if (!oauth) goto exit;

    consumer_key = g_variant_lookup_value (oauth, "ConsumerKey", NULL);
    if (!consumer_key) goto exit;

    consumer_secret = g_variant_lookup_value (oauth, "ConsumerSecret", NULL);
    if (!consumer_secret) goto exit;

    self->priv->consumer_key = g_strdup (g_variant_get_string (consumer_key, NULL));
    self->priv->consumer_secret = g_strdup (g_variant_get_string (consumer_secret, NULL));

    proxy = flickr_credentials_mgr_goa_create_proxy_sync (
        self,
        "org.gnome.OnlineAccounts",
        object_path,
        "org.gnome.OnlineAccounts.OAuthBased"
        );

    if (proxy) {

        g_dbus_proxy_call (
            proxy,
            "GetAccessToken",
            NULL, /* GVariant *parameters */
            G_DBUS_CALL_FLAGS_NONE, /*GDBusCallFlags flags */
            -1, /* gint timeout_msec */
            NULL, /* GCancellable *cancellable */
            (GAsyncReadyCallback) flickr_credentials_mgr_goa_on_access_token,
            self
            );

        called = TRUE;

    }

    exit:

    if (!called) {
        g_signal_emit_by_name (self, "credentials-available", FALSE);
    }

    if (proxy && !called) {
        g_object_unref (proxy);
    }

    if (consumer_key) {
        g_variant_unref (consumer_key);
    }

    if (consumer_secret) {
        g_variant_unref (consumer_secret);
    }

    if (oauth) {
        g_variant_unref (oauth);
    }

/* } UserCode */
}

static void
flickr_credentials_mgr_goa_on_managed_objects(GObject* mgr_proxy, GAsyncResult* result, gpointer user_data) {
/* UserCode on_managed_objects { */

    FlickrCredentialsManagerGOA *self = FLICKR_CREDENTIALS_MANAGER_GOA (user_data);
    gboolean error_occurred = FALSE;

    if (!result) {
        error_occurred = TRUE;
        goto exit;
    }

    GVariant *result_data = g_dbus_proxy_call_finish ((GDBusProxy *) mgr_proxy, result, NULL);

    if (!result_data) {
        error_occurred = TRUE;
        goto exit;
    }

    GVariant *content = g_variant_get_child_value (result_data, 0);
    g_variant_unref (result_data);
   
    GVariantIter iter;
    GVariant *child;
    GVariant *object_path;
    GVariant *interfaces;
    GVariant *account;
    GVariant *provider_type;
    gboolean done = FALSE;
    GError *error = NULL;
    
    g_variant_iter_init (&iter, content);

    while ((child = g_variant_iter_next_value(&iter))) {

        /* Check whether child is an account: */
        interfaces = g_variant_get_child_value (child, 1);

        account = g_variant_lookup_value (
            interfaces, 
            "org.gnome.OnlineAccounts.Account",
            NULL
            );

        if (account) {
              
            /* Is account an flickr account? */
            provider_type = g_variant_lookup_value (account, "ProviderType", NULL);

            if (provider_type) {

                if (g_strcmp0 (g_variant_get_string (provider_type, NULL), "flickr") == 0) {

                    object_path = g_variant_get_child_value (child, 0);
                
                    flickr_credentials_mgr_goa_get_access_data (
                        self, 
                        g_variant_get_string (object_path, NULL),
                        interfaces, 
                        &error
                        );

                    g_variant_unref (object_path);
                
                    if (error) {
                        error_occurred = TRUE;
                        g_error_free (error);
                    }
                
                    done = TRUE;

                }

                g_variant_unref (provider_type);
            }

            g_variant_unref (account);

        }

        g_variant_unref (interfaces);

        if (done) {
            break;
        }

    }

    g_variant_unref (content);

    exit:

    g_object_unref (mgr_proxy);

    if (error_occurred) {
        g_signal_emit_by_name (self, "credentials-available", FALSE);
    }

/* } UserCode */
}

static void
flickr_credentials_mgr_goa_on_access_token(GObject* proxy, GAsyncResult* result, gpointer user_data) {
/* UserCode on_access_token { */

    FlickrCredentialsManagerGOA *self = FLICKR_CREDENTIALS_MANAGER_GOA (user_data);
    GVariant *data = g_dbus_proxy_call_finish ((GDBusProxy *) proxy, result, NULL);
    GVariant *token = NULL;
    GVariant *token_secret = NULL;
    gboolean credentials_available = FALSE;

    if (!data) {
        goto exit;
    }

    token = g_variant_get_child_value (data, 0);
    token_secret = g_variant_get_child_value (data, 1);

    if (token && token_secret) {

        credentials_available = TRUE;
        
        self->priv->token = g_strdup (g_variant_get_string (token, NULL));
        self->priv->token_secret = g_strdup (g_variant_get_string (token_secret, NULL));

    } 

    exit:

    if (token) {
        g_variant_unref (token);
    }

    if (token_secret) {
        g_variant_unref (token_secret);
    }

    if (data) {
        g_variant_unref (data);
    }

    g_object_unref (proxy);

    g_signal_emit_by_name (self, "credentials-available", credentials_available);

/* } UserCode */
}

/* UserCode source_bottom { */

/* } UserCode */
