/*
 * libsocialweb - social data store
 * Copyright (C) 2008 - 2009 Intel Corporation.
 * Copyright (C) 2011 Collabora Ltd.
 *
 * Author: Rob Bradford <rob@linux.intel.com>
 *         Alban Crequy <alban.crequy@collabora.co.uk>
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

#ifndef _SW_TWITTER_CONTACT_VIEW
#define _SW_TWITTER_CONTACT_VIEW

#include <glib-object.h>
#include <libsocialweb/sw-contact-view.h>

G_BEGIN_DECLS

#define SW_TYPE_TWITTER_CONTACT_VIEW sw_twitter_contact_view_get_type()

#define SW_TWITTER_CONTACT_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_TWITTER_CONTACT_VIEW, \
  SwTwitterContactView))

#define SW_TWITTER_CONTACT_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_TYPE_TWITTER_CONTACT_VIEW, \
  SwTwitterContactViewClass))

#define SW_IS_TWITTER_CONTACT_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_TWITTER_CONTACT_VIEW))

#define SW_IS_TWITTER_CONTACT_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_TYPE_TWITTER_CONTACT_VIEW))

#define SW_TWITTER_CONTACT_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_TWITTER_CONTACT_VIEW, \
  SwTwitterContactViewClass))

typedef struct {
  SwContactView parent;
} SwTwitterContactView;

typedef struct {
  SwContactViewClass parent_class;
} SwTwitterContactViewClass;

GType sw_twitter_contact_view_get_type (void);

G_END_DECLS

#endif /* _SW_TWITTER_CONTACT_VIEW */

