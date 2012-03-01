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

#ifndef FLICKRCREDENTIALS_H
#define FLICKRCREDENTIALS_H

#include "glib-object.h"

G_BEGIN_DECLS

/* UserCode header_top { */

typedef struct _FlickrCredentials FlickrCredentials;

typedef void (*FlickrCredentialsLoadCallback) (
    FlickrCredentials *credentials, 
    gboolean credentials_found,
    const GError *error,
    gpointer user_data
    );

typedef void (*FlickrCredentialsCheckCallback) (
    FlickrCredentials *credentials, 
    gboolean credentials_ok,
    const GError *error,
    gpointer user_data
    );

typedef struct _FlickrCredentialsLoadClosure {
    FlickrCredentials *credentials;
    FlickrCredentialsLoadCallback callback;
    gpointer user_data;
} FlickrCredentialsLoadClosure;

typedef struct _FlickrCredentialsCheckClosure {
    FlickrCredentials *credentials;
    FlickrCredentialsCheckCallback callback;
    gpointer user_data;
} FlickrCredentialsCheckClosure;


FlickrCredentialsLoadClosure *
flickr_credentials_load_closure_new (
    FlickrCredentials *credentials,
    FlickrCredentialsLoadCallback callback, 
    gpointer user_data
    );

void
flickr_credentials_load_closure_free (FlickrCredentialsLoadClosure *closure);

FlickrCredentialsCheckClosure *
flickr_credentials_check_closure_new (
    FlickrCredentials *credentials,
    FlickrCredentialsLoadCallback callback, 
    gpointer user_data
    );

void
flickr_credentials_check_closure_free (FlickrCredentialsCheckClosure *closure);

/* } UserCode */

typedef struct _FlickrCredentials FlickrCredentials;

typedef struct _FlickrCredentialsIface {
    
    GTypeInterface base_interface;
    
    /* == methods == */
    
    void
    (*load)(FlickrCredentials* self, FlickrCredentialsLoadCallback callback, 
        gpointer user_data);
    void
    (*check)(FlickrCredentials* self, FlickrCredentialsCheckCallback callback, 
        gpointer user_data);
    const gchar*
    (*get_consumer_key)(FlickrCredentials* self);
    const gchar*
    (*get_consumer_secret)(FlickrCredentials* self);
    const gchar*
    (*get_token)(FlickrCredentials* self);
    const gchar*
    (*get_token_secret)(FlickrCredentials* self);
    
} FlickrCredentialsIface;

GType
flickr_credentials_get_type();

/* ===== methods ===== */

void
flickr_credentials_load(FlickrCredentials* self, FlickrCredentialsLoadCallback callback, 
    gpointer user_data);

void
flickr_credentials_check(FlickrCredentials* self, FlickrCredentialsCheckCallback callback, 
    gpointer user_data);

const gchar*
flickr_credentials_get_consumer_key(FlickrCredentials* self);

const gchar*
flickr_credentials_get_consumer_secret(FlickrCredentials* self);

const gchar*
flickr_credentials_get_token(FlickrCredentials* self);

const gchar*
flickr_credentials_get_token_secret(FlickrCredentials* self);

/* ===== macros ===== */

#define FLICKR_TYPE_CREDENTIALS \
    (flickr_credentials_get_type())
#define FLICKR_CREDENTIALS(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), FLICKR_TYPE_CREDENTIALS, FlickrCredentials))
#define FLICKR_CREDENTIALS_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_CAST((cls), FLICKR_TYPE_CREDENTIALS, FlickrCredentialsIface))
#define FLICKR_IS_CREDENTIALS(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), FLICKR_TYPE_CREDENTIALS))
#define FLICKR_CREDENTIALS_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_INTERFACE((obj), FLICKR_TYPE_CREDENTIALS, FlickrCredentialsIface))
#define FLICKR_CREDENTIALS_GET_INTERFACE(obj) \
    (G_TYPE_INSTANCE_GET_INTERFACE((obj), FLICKR_TYPE_CREDENTIALS, FlickrCredentialsIface))

/* UserCode header_bottom { */

/* } UserCode */

G_END_DECLS

#endif

