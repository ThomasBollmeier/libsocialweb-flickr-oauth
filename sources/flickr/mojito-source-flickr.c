#include <stdlib.h>
#include "mojito-source-flickr.h"
#include "mojito-photos.h"
#include <mojito/mojito-item.h>
#include <mojito/mojito-set.h>
#include <mojito/mojito-utils.h>
#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>

G_DEFINE_TYPE (MojitoSourceFlickr, mojito_source_flickr, MOJITO_TYPE_SOURCE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SOURCE_FLICKR, MojitoSourceFlickrPrivate))

struct _MojitoSourceFlickrPrivate {
  RestProxy *proxy;
  char *user_id;
};

static char *
construct_photo_page_url (RestXmlNode *node)
{
  g_assert (node != NULL);

  return g_strdup_printf ("http://www.flickr.com/photos/%s/%s/",
                          rest_xml_node_get_attr (node, "owner"),
                          rest_xml_node_get_attr (node, "id"));
}

typedef struct {
  MojitoSourceDataFunc callback;
  gpointer user_data;
}UpdateData;

static void
flickr_callback (RestProxyCall *call,
                 GError *error,
                 GObject *weak_object,
                 gpointer user_data)
{
  MojitoSourceFlickr *source = MOJITO_SOURCE_FLICKR (weak_object);
  MojitoSourceFlickrPrivate *priv = source->priv;
  UpdateData *data = user_data;
  RestXmlParser *parser;
  RestXmlNode *root, *node;
  MojitoSet *set;

  if (error) {
    g_printerr ("Cannot get Flickr photos: %s", error->message);
    return;
  }

  parser = rest_xml_parser_new ();
  root = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));
  g_object_unref (call);

  node = rest_xml_node_find (root, "rsp");
  /* TODO: check for failure */

  set = mojito_item_set_new ();

  node = rest_xml_node_find (root, "photos");
  for (node = rest_xml_node_find (node, "photo"); node; node = node->next) {
    char *url;
    MojitoItem *item;
    gint64 date;

    item = mojito_item_new ();
    mojito_item_set_source (item, (MojitoSource*)source);

    url = construct_photo_page_url (node);
    mojito_item_put (item, "id", url);
    mojito_item_put (item, "title", rest_xml_node_get_attr (node, "title"));
    mojito_item_put (item, "url", url);
    g_free (url);

    date = atoi (rest_xml_node_get_attr (node, "dateupload"));
    mojito_item_take (item, "date", mojito_time_t_to_string (date));

    mojito_set_add (set, G_OBJECT (item));
    g_object_unref (item);
  }

  rest_xml_node_free (root);
  g_object_unref (parser);

  data->callback ((MojitoSource*)source, set, data->user_data);

  g_slice_free (UpdateData, data);
}

static void
update (MojitoSource *source, MojitoSourceDataFunc callback, gpointer user_data)
{
  MojitoSourceFlickr *flickr = (MojitoSourceFlickr*)source;
  UpdateData *data;
  RestProxyCall *call;

  g_debug ("Updating Flickr");

  data = g_slice_new (UpdateData);
  data->callback = callback;
  data->user_data = user_data;

  call = rest_proxy_new_call (flickr->priv->proxy);
  rest_proxy_call_add_params (call,
                              "method", "flickr.photos.getContactsPublicPhotos",
                              /* TODO: this is a temporary key */
                              "api_key", "cf4e02fc57240a9b07346ad26e291080",
                              "user_id", flickr->priv->user_id,
                              "extras", "date_upload",
                              NULL);
  /* TODO: GError */
  rest_proxy_call_async (call, flickr_callback, (GObject*)source, data, NULL);
}

static char *
mojito_source_flickr_get_name (MojitoSource *source)
{
  return "flickr";
}

static void
mojito_source_flickr_dispose (GObject *object)
{
  MojitoSourceFlickrPrivate *priv = MOJITO_SOURCE_FLICKR (object)->priv;
  
  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }
  
  G_OBJECT_CLASS (mojito_source_flickr_parent_class)->dispose (object);
}

static void
mojito_source_flickr_finalize (GObject *object)
{
  MojitoSourceFlickrPrivate *priv = MOJITO_SOURCE_FLICKR (object)->priv;
  
  g_free (priv->user_id);
  
  G_OBJECT_CLASS (mojito_source_flickr_parent_class)->finalize (object);
}

static void
mojito_source_flickr_class_init (MojitoSourceFlickrClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MojitoSourceClass *source_class = MOJITO_SOURCE_CLASS (klass);
  
  g_type_class_add_private (klass, sizeof (MojitoSourceFlickrPrivate));
  
  object_class->dispose = mojito_source_flickr_dispose;
  object_class->finalize = mojito_source_flickr_finalize;
  
  source_class->get_name = mojito_source_flickr_get_name;
  source_class->update = update;
}

static void
mojito_source_flickr_init (MojitoSourceFlickr *self)
{
  self->priv = GET_PRIVATE (self);

  self->priv->proxy = rest_proxy_new ("http://api.flickr.com/services/rest/", FALSE);
  /* TODO: get from configuration file */
  self->priv->user_id = g_strdup ("35468147630@N01");
}
