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

#ifndef FLICKRCREDENTIALSMANAGERGOA_H
#define FLICKRCREDENTIALSMANAGERGOA_H

#include "glib-object.h"

G_BEGIN_DECLS

#include "flickr-credentials.h"

/* UserCode header_top { */

    /* add includes here... */

/* } UserCode */

typedef struct _FlickrCredentialsManagerGOAPriv FlickrCredentialsManagerGOAPriv;

typedef struct _FlickrCredentialsManagerGOA {
    
    GObject super;
    
    /* UserCode public_members { */
    
        /* add further public members...*/
    
    /* } UserCode */
    
    FlickrCredentialsManagerGOAPriv* priv;
    
} FlickrCredentialsManagerGOA;

/* ===== Class ===== */

typedef struct _FlickrCredentialsManagerGOAClass {

    GObjectClass super_class;

} FlickrCredentialsManagerGOAClass;

GType flickr_credentials_mgr_goa_get_type();

/* ===== instance creation ===== */

FlickrCredentialsManagerGOA*
flickr_credentials_mgr_goa_new();

/* ===== macros ===== */

#define FLICKR_TYPE_CREDENTIALS_MANAGER_GOA \
    (flickr_credentials_mgr_goa_get_type())
#define FLICKR_CREDENTIALS_MANAGER_GOA(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), FLICKR_TYPE_CREDENTIALS_MANAGER_GOA, FlickrCredentialsManagerGOA))
#define FLICKR_CREDENTIALS_MANAGER_GOA_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_CAST((cls), FLICKR_TYPE_CREDENTIALS_MANAGER_GOA, FlickrCredentialsManagerGOAClass))
#define FLICKR_IS_CREDENTIALS_MANAGER_GOA(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), FLICKR_TYPE_CREDENTIALS_MANAGER_GOA))
#define FLICKR_IS_CREDENTIALS_MANAGER_GOA_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_TYPE((cls), FLICKR_TYPE_CREDENTIALS_MANAGER_GOA))
#define FLICKR_CREDENTIALS_MANAGER_GOA_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), FLICKR_TYPE_CREDENTIALS_MANAGER_GOA, FlickrCredentialsManagerGOAClass))

/* UserCode header_bottom { */

/* } UserCode */

G_END_DECLS

#endif

