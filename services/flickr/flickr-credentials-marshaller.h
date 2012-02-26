
#ifndef __flickr_credentials_MARSHAL_H__
#define __flickr_credentials_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* VOID:BOOLEAN (/dev/stdin:1) */
#define flickr_credentials_VOID__BOOLEAN	g_cclosure_marshal_VOID__BOOLEAN

/* VOID:BOOLEAN,POINTER (/dev/stdin:2) */
extern void flickr_credentials_VOID__BOOLEAN_POINTER (GClosure     *closure,
GValue       *return_value,
guint         n_param_values,
const GValue *param_values,
gpointer      invocation_hint,
gpointer      marshal_data);

G_END_DECLS

#endif /* __flickr_credentials_MARSHAL_H__ */


