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

#ifndef FLICKR_AUTH_ERROR_H
#define FLICKR_AUTH_ERROR_H

#include "glib-object.h"

G_BEGIN_DECLS
#define FLICKR_AUTH_ERROR g_quark_from_static_string("flickr_auth_error_quark")
  enum FlickrAuthError
{

  FLICKR_AUTH_ERROR_INVALID_URL_FORMAT = 1,
  FLICKR_AUTH_ERROR_FLICKR_ERROR
};

G_END_DECLS
#endif
