/*
 * libsocialweb - social data store
 * Copyright (C) 2009 Intel Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <config.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <gio/gio.h>
#include "sw-keystore.h"

#if WITH_KEYUTILS
#include <keyutils.h>
#endif

typedef struct {
  char *key;
  char *secret;
} KeyData;

static void
key_data_free (gpointer data)
{
  KeyData *keydata = data;
  g_free (keydata->key);
  g_free (keydata->secret);
  g_free (keydata);
}

static void
load_keys_from_dir (GHashTable *hash, const char *base_dir, gboolean is_base)
{
  GError *error = NULL;
  char *directory;
  GFileEnumerator *fenum;
  GFile *dir, *file;
  GFileInfo *info;

  if (is_base) {
    directory = g_build_filename (base_dir, PACKAGE, "keys", NULL);
    dir = g_file_new_for_path (directory);
    g_free (directory);
  } else {
    dir = g_file_new_for_path (base_dir);
  }

  fenum = g_file_enumerate_children (dir, "standard::*",
                                     G_FILE_QUERY_INFO_NONE,
                                     NULL, &error);
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)) {
    g_error_free (error);
    goto done;
  }

  if (error) {
    g_message ("Cannot open directory: %s", error->message);
    g_error_free (error);
    goto done;
  }

  while ((info = g_file_enumerator_next_file (fenum, NULL, &error)) != NULL) {
    GFileInputStream *stream = NULL;
    GDataInputStream *dstream = NULL;
    const char *name;
    KeyData *data;

    if (g_file_info_get_file_type (info) != G_FILE_TYPE_REGULAR ||
        g_file_info_get_is_backup (info))
    {
      g_object_unref (info);
      continue;
    }

    name = g_file_info_get_name (info);
    file = g_file_get_child (dir, name);

    stream = g_file_read (file, NULL, &error);
    if (error)
    {
      g_object_unref (info);
      continue;
    }
    dstream = g_data_input_stream_new ((GInputStream *)stream);

    data = g_new0 (KeyData, 1);
    data->key = g_data_input_stream_read_line (dstream, NULL, NULL, NULL);
    if (data->key) {
      g_strstrip (data->key);
      if (data->key[0] == '\0') {
        free (data->key);
        data->key = NULL;
      }
    }

    data->secret = g_data_input_stream_read_line (dstream, NULL, NULL, NULL);
    if (data->secret) {
      g_strstrip (data->secret);
      if (data->secret[0] == '\0') {
          free (data->secret);
          data->secret = NULL;
        }
    }

    g_hash_table_insert (hash, g_strdup (name), data);

    if (dstream)
      g_object_unref (dstream);
    if (stream)
      g_object_unref (stream);
    g_object_unref (file);
    g_object_unref (info);
  }

 done:
  if (fenum)
    g_object_unref (fenum);
  g_object_unref (dir);
}

static gpointer
load_keys (gpointer data)
{
  GHashTable *hash;
#if ! BUILD_TESTS
  const char * const *dirs;
#endif

  hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, key_data_free);

#if ! BUILD_TESTS
  for (dirs = g_get_system_data_dirs (); *dirs; dirs++) {
    load_keys_from_dir (hash, *dirs, TRUE);
  }
  load_keys_from_dir (hash, g_get_user_config_dir (), TRUE);
#else
  load_keys_from_dir (hash, SOURCE_DIR "/test-keys", FALSE);
#endif

  return hash;
}

static GHashTable *
get_keys_hash (void)
{
  static GOnce once = G_ONCE_INIT;
  g_once (&once, load_keys, NULL);
  return once.retval;
}

#ifdef WITH_KEYUTILS

static gboolean
lookup_key_in_kernel (const char *service, const char **key, const char **secret)
{
  gchar *description;
  key_serial_t serial;
  KeyData *data;
  gchar *buffer = NULL;
  const gchar *end;
  const gchar *line;
  long len;

  description = g_strdup_printf ("libsocialweb:%s", service);
  serial = request_key ("user", description, service,
                        KEY_SPEC_SESSION_KEYRING);
  g_free (description);

  if (serial == -1) {
    g_warning (G_STRLOC ": couldn't lookup key in kernel: %s",
               g_strerror (errno));
    return FALSE;
 }

  len = keyctl_read_alloc (serial, (void**)&buffer);
  if (len == -1) {
    g_warning (G_STRLOC ": couldn't read key from kernel: %s",
               g_strerror (errno));
    return FALSE;
  }

  data = g_new0 (KeyData, 1);
  end = buffer + len;

  /* See if there are two values separated by a line break */
  line = memchr (buffer, '\n', len);
  if (line == NULL)
    line = end;

  /* If either line is empty, that part becomes NULL */
  if (line == buffer)
    data->key = NULL;
  else
    data->key = g_strndup (buffer, line - buffer);
  if (line != end) {
    line++;
    if (line == end)
      data->secret = NULL;
    else
      data->secret = g_strndup (line, end - line);
  }

  free (buffer);

  g_hash_table_replace (get_keys_hash (), g_strdup (service), data);
  *key = data->key;
  if (secret)
    *secret = data->secret;

  return TRUE;
}

#endif /* WITH_KEYUTILS */

gboolean
sw_keystore_get_key_secret (const char *service, const char **key, const char **secret)
{
  KeyData *data;

  g_return_val_if_fail (service, FALSE);
  g_return_val_if_fail (key, FALSE);
  /* secret can be NULL because some services don't have or need a secret */

  data = g_hash_table_lookup (get_keys_hash (), service);
  if (data) {
    *key = data->key;
    if (secret)
      *secret = data->secret;
    return TRUE;
  }

#if ! BUILD_TESTS
#ifdef WITH_KEYUTILS
  if (lookup_key_in_kernel (service, key, secret))
    return TRUE;
#endif
#endif

  *key = NULL;
  if (secret)
    *secret = NULL;
  return FALSE;
}

const char *
sw_keystore_get_key (const char *service)
{
  const char *key = NULL;

  g_return_val_if_fail (service, NULL);

  sw_keystore_get_key_secret (service, &key, NULL);

  return key;
}

#if BUILD_TESTS
static void
test_invalid (void)
{
  const char *key = NULL, *secret = NULL;
  gboolean ret;

  ret = sw_keystore_get_key_secret ("foobar", &key, &secret);
  g_assert (ret == FALSE);
  g_assert_cmpstr (key, ==, NULL);
  g_assert_cmpstr (secret, ==, NULL);

  key = sw_keystore_get_key ("foobar");
  g_assert_cmpstr (key, ==, NULL);
}

static void
test_key_secret (void)
{
  const char *key, *secret;
  gboolean ret;

  key = secret = NULL;
  ret = sw_keystore_get_key_secret ("flickr", &key, &secret);
  g_assert (ret == TRUE);
  g_assert_cmpstr (key, ==, "flickrkey");
  g_assert_cmpstr (secret, ==, "flickrsecret");

  key = secret = NULL;
  ret = sw_keystore_get_key_secret ("lastfm", &key, &secret);
  g_assert (ret == TRUE);
  g_assert_cmpstr (key, ==, "lastfmkey");
  g_assert_cmpstr (secret, ==, NULL);

  key = secret = NULL;
  ret = sw_keystore_get_key_secret ("twitter", &key, &secret);
  g_assert (ret == TRUE);
  g_assert_cmpstr (key, ==, "twitterkey");
  g_assert_cmpstr (secret, ==, "twittersecret");
}

static void
test_key (void)
{
  const char *key;

  key = sw_keystore_get_key ("flickr");
  g_assert_cmpstr (key, ==, "flickrkey");

  key = sw_keystore_get_key ("lastfm");
  g_assert_cmpstr (key, ==, "lastfmkey");

  key = sw_keystore_get_key ("twitter");
  g_assert_cmpstr (key, ==, "twitterkey");
}

int
main (int argc, char *argv[])
{
  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/keystore/invalid", test_invalid);
  g_test_add_func ("/keystore/key_secret", test_key_secret);
  g_test_add_func ("/keystore/key", test_key);

  return g_test_run ();
}
#endif
