/*
 * Gtk Databox time ruler, subclassed from Gtk Databox
 * Nigel Stuckey, March 2011. 
 * Copyright System Garden 2011
 */

#ifndef __GTK_DATABOX_TRULER_H__
#define __GTK_DATABOX_TRULER_H__

#include <glib-object.h>
#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gtkdatabox.h>

G_BEGIN_DECLS

#define GTK_DATABOX_TRULER_TYPE            (gtk_databox_truler_get_type ())
#define GTK_DATABOX_TRULER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_DATABOX_TRULER_TYPE, GtkDataboxTRuler))
#define GTK_DATABOX_TRULER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_DATABOX_TRULER_TYPE, GtkDataboxTRulerClass))
#define GTK_DATABOX_IS_TRULER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_DATABOX_TRULER_TYPE))
#define GTK_DATABOX_IS_TRULER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_DATABOX_TRULER_TYPE))
#define GTK_DATABOX_TRULER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_DATABOX_TRULER_TYPE, GtkDataboxTRulerClass))


typedef struct _GtkDataboxTRuler	GtkDataboxTRuler;
typedef struct _GtkDataboxTRulerClass	GtkDataboxTRulerClass;

typedef struct _GtkDataboxTRulerPrivate	GtkDataboxTRulerPrivate;

struct _GtkDataboxTRuler
{
   GtkDataboxRuler parent;
 
   GtkDataboxTRulerPrivate *priv;
};

struct _GtkDataboxTRulerClass
{
   GtkDataboxRulerClass parent_class;
};


typedef enum
{
   GTK_DATABOX_TSCALE_LINEAR = 0,
   GTK_DATABOX_TSCALE_LOG,
   GTK_DATABOX_TSCALE_LOG2,
   GTK_DATABOX_TSCALE_TIME
}
GtkDataboxTScaleType;

GtkWidget *gtk_databox_truler_new (GtkOrientation orientation);
void gtk_databox_truler_set_scale_type (GtkDataboxTRuler * ruler,
					GtkDataboxTScaleType scale_type);
GType gtk_databox_truler_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GTK_DATABOX_TRULER_H__ */
