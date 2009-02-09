/*
 * Mojito - social data store
 * Copyright (C) 2009 Intel Corporation.
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
#include <mojito/mojito-service.h>
#include <mojito/mojito-item.h>
#include <mojito/mojito-utils.h>
#include <mojito/mojito-web.h>
#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>
#include <libsoup/soup.h>

#include "lastfm.h"

G_DEFINE_TYPE (MojitoServiceLastfm, mojito_service_lastfm, MOJITO_TYPE_SERVICE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SERVICE_LASTFM, MojitoServiceLastfmPrivate))

struct _MojitoServiceLastfmPrivate {
  RestProxy *proxy;
  SoupSession *soup;
};

static char *
make_title (RestXmlNode *node)
{
  const char *track, *artist;

  track = rest_xml_node_find (node, "name")->content;
  artist = rest_xml_node_find (node, "artist")->content;

  if (track && artist) {
    return g_strdup_printf ("%s by %s", track, artist);
  } else if (track) {
    return g_strdup (track);
  } else {
    return g_strdup ("Unknown");
  }
}

static char *
get_image (RestXmlNode *node, SoupSession *session)
{
  /* TODO: prefer medium then large then small? */
  node = rest_xml_node_find (node, "image");
  if (node && node->content) {
    return mojito_web_download_image (session, node->content);
  } else {
    return NULL;
  }
}

static void
update (MojitoService *service, MojitoServiceDataFunc callback, gpointer user_data)
{
  MojitoServiceLastfm *lastfm = MOJITO_SERVICE_LASTFM (service);
  RestProxyCall *call;
  GError *error = NULL;
  RestXmlParser *parser;
  RestXmlNode *root, *node;
  MojitoSet *set;

  call = rest_proxy_new_call (lastfm->priv->proxy);
  rest_proxy_call_add_params (call,
                              /* TODO: get proper API key */
                              "api_key", "aa581f6505fd3ea79073ddcc2215cbc7",
                              "method", "user.getFriends",
                              /* TODO: parameterize */
                              "user", "rossburton",
                              NULL);
  /* TODO: GError */
  if (!rest_proxy_call_run (call, NULL, &error)) {
    g_printerr ("Cannot get Last.fm friends: %s\n", error->message);
    g_error_free (error);
    g_object_unref (call);

    callback (service, NULL, user_data);
    return;
  }

  parser = rest_xml_parser_new ();
  root = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));
  g_object_unref (call);
  g_object_unref (parser);

  set = mojito_item_set_new ();

  /* TODO: check for failure in lfm root element */
  for (node = rest_xml_node_find (root, "user"); node; node = node->next) {
    MojitoItem *item;
    RestXmlNode *recent, *track, *date;
    const char *s;

    call = rest_proxy_new_call (lastfm->priv->proxy);
    rest_proxy_call_add_params (call,
                                /* TODO: get proper API key */
                                "api_key", "aa581f6505fd3ea79073ddcc2215cbc7",
                                "method", "user.getRecentTracks",
                                "user", rest_xml_node_find (node, "name")->content,
                                "limit", "1",
                                NULL);
    /* TODO: GError */
    if (!rest_proxy_call_run (call, NULL, &error)) {
      g_printerr ("Cannot get Last.fm recent tracks: %s\n", error->message);
      g_error_free (error);
      g_object_unref (call);
      /* TODO: proper cleanup */
      callback (service, NULL, user_data);
      return;
    }

    parser = rest_xml_parser_new ();
    recent = rest_xml_parser_parse_from_data (parser,
                                                   rest_proxy_call_get_payload (call),
                                                   rest_proxy_call_get_payload_length (call));
    g_object_unref (call);
    g_object_unref (parser);

    track = rest_xml_node_find (recent, "track");

    if (!track) {
      rest_xml_node_unref (recent);
      continue;
    }

    item = mojito_item_new ();
    mojito_item_set_service (item, service);

    /* TODO user+track url? user+timestamp? */
    mojito_item_put (item, "id", rest_xml_node_find (track, "url")->content);
    mojito_item_put (item, "link", rest_xml_node_find (track, "url")->content);
    mojito_item_take (item, "title", make_title (track));
    mojito_item_put (item, "album", rest_xml_node_find (track, "album")->content);

    mojito_item_take (item, "thumbnail", get_image (track, lastfm->priv->soup));

    date = rest_xml_node_find (track, "date");
    mojito_item_take (item, "date", mojito_time_t_to_string (atoi (rest_xml_node_get_attr (date, "uts"))));

    s = rest_xml_node_find (node, "realname")->content;
    if (s) mojito_item_put (item, "author", s);
    mojito_item_put (item, "authorid", rest_xml_node_find (node, "name")->content);
    mojito_item_take (item, "authoricon", get_image (node, lastfm->priv->soup));

    rest_xml_node_unref (recent);

    mojito_set_add (set, (GObject*)item);
  }

  rest_xml_node_unref (root);

  callback (service, set, user_data);
}

static const char *
get_name (MojitoService *service)
{
  return "lastfm";
}

static void
mojito_service_lastfm_dispose (GObject *object)
{
  MojitoServiceLastfmPrivate *priv = ((MojitoServiceLastfm*)object)->priv;

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  if (priv->soup) {
    g_object_unref (priv->soup);
    priv->soup = NULL;
  }

  G_OBJECT_CLASS (mojito_service_lastfm_parent_class)->dispose (object);
}

static void
mojito_service_lastfm_finalize (GObject *object)
{
  G_OBJECT_CLASS (mojito_service_lastfm_parent_class)->finalize (object);
}

static void
mojito_service_lastfm_class_init (MojitoServiceLastfmClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MojitoServiceClass *service_class = MOJITO_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MojitoServiceLastfmPrivate));

  object_class->dispose = mojito_service_lastfm_dispose;
  object_class->finalize = mojito_service_lastfm_finalize;

  service_class->get_name = get_name;
  service_class->update = update;
}

static void
mojito_service_lastfm_init (MojitoServiceLastfm *self)
{
  self->priv = GET_PRIVATE (self);

  self->priv->proxy = rest_proxy_new ("http://ws.audioscrobbler.com/2.0/", FALSE);

  /* TODO: when the image fetching is async change this to async */
  self->priv->soup = soup_session_sync_new ();
}
