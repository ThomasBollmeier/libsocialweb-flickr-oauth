/*
 * libsocialweb - social data store
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
#include <string.h>
#include <libsocialweb/sw-service.h>
#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-web.h>
#include <libsocialweb-keystore/sw-keystore.h>
#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>
#include <gconf/gconf-client.h>

#include "digg.h"

G_DEFINE_TYPE (SwServiceDigg, sw_service_digg, SW_TYPE_SERVICE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_SERVICE_DIGG, SwServiceDiggPrivate))

#define KEY_BASE "/apps/libsocialweb/services/digg"
#define KEY_USER KEY_BASE "/user"

struct _SwServiceDiggPrivate {
  gboolean running;
  RestProxy *proxy;
  GConfClient *gconf;
  char *user_id;
  guint gconf_notify_id;
};

/*
 * Run the call. Be aware each result may be required to be parsed in
 * different ways, just the raw result is returned
 */
static RestXmlNode *
digg_call (SwServiceDigg *digg,
           gchar         *subrequest)
{
  GError *error = NULL;
  RestProxyCall *call;
  RestXmlParser *parser;
  RestXmlNode *node;

  rest_proxy_bind (digg->priv->proxy, digg->priv->user_id, subrequest);

  /*
   * TODO Each time only 5 items are required, if more are available are
   * not retrieved. ID of the newer item for each category ("submissions",
   * "commented" and "dugg") may be maintained and searched into this set, if
   * it is not found another slice may be fetched (using parameter "offset")
   * and so on until all are downloaded
   */
  call = rest_proxy_new_call (digg->priv->proxy);
  rest_proxy_call_add_params (call,
                              "appkey", sw_keystore_get_key ("digg"),
                              "count", "5",
                              NULL);

  rest_proxy_call_sync (call, &error);
  parser = rest_xml_parser_new ();
  node = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));

  g_object_unref (call);

  /* No content, or wrong content */
  if (node == NULL || strcmp (node->name, "stories") != 0) {
    g_printerr ("Cannot make Digg call: %s\n",
                error ? error->message : "unknown reason");

    if (error)
      g_error_free (error);

    if (node) {
      rest_xml_node_unref (node);
      node = NULL;
    }
  }

  return node;
}

static void
retrieve_thumbnail (SwItem      *item,
                    RestXmlNode *node)
{
  RestXmlNode *thumb;

  thumb = rest_xml_node_find (node, "thumbnail");
  if (thumb == NULL)
    return;

  sw_item_request_image_fetch (item,
                               TRUE,
                               "thumbnail",
                               rest_xml_node_get_attr (thumb, "src"));
}

static void
start (SwService *service)
{
  SwServiceDigg *digg = SW_SERVICE_DIGG (service);

  digg->priv->running = TRUE;
}

static SwItem *
init_item (SwServiceDigg *digg,
           RestXmlNode   *node,
           gchar         *id_prefix)
{
  SwItem *item;
  char *id;

  item = sw_item_new ();
  sw_item_set_service (item, (SwService *)digg);

  if (id_prefix != NULL)
    id = g_strdup_printf ("%s_%s",
                          id_prefix,
                          rest_xml_node_get_attr (node, "id"));
  else
    id = g_strdup (rest_xml_node_get_attr (node, "id"));

  sw_item_take (item, "id", id);

  sw_item_put (item, "url", rest_xml_node_get_attr (node, "link"));
  sw_item_put (item, "title", rest_xml_node_find (node, "title")->content);
  sw_item_put (item, "content", rest_xml_node_find (node, "description")->content);
  retrieve_thumbnail (item, node);
  sw_item_take (item,
                "date",
                sw_time_t_to_string (atoi (rest_xml_node_get_attr (node,
                                                                   "submit_date"))));

  /*
   * "author" is assigned case by case
   */

  return item;
}

static void
assign_author (SwItem      *item,
               RestXmlNode *author)
{
  sw_item_put (item, "author", rest_xml_node_get_attr (author, "fullname"));
  sw_item_put (item, "authorid", rest_xml_node_get_attr (author, "name"));
  sw_item_request_image_fetch (item,
                               FALSE,
                               "authoricon",
                               rest_xml_node_get_attr (author, "icon"));
}

static void
collect_with_friends (SwServiceDigg *digg,
                      RestXmlNode   *node,
                      SwSet         *set,
                      gchar         *prefix)
{
  gchar *complete_prefix;
  RestXmlNode *friends, *author;
  SwItem *item;

  friends = rest_xml_node_find (node, "friends");

  for (author = rest_xml_node_find (friends, "user"); author; author = author->next) {
    complete_prefix = g_strdup_printf ("%s_%s", prefix,
                                       rest_xml_node_get_attr (author, "name"));
    item = init_item (digg, node, complete_prefix);
    g_free (complete_prefix);
    assign_author (item, author);
    sw_set_add (set, (GObject*)item);
  }
}

/* TODO: this is one huge main loop blockage and should be rewritten */
static void
refresh (SwService *service)
{
  SwServiceDigg *digg = SW_SERVICE_DIGG (service);
  RestXmlNode *root_node, *user_node;
  SwSet *set;
  SwItem *item;

  if (!digg->priv->running || digg->priv->user_id == NULL) {
    return;
  }

  set = sw_item_set_new ();

  root_node = digg_call (digg, "submissions");
  if (root_node != NULL) {
    for (user_node = rest_xml_node_find (root_node, "story");
         user_node;
         user_node = user_node->next) {
      item = init_item (digg, user_node, NULL);
      assign_author (item, rest_xml_node_find (user_node, "user"));
      sw_set_add (set, (GObject*)item);
    }

    rest_xml_node_unref (root_node);
  }

  root_node = digg_call (digg, "commented");
  if (root_node != NULL) {
    for (user_node = rest_xml_node_find (root_node, "story");
         user_node;
         user_node = user_node->next)
      collect_with_friends (digg, user_node, set, "comment");
    rest_xml_node_unref (root_node);
  }

  root_node = digg_call (digg, "dugg");
  if (root_node != NULL) {
    for (user_node = rest_xml_node_find (root_node, "story");
         user_node;
         user_node = user_node->next)
      collect_with_friends (digg, user_node, set, "digg");
    rest_xml_node_unref (root_node);
  }

  sw_service_emit_refreshed (service, set);
}

static void
user_changed_cb (GConfClient *client,
                 guint        cnxn_id,
                 GConfEntry  *entry,
                 gpointer     user_data)
{
  SwService *service = SW_SERVICE (user_data);
  SwServiceDigg *digg = SW_SERVICE_DIGG (service);
  SwServiceDiggPrivate *priv = digg->priv;
  const char *new_user;

  if (entry->value) {
    new_user = gconf_value_get_string (entry->value);
    if (new_user && new_user[0] == '\0')
      new_user = NULL;
  } else {
    new_user = NULL;
  }

  if (g_strcmp0 (new_user, priv->user_id) != 0) {
    g_free (priv->user_id);
    priv->user_id = g_strdup (new_user);

    if (priv->user_id)
      refresh (service);
    else
      sw_service_emit_refreshed (service, NULL);
  }
}

static const char *
get_name (SwService *service)
{
  return "digg";
}

static void
sw_service_digg_dispose (GObject *object)
{
  SwServiceDiggPrivate *priv = ((SwServiceDigg*)object)->priv;

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  if (priv->gconf) {
    gconf_client_notify_remove (priv->gconf,
                                priv->gconf_notify_id);
    gconf_client_remove_dir (priv->gconf, KEY_BASE, NULL);
    g_object_unref (priv->gconf);
    priv->gconf = NULL;
  }

  G_OBJECT_CLASS (sw_service_digg_parent_class)->dispose (object);
}

static void
sw_service_digg_finalize (GObject *object)
{
  G_OBJECT_CLASS (sw_service_digg_parent_class)->finalize (object);
}

static void
sw_service_digg_class_init (SwServiceDiggClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwServiceClass *service_class = SW_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SwServiceDiggPrivate));

  object_class->dispose = sw_service_digg_dispose;
  object_class->finalize = sw_service_digg_finalize;

  service_class->get_name = get_name;
  service_class->start = start;
  service_class->refresh = refresh;
}

static void
sw_service_digg_init (SwServiceDigg *self)
{
  SwServiceDiggPrivate *priv;

  priv = self->priv = GET_PRIVATE (self);

  priv->running = FALSE;

  priv->proxy = rest_proxy_new ("http://services.digg.com/user/%s/friends/%s", TRUE);

  /*
   * Digg webservice doesn't responses at all if no User-agent is set in requests
   * http://apidoc.digg.com/BasicConcepts#UserAgents
   */
  rest_proxy_set_user_agent (priv->proxy, "Sw");

  priv->gconf = gconf_client_get_default ();
  gconf_client_add_dir (priv->gconf, KEY_BASE,
                        GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  priv->gconf_notify_id = gconf_client_notify_add (priv->gconf,
                                                   KEY_USER,
                                                   user_changed_cb,
                                                   self,
                                                   NULL,
                                                   NULL);
  gconf_client_notify (priv->gconf, KEY_USER);
}
