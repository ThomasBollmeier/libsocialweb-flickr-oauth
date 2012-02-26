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

#include "flickr-credentials.h"

/* UserCode source_top { */

#include "flickr-credentials-marshaller.h"
/* add further includes... */

/* } UserCode */

/* ===== signals ===== */

enum {
    CREDENTIALS_AVAILABLE,
    CREDENTIALS_CHECKED,
    LAST_SIGNAL
};

static guint flickr_credentials_signals[LAST_SIGNAL] = {0};

/* ===== initialization & finalization ===== */

static void
flickr_credentials_base_init(FlickrCredentialsIface* iface) {
    
    static gboolean initialized = FALSE;
    
    if (initialized)
        return;
    
    /* add signals */
    
    /* UserCode signal_credentials_available { */
    
    flickr_credentials_signals[CREDENTIALS_AVAILABLE] = g_signal_new("credentials-available",
        FLICKR_TYPE_CREDENTIALS,
        G_SIGNAL_RUN_LAST|G_SIGNAL_DETAILED,
        G_STRUCT_OFFSET(FlickrCredentialsIface, credentials_available),
        NULL, /* accumulator */
        NULL,
        flickr_credentials_VOID__BOOLEAN,
        G_TYPE_NONE,
        1,
        G_TYPE_BOOLEAN
        );
    
    /* } UserCode */
    
    /* UserCode signal_credentials_checked { */
    
    flickr_credentials_signals[CREDENTIALS_CHECKED] = g_signal_new("credentials-checked",
        FLICKR_TYPE_CREDENTIALS,
        G_SIGNAL_RUN_LAST|G_SIGNAL_DETAILED,
        G_STRUCT_OFFSET(FlickrCredentialsIface, credentials_checked),
        NULL, /* accumulator */
        NULL,
        flickr_credentials_VOID__BOOLEAN_POINTER,
        G_TYPE_NONE,
        2,
        G_TYPE_BOOLEAN,
        G_TYPE_POINTER
        );
    
    /* } UserCode */
    
    /* UserCode interface_init { */
    
        /* further initializations... */
    
    /* } UserCode */
    
    initialized = TRUE;
    
}

static void
flickr_credentials_base_finalize(FlickrCredentialsIface* iface) {
    
    static gboolean finalized = FALSE;
    
    if (finalized)
        return;
    
    /* UserCode interface_finalize { */
    
        /* do some final stuff... */
    
    /* } UserCode */
    
    finalized = TRUE;
    
}

GType
flickr_credentials_get_type() {
    
    static GType type_id = 0;
    
    if (type_id == 0) {
        
        const GTypeInfo flickr_credentials_info = {
            sizeof(FlickrCredentialsIface),
            (GBaseInitFunc) flickr_credentials_base_init,
            (GBaseFinalizeFunc) flickr_credentials_base_finalize
            };
        
        type_id = g_type_register_static(
            G_TYPE_INTERFACE,
            "FlickrCredentials",
            &flickr_credentials_info,
            0
            );
            
        /* all classes are allowed to implement this interface: */
        g_type_interface_add_prerequisite(type_id, G_TYPE_OBJECT);
        
    }
    
    return type_id;
    
}

/* ===== methods ===== */

/**
 * flickr_credentials_load: 
 *
 * @self: a #FlickrCredentials instance
 */
void
flickr_credentials_load(FlickrCredentials* self) {
    
    FlickrCredentialsIface* iface = FLICKR_CREDENTIALS_GET_INTERFACE(self);
    
    iface->load(self);
    
}

/**
 * flickr_credentials_check: 
 *
 * @self: a #FlickrCredentials instance
 */
void
flickr_credentials_check(FlickrCredentials* self, GError** error) {
    
    FlickrCredentialsIface* iface = FLICKR_CREDENTIALS_GET_INTERFACE(self);
    
    iface->check(self, error);
    
}

/**
 * flickr_credentials_get_consumer_key: 
 *
 * @self: a #FlickrCredentials instance
 *
 * Return value: ...
 */
const gchar*
flickr_credentials_get_consumer_key(FlickrCredentials* self) {
    
    FlickrCredentialsIface* iface = FLICKR_CREDENTIALS_GET_INTERFACE(self);
    
    return iface->get_consumer_key(self);
    
}

/**
 * flickr_credentials_get_consumer_secret: 
 *
 * @self: a #FlickrCredentials instance
 *
 * Return value: ...
 */
const gchar*
flickr_credentials_get_consumer_secret(FlickrCredentials* self) {
    
    FlickrCredentialsIface* iface = FLICKR_CREDENTIALS_GET_INTERFACE(self);
    
    return iface->get_consumer_secret(self);
    
}

/**
 * flickr_credentials_get_token: 
 *
 * @self: a #FlickrCredentials instance
 *
 * Return value: ...
 */
const gchar*
flickr_credentials_get_token(FlickrCredentials* self) {
    
    FlickrCredentialsIface* iface = FLICKR_CREDENTIALS_GET_INTERFACE(self);
    
    return iface->get_token(self);
    
}

/**
 * flickr_credentials_get_token_secret: 
 *
 * @self: a #FlickrCredentials instance
 *
 * Return value: ...
 */
const gchar*
flickr_credentials_get_token_secret(FlickrCredentials* self) {
    
    FlickrCredentialsIface* iface = FLICKR_CREDENTIALS_GET_INTERFACE(self);
    
    return iface->get_token_secret(self);
    
}

/* UserCode source_bottom { */

/* } UserCode */

