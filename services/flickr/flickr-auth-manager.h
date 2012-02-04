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

#ifndef FLICKRAUTHMANAGER_H
#define FLICKRAUTHMANAGER_H

#include "glib-object.h"

G_BEGIN_DECLS


/* UserCode header_top { */

#include <rest/oauth-proxy.h>

typedef void (*FlickrCheckProxyCallback) (gboolean proxy_valid,
                                          const GError *validation_error,
                                          OAuthProxy *proxy,
                                          gpointer user_data
                                          );
typedef void (*FlickrCheckTokenCallback) (gboolean token_valid,
                                          const GError* validation_error,
                                          const gchar *consumer_key,
                                          const gchar *consumer_secret,
                                          const gchar *token,
                                          const gchar *token_secret,
                                          gpointer user_data);
typedef gchar * (*FlickrAuthorizeCallback) (const gchar *authorize_url,
                                            gpointer user_data);
typedef void (*FlickrAccessGrantedCallback) (const gchar *consumer_key,
                                             const gchar *consumer_secret,
                                             const gchar *token,
                                             const gchar *token_secret,
                                             gpointer user_data);

/* } UserCode */


typedef struct _FlickrAuthManager {
    
    GObject super;
    
    /* UserCode public_members { */
    
        /* add further public members...*/
    
    /* } UserCode */
    
    
} FlickrAuthManager;

/* ===== Class ===== */

typedef struct _FlickrAuthManagerClass {

    GObjectClass super_class;

} FlickrAuthManagerClass;

GType flickr_auth_mgr_get_type();

/* ===== instance creation ===== */

FlickrAuthManager*
flickr_auth_mgr_new();

/* ===== public methods ===== */

void
flickr_auth_mgr_check_proxy_async(FlickrAuthManager* self, OAuthProxy* proxy, FlickrCheckProxyCallback callback, 
    gpointer user_data, GError** error);

void
flickr_auth_mgr_check_token_async(FlickrAuthManager* self, const gchar* consumer_key, 
    const gchar* consumer_secret, const gchar* token, const gchar* token_secret, 
    FlickrCheckTokenCallback callback, gpointer user_data, 
    GError** error);

void
flickr_auth_mgr_get_access_token_async(FlickrAuthManager* self, const gchar* consumer_key, 
    const gchar* consumer_secret, FlickrAuthorizeCallback auth_callback, 
    FlickrAccessGrantedCallback access_callback, gpointer user_data, 
    GError** error);

/* ===== macros ===== */

#define FLICKR_TYPE_AUTH_MANAGER \
    (flickr_auth_mgr_get_type())
#define FLICKR_AUTH_MANAGER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), FLICKR_TYPE_AUTH_MANAGER, FlickrAuthManager))
#define FLICKR_AUTH_MANAGER_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_CAST((cls), FLICKR_TYPE_AUTH_MANAGER, FlickrAuthManagerClass))
#define FLICKR_IS_AUTH_MANAGER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), FLICKR_TYPE_AUTH_MANAGER))
#define FLICKR_IS_AUTH_MANAGER_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_TYPE((cls), FLICKR_TYPE_AUTH_MANAGER))
#define FLICKR_AUTH_MANAGER_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), FLICKR_TYPE_AUTH_MANAGER, FlickrAuthManagerClass))

/* UserCode header_bottom { */

/* } UserCode */

G_END_DECLS

#endif

