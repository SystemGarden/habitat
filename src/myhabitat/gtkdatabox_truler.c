/*
 * Gtk Databox time ruler, subclassed from Gtk Databox
 * Nigel Stuckey, March 2011. 
 * Copyright System Garden 2011
 */

#include <math.h>
#include <gtkdatabox_ruler.h>
#include <gtkdatabox_scale.h>
#include <glib/gprintf.h>
#include "gtkdatabox_truler.h"
#include "../iiab/timeline.h"

#define ROUND(x) ((int) ((x) + 0.5))

/* private prototypes */
static void gtk_databox_truler_unrealize (GtkWidget * widget);
static gint gtk_databox_truler_expose (GtkWidget * widget, 
				       GdkEventExpose * event);
static void gtk_databox_truler_draw_ticks (GtkDataboxTRuler * truler);
static void gtk_databox_truler_draw_pos (GtkDataboxRuler * ruler);

/* private structures */
#define GTK_DATABOX_TRULER_GET_PRIVATE(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_DATABOX_TRULER_TYPE, GtkDataboxTRulerPrivate))

struct _GtkDataboxTRulerPrivate
{
  int is_time_scale;
};

struct _GtkDataboxRulerPrivate
{
   GdkPixmap *backing_pixmap;
   gint xsrc;
   gint ysrc;
   /* The lower limit of the ruler */
   gdouble lower;
   /* The upper limit of the ruler */
   gdouble upper;
   /* The position of the mark on the ruler */
   gdouble position;
   /* The maximum length of the labels (in characters) */
   guint max_length;
   /* The scale type of the ruler */
   GtkDataboxScaleType scale_type;
   /* Orientation of the ruler */
   GtkOrientation orientation;
};

G_DEFINE_TYPE (GtkDataboxTRuler, gtk_databox_truler, GTK_DATABOX_TYPE_RULER)


static void gtk_databox_truler_class_init (GtkDataboxTRulerClass * class)
{
   GtkWidgetClass *widget_class;
   GtkDataboxRulerClass  *ruler_class;

   widget_class = (GtkWidgetClass *) class;
   widget_class->expose_event = gtk_databox_truler_expose;
   widget_class->unrealize = gtk_databox_truler_unrealize;

   ruler_class = GTK_DATABOX_RULER_CLASS (class);
}

static void gtk_databox_truler_init (GtkDataboxTRuler * self)
{
  self->priv = g_new0 (GtkDataboxTRulerPrivate, 1);
  self->priv->is_time_scale = 0;
  GTK_DATABOX_RULER(self)->priv->max_length = 9;
}


GtkWidget *
gtk_databox_truler_new (GtkOrientation orientation)
{
   return g_object_new (GTK_DATABOX_TRULER_TYPE, "orientation", orientation,
			NULL);
}


static void
gtk_databox_truler_unrealize (GtkWidget * widget)
{
   GtkDataboxTRuler *truler = GTK_DATABOX_TRULER (widget);
   /*   GtkDataboxRuler  *ruler  = GTK_DATABOX_RULER (widget);*/

   g_free (truler->priv);

   if (GTK_WIDGET_CLASS (gtk_databox_truler_parent_class)->unrealize)
      (*GTK_WIDGET_CLASS (gtk_databox_truler_parent_class)->
       unrealize) (widget);

}



void
gtk_databox_truler_set_scale_type (GtkDataboxTRuler * truler,
				   GtkDataboxTScaleType scale_type)
{
   GtkDataboxRuler *ruler;

   g_return_if_fail (GTK_DATABOX_IS_TRULER (truler));

   ruler = GTK_DATABOX_RULER(truler);

   switch (scale_type) {
     case GTK_DATABOX_TSCALE_LINEAR:
       truler->priv->is_time_scale = 0;
       gtk_databox_ruler_set_scale_type(ruler, GTK_DATABOX_SCALE_LINEAR);
       break;
     case GTK_DATABOX_TSCALE_LOG:
       truler->priv->is_time_scale = 0;
       gtk_databox_ruler_set_scale_type(ruler, GTK_DATABOX_SCALE_LOG);
       break;
     case GTK_DATABOX_TSCALE_LOG2:
       truler->priv->is_time_scale = 0;
       gtk_databox_ruler_set_scale_type(ruler, GTK_DATABOX_SCALE_LOG);
       break;
     case GTK_DATABOX_TSCALE_TIME:
       truler->priv->is_time_scale = 1;
       gtk_databox_ruler_set_scale_type(ruler, GTK_DATABOX_SCALE_LINEAR);
       break;
     }
}


static gint
gtk_databox_truler_expose (GtkWidget * widget, GdkEventExpose * event)
{
   GtkDataboxTRuler *truler;
   GtkDataboxRuler *ruler;

   if (GTK_WIDGET_DRAWABLE (widget))
   {
      truler = GTK_DATABOX_TRULER (widget);
      ruler  = GTK_DATABOX_RULER  (widget);

      gtk_databox_truler_draw_ticks (truler);

      gdk_draw_drawable (widget->window,
			 widget->style->fg_gc[GTK_WIDGET_STATE (ruler)],
			 ruler->priv->backing_pixmap,
			 0, 0, 0, 0,
			 widget->allocation.width, widget->allocation.height);

      gtk_databox_truler_draw_pos (ruler);
   }

   return FALSE;
}


static void
gtk_databox_truler_draw_ticks (GtkDataboxTRuler * truler)
{
   GtkWidget *widget;
   cairo_t *cr;
   gint i;
   gint width, height;
   gint xthickness;
   gint ythickness;
   gint length;
   gdouble lower, upper;	/* Upper and lower limits */
   gdouble increment;		/* pixel per value unit */
   gint power;
   gint digit;
   gdouble subd_incr;
   gdouble start, end, cur;
   gchar unit_str[GTK_DATABOX_RULER_MAX_MAX_LENGTH + 1];	/* buffer for writing numbers */
   gint digit_width;
   gint digit_height;
   gint digit_offset;
   gint text_width;
   gint pos;
   gchar format_string[10];
   PangoMatrix matrix = PANGO_MATRIX_INIT;
   PangoContext *context;
   PangoLayout *layout;
   PangoRectangle logical_rect, ink_rect;
   GtkDataboxRuler *ruler;

   if (!GTK_WIDGET_DRAWABLE (truler))
      return;

   widget = GTK_WIDGET (truler);
   ruler  = GTK_DATABOX_RULER (truler);

   g_sprintf (format_string, "%%-%dg", ruler->priv->max_length - 1);

   xthickness = widget->style->xthickness;
   ythickness = widget->style->ythickness;

   /* work out the actual size of each digit */
   layout = gtk_widget_create_pango_layout (widget, "E+-012456789");
   pango_layout_get_pixel_extents (layout, &ink_rect, &logical_rect);

   digit_width = ceil ((logical_rect.width) / 12);
   digit_height = (logical_rect.height) + 2;
   digit_offset = ink_rect.y;

   if (ruler->priv->orientation == GTK_ORIENTATION_VERTICAL)
   {
      context = gtk_widget_get_pango_context (widget);
      pango_context_set_base_gravity (context, PANGO_GRAVITY_WEST);
      pango_matrix_rotate (&matrix, 90.);
      pango_context_set_matrix (context, &matrix);
   }

   width = widget->allocation.width;
   height = widget->allocation.height;

   gtk_paint_box (widget->style, ruler->priv->backing_pixmap,
		  GTK_STATE_NORMAL, GTK_SHADOW_OUT,
		  NULL, widget, "ruler", 0, 0, width, height);

   cr = gdk_cairo_create (ruler->priv->backing_pixmap);
   gdk_cairo_set_source_color (cr, &widget->style->fg[widget->state]);

   cairo_rectangle (cr,
		    xthickness,
		    height - ythickness, width - 2 * xthickness, 1);

   if (ruler->priv->scale_type == GTK_DATABOX_SCALE_LINEAR || 
       truler->priv->is_time_scale)
   {
      upper = ruler->priv->upper;
      lower = ruler->priv->lower;
   }
   else
   {
      if (ruler->priv->upper <= 0 || ruler->priv->lower <= 0)
      {
	 g_warning
	    ("For logarithmic scaling, the visible limits must by larger than 0!");
      }
      upper = log10 (ruler->priv->upper);
      lower = log10 (ruler->priv->lower);
   }

   if ((upper - lower) == 0)
      goto out;

   if (ruler->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
      increment = (gdouble) width / (upper - lower);
   else
      increment = (gdouble) height / (upper - lower);


   /* determine the scale, i.e. the distance between the most significant 
    * ticks; the ticks have to be farther apart than the length of the 
    * displayed numbers
    */
   if (ruler->priv->scale_type == GTK_DATABOX_SCALE_LINEAR)
   {
      text_width = (ruler->priv->max_length) * digit_width + 1;

      for (power = -20; power < 21; power++)
      {
	 if ((digit = 1) * pow (10, power) * fabs (increment) > text_width)
	    break;
	 if ((digit = 2.5) * pow (10, power) * fabs (increment) > text_width)
	    break;
	 if ((digit = 5) * pow (10, power) * fabs (increment) > text_width)
	    break;
      }


      if (power == 21)
      {
	 power = 20;
	 digit = 5;
      }
      subd_incr = digit * pow (10, power);
   }
   else
   {
      subd_incr = 1.;
   }

   length = (ruler->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
      ? height - 1 : width - 1;

   if (lower < upper)
   {
      start = floor (lower / subd_incr) * subd_incr;
      end = ceil (upper / subd_incr) * subd_incr;
   }
   else
   {
      start = floor (upper / subd_incr) * subd_incr;
      end = ceil (lower / subd_incr) * subd_incr;
   }


   if (truler->priv->is_time_scale) {
   }
   /* drawing unmodified linear and log scales */

   for (cur = start; cur <= end; cur += subd_incr)
   {
     pos = ROUND ((cur - lower) * increment);

     /* what line/box is drawing */
     if (ruler->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
       cairo_rectangle (cr, pos, height + ythickness - length, 1, length);
     else
       cairo_rectangle (cr, width + xthickness - length, pos, length, 1);


     /* compose label */
     if (truler->priv->is_time_scale)
       timeline_label(cur, upper-lower, (char *) unit_str, ruler->priv->max_length + 1);
     else if (ruler->priv->scale_type == GTK_DATABOX_SCALE_LINEAR)
     {
       if (ABS (cur) < 0.1 * subd_incr)	/* Rounding errors occur and 
					 * might make "0" look funny 
					 * without this check */
	 cur = 0;
       
       g_snprintf (unit_str, ruler->priv->max_length + 1, format_string, cur);
     }
     else
       g_snprintf (unit_str, ruler->priv->max_length + 1, format_string,
		   pow (10, cur));

     /* layout label and find its width */
     pango_layout_set_text (layout, unit_str, -1);
     pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

     /* paint the label layout */
     if (ruler->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
       gtk_paint_layout (widget->style,
			 ruler->priv->backing_pixmap,
			 GTK_WIDGET_STATE (widget),
			 FALSE,
			 NULL,
			 widget, "ruler", pos + 2, ythickness - 1, layout);
     else
       gtk_paint_layout (widget->style,
			 ruler->priv->backing_pixmap,
			 GTK_WIDGET_STATE (widget),
			 FALSE,
			 NULL,
			 widget,
			 "ruler",
			 xthickness - 1,
			 pos - logical_rect.width - 2, layout);
 
     /* Draw sub-ticks */
     if (truler->priv->is_time_scale)
       /* time subdivisions */
       for (i = 1; i < 6; ++i)
       {
	 /* mark in 1/6ths */
	 pos = ROUND ((cur - lower + subd_incr / 6 * i) * increment);

	 if (ruler->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
	   cairo_rectangle (cr,
			    pos, height + ythickness - length / 2,
			    1, length / 2);
	 else
	   cairo_rectangle (cr,
			    width + xthickness - length / 2, pos,
			    length / 2, 1);
       }
     else  if (ruler->priv->scale_type == GTK_DATABOX_SCALE_LINEAR)
       /* linear subdivisions */
       for (i = 1; i < 5; ++i)
       {
	 /* mark in 1/5ths */
	 pos = ROUND ((cur - lower + subd_incr / 5 * i) * increment);

	 if (ruler->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
	   cairo_rectangle (cr,
			    pos, height + ythickness - length / 2,
			    1, length / 2);
	 else
	   cairo_rectangle (cr,
			    width + xthickness - length / 2, pos,
			    length / 2, 1);
       }
     else
       /* logrithmic subdivisions */
       for (i = 2; i < 10; ++i)
	 {
	   /* mark in 1/10ths */
	   pos = ROUND ((cur - lower + log10 (i)) * increment);

	   if (ruler->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
	     cairo_rectangle (cr,
			      pos, height + ythickness - length / 2,
			      1, length / 2);
	   else
	     cairo_rectangle (cr,
			      width + xthickness - length / 2, pos,
			      length / 2, 1);
	 }

   }

   cairo_fill (cr);
 out:
   cairo_destroy (cr);
   
   g_object_unref (layout);
}


static void
gtk_databox_truler_draw_pos (GtkDataboxRuler * ruler)
{
   GtkWidget *widget = GTK_WIDGET (ruler);
   gint x, y;
   gint width, height;
   gint bs_width, bs_height;
   gint xthickness;
   gint ythickness;
   gdouble increment;

   if (GTK_WIDGET_DRAWABLE (ruler))
   {
      xthickness = widget->style->xthickness;
      ythickness = widget->style->ythickness;
      width = widget->allocation.width - xthickness * 2;
      height = widget->allocation.height - ythickness * 2;

      if (ruler->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
      {
	 bs_width = height / 2 + 2;
	 bs_width |= 1;		/* make sure it's odd */
	 bs_height = bs_width / 2 + 1;
      }
      else
      {
	 bs_height = width / 2 + 2;
	 bs_height |= 1;	/* make sure it's odd */
	 bs_width = bs_height / 2 + 1;
      }

      if ((bs_width > 0) && (bs_height > 0))
      {
	 cairo_t *cr = gdk_cairo_create (widget->window);

	 /*  If a backing store exists, restore the ruler  */
	 if (ruler->priv->backing_pixmap)
	    gdk_draw_drawable (widget->window,
			       widget->style->black_gc,
			       ruler->priv->backing_pixmap,
			       ruler->priv->xsrc, ruler->priv->ysrc,
			       ruler->priv->xsrc, ruler->priv->ysrc, bs_width, bs_height);

	 if (ruler->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
	 {
	    increment = (gdouble) width / (ruler->priv->upper - ruler->priv->lower);

	    x = ROUND ((ruler->priv->position - ruler->priv->lower) * increment) +
	       (xthickness - bs_width) / 2 - 1;
	    y = (height + bs_height) / 2 + ythickness;

	    gdk_cairo_set_source_color (cr,
					&widget->style->fg[widget->state]);

	    cairo_move_to (cr, x, y);
	    cairo_line_to (cr, x + bs_width / 2., y + bs_height);
	    cairo_line_to (cr, x + bs_width, y);
	 }
	 else
	 {
	    increment = (gdouble) height / (ruler->priv->upper - ruler->priv->lower);

	    x = (width + bs_width) / 2 + xthickness;
	    y = ROUND ((ruler->priv->position - ruler->priv->lower) * increment) +
	       (ythickness - bs_height) / 2 - 1;

	    gdk_cairo_set_source_color (cr,
					&widget->style->fg[widget->state]);

	    cairo_move_to (cr, x, y);
	    cairo_line_to (cr, x + bs_width, y + bs_height / 2.);
	    cairo_line_to (cr, x, y + bs_height);
	 }
	 cairo_fill (cr);

	 cairo_destroy (cr);

	 ruler->priv->xsrc = x;
	 ruler->priv->ysrc = y;
      }
   }
}

