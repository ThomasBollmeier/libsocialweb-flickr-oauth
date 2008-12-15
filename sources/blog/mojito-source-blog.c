#include "mojito-source-blog.h"
#include "mojito-web.h"
#include "mojito-utils.h"
#include <rss-glib/rss-glib.h>
#include <libsoup/soup.h>
#include "generic.h"

G_DEFINE_TYPE (MojitoSourceBlog, mojito_source_blog, MOJITO_TYPE_SOURCE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SOURCE_BLOG, MojitoSourceBlogPrivate))

struct _MojitoSourceBlogPrivate {
  MojitoCore *core; /* TODO: move to MojitoSource */
  RssParser *parser;
  SoupURI *uri;
};

/* Need a table of blogs (url, title) which is both the configuration (url) and
   metadata for display (url->title). Then store the rowid into the blogposts
   table instead of source url */
static const char sql_create[] =
  "CREATE TABLE IF NOT EXISTS blogposts ("
  "'source' TEXT NOT NULL," /* source url */
  "'uuid' TEXT NOT NULL," /* item id */
  /* TODO: time_t or iso8901 string? */
  "'date' INTEGER NOT NULL," /* post date */
  "'link' TEXT NOT NULL," /* post link */
  "'title' TEXT" /* post title */
  ");";
static const char sql_add_item[] = "INSERT INTO "
  "blogposts(source, uuid, date, link, title) "
  "VALUES (:source, :uuid, :date, :link, :title);";
static const char sql_delete_items[] = "DELETE FROM blogposts WHERE source=:source;";

static GList *
mojito_source_blog_initialize (MojitoSourceClass *source_class, MojitoCore *core)
{
  sqlite3 *db;
  MojitoSourceBlog *source;

  db = mojito_core_get_db (core);
  if (!mojito_create_tables (db, sql_create)) {
    g_printerr ("Cannot create tables for blog source: %s\n", sqlite3_errmsg (db));
    return NULL;
  }

  /* TODO: replace with configuration file */
  source = g_object_new (MOJITO_TYPE_SOURCE_BLOG, NULL);
  GET_PRIVATE (source)->core = core;
  GET_PRIVATE (source)->uri = soup_uri_new ("http://planet.gnome.org/atom.xml");

  return g_list_prepend (NULL, source);
}

static void
mojito_source_blog_start (MojitoSource *source)
{
}

#if 0
static void
add_item (MojitoCore *core, const char *source_id, const char *item_id, time_t date, const char *link, const char *title)
{
  sqlite3 *db;
  sqlite3_stmt *statement;

  g_return_if_fail (MOJITO_IS_CORE (core));
  g_return_if_fail (source_id);
  g_return_if_fail (item_id);

  db = mojito_core_get_db (core);
  statement = db_generic_prepare_and_bind (db, sql_add_item,
                                           ":source", BIND_TEXT, source_id,
                                           ":uuid", BIND_TEXT, item_id,
                                           ":date", BIND_INT, date,
                                           ":link", BIND_TEXT, link,
                                           ":title", BIND_TEXT, title,
                                           NULL);
  if (db_generic_exec (statement, TRUE) != SQLITE_OK) {
    g_printerr ("cannot add item\n");
  }
}


static void
remove_items (MojitoCore *core, const char *source_id)
{
  sqlite3_stmt *statement;
  sqlite3 *db;

  g_return_if_fail (MOJITO_IS_CORE (core));
  g_return_if_fail (source_id);

  db = mojito_core_get_db (core);
  statement = db_generic_prepare_and_bind (db, sql_delete_items,
                                           ":source", BIND_TEXT, source_id, NULL);
  if (db_generic_exec (statement, TRUE) != SQLITE_OK) {
    g_printerr ("cannot remove items\n");
  }
}

static void
mojito_source_blog_update (MojitoSource *source)
{
  MojitoSourceBlog *blog = MOJITO_SOURCE_BLOG (source);
  MojitoSourceBlogPrivate *priv = blog->priv;
  SoupMessage *msg;
  guint status;
  char *blog_url;

  g_debug ("Updating %s%s", blog->priv->uri->host, blog->priv->uri->path);

  msg = soup_message_new_from_uri (SOUP_METHOD_GET, blog->priv->uri);

  status = mojito_web_cached_send (blog->priv->core, msg);

  if (SOUP_STATUS_IS_SUCCESSFUL (status)) {
    GError *error = NULL;
    RssDocument *document;
    GList *items, *l;

    if (!rss_parser_load_from_data (priv->parser,
                                    msg->response_body->data,
                                    msg->response_body->length,
                                    &error)) {
      g_printerr ("Cannot parse feed: %s\n", error->message);
      g_error_free (error);
      goto done;
    }

    blog_url = soup_uri_to_string (priv->uri, FALSE);

    remove_items (priv->core, blog_url);

    document = rss_parser_get_document (priv->parser);
    items = rss_document_get_items (document);
    for (l = items; l; l = l->next) {
      RssItem *item = l->data;
      char *guid, *link, *date_s, *title;
      time_t date;

      g_object_get (item,
                    "guid", &guid,
                    "link", &link,
                    "pub-date", &date_s,
                    "title", &title,
                    NULL);
      date = mojito_time_t_from_string (date_s);

      add_item (priv->core, blog_url, guid, date, link, title);

      g_free (guid);
      g_free (link);
      g_free (date_s);
      g_free (title);
    }
    g_list_free (items);
    g_object_unref (document);
    g_free (blog_url);
  }

 done:
  g_object_unref (msg);
}
#endif

static void
mojito_source_blog_dispose (GObject *object)
{
  MojitoSourceBlogPrivate *priv = MOJITO_SOURCE_BLOG (object)->priv;

  /* ->core is a weak reference so we don't unref it here */

  if (priv->parser) {
    g_object_unref (priv->parser);
    priv->parser = NULL;
  }

  G_OBJECT_CLASS (mojito_source_blog_parent_class)->dispose (object);
}

static void
mojito_source_blog_class_init (MojitoSourceBlogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MojitoSourceClass *source_class = MOJITO_SOURCE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MojitoSourceBlogPrivate));

  object_class->dispose = mojito_source_blog_dispose;

  source_class->initialize = mojito_source_blog_initialize;
  source_class->start = mojito_source_blog_start;
}

static void
mojito_source_blog_init (MojitoSourceBlog *self)
{
  self->priv = GET_PRIVATE (self);

  self->priv->parser = rss_parser_new ();
}

MojitoSource *
mojito_source_blog_new (MojitoCore *core)
{
  MojitoSourceBlog *source = g_object_new (MOJITO_TYPE_SOURCE_BLOG, NULL);
  GET_PRIVATE (source)->core = core;
  return MOJITO_SOURCE (source);
}
