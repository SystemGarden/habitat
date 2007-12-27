/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

/*
 * gtkhtimeline.c created by Nigel Stuckey from gtk code under LGPL.
 * Copyright System Garden 2000-2001. All rights reserved.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "gtkhtimeline.h"
#include "../iiab/itree.h"
#include "../iiab/timeline.h"


#define RULER_HEIGHT          14
#define MINIMUM_INCR          5
#define MAXIMUM_SUBDIVIDE     5
#define MAXIMUM_SCALES        10

#define ROUND(x) ((int) ((x) + 0.5))


static void gtk_htimeline_class_init    (GtkHTimelineClass *klass);
static void gtk_htimeline_init          (GtkHTimeline      *htimeline);
static gint gtk_htimeline_motion_notify (GtkWidget      *widget,
				      GdkEventMotion *event);
static void gtk_htimeline_draw_ticks    (GtkRuler       *ruler);
static void gtk_htimeline_draw_pos      (GtkRuler       *ruler);


guint
gtk_htimeline_get_type (void)
{
  static guint htimeline_type = 0;

  if (!htimeline_type)
    {
      static const GtkTypeInfo htimeline_info =
      {
	"GtkHTimeline",
	sizeof (GtkHTimeline),
	sizeof (GtkHTimelineClass),
	(GtkClassInitFunc) gtk_htimeline_class_init,
	(GtkObjectInitFunc) gtk_htimeline_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      htimeline_type = gtk_type_unique (gtk_ruler_get_type (), &htimeline_info);
    }

  return htimeline_type;
}

static void
gtk_htimeline_class_init (GtkHTimelineClass *klass)
{
  GtkWidgetClass *widget_class;
  GtkRulerClass *ruler_class;

  widget_class = (GtkWidgetClass*) klass;
  ruler_class = (GtkRulerClass*) klass;

  widget_class->motion_notify_event = gtk_htimeline_motion_notify;

  ruler_class->draw_ticks = gtk_htimeline_draw_ticks;
  ruler_class->draw_pos = gtk_htimeline_draw_pos;
}

static void
gtk_htimeline_init (GtkHTimeline *htimeline)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (htimeline);
  widget->requisition.width = widget->style->klass->xthickness * 2 + 1;
  widget->requisition.height = widget->style->klass->ythickness * 2 + RULER_HEIGHT;
}


GtkWidget*
gtk_htimeline_new (void)
{
  return GTK_WIDGET (gtk_type_new (gtk_htimeline_get_type ()));
}

static gint
gtk_htimeline_motion_notify (GtkWidget      *widget,
			  GdkEventMotion *event)
{
  GtkRuler *ruler;
  gint x;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_HTIMELINE (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  ruler = GTK_RULER (widget);

  if (event->is_hint)
    gdk_window_get_pointer (widget->window, &x, NULL, NULL);
  else
    x = event->x;

  ruler->position = ruler->lower + ((ruler->upper - ruler->lower) * x) / widget->allocation.width;

  /*  Make sure the ruler has been allocated already  */
  if (ruler->backing_store != NULL)
    gtk_ruler_draw_pos (ruler);

  return FALSE;
}

static void
gtk_htimeline_draw_ticks (GtkRuler *ruler)
{
  GtkWidget *widget;
  GdkGC *gc, *bg_gc;
  GdkFont *font;
  gint width, height;
  gint xthickness;
  gint ythickness;
  gint length=0;
  gfloat lower, upper;		/* Upper and lower limits, in ruler units */
  gfloat increment;		/* Number of pixels per unit */
  gint digit_height;
  gint pos;
  ITREE *ticklist;
  struct timeline_tick *tick;

  g_return_if_fail (ruler != NULL);
  g_return_if_fail (GTK_IS_HTIMELINE (ruler));

  if (!GTK_WIDGET_DRAWABLE (ruler)) 
    return;

  widget = GTK_WIDGET (ruler);

  gc = widget->style->fg_gc[GTK_STATE_NORMAL];
  bg_gc = widget->style->bg_gc[GTK_STATE_NORMAL];
  font = widget->style->font;

  xthickness = widget->style->klass->xthickness;
  ythickness = widget->style->klass->ythickness;
  digit_height = font->ascent; /* assume descent == 0 ? */

  width = widget->allocation.width;
  height = widget->allocation.height - ythickness * 2;

   
   gtk_paint_box (widget->style, ruler->backing_store,
		  GTK_STATE_NORMAL, GTK_SHADOW_OUT, 
		  NULL, widget, "htimeline",
		  0, 0, 
		  widget->allocation.width, widget->allocation.height);


   gdk_draw_line (ruler->backing_store, gc,
		 xthickness,
		 height + ythickness,
		 widget->allocation.width - xthickness,
		 height + ythickness);

   /* highest and lowest points on the scale */
  upper = ruler->upper / ruler->metric->pixels_per_unit;
  lower = ruler->lower / ruler->metric->pixels_per_unit;

  if ((upper - lower) == 0) 
    return;
  increment = (gfloat) width / (upper - lower);

#if 1
  /* get the timeline */
  ticklist = timeline_calc(/*0*/lower, upper, upper - lower);
  if ( ! ticklist )
       return;

  /* draw the list on the chart */
  itree_traverse(ticklist) {
       tick = itree_get(ticklist);
       pos = ((int) itree_getkey(ticklist) - lower) * increment;
       switch (tick->type) {
       case TIMELINE_MAJOR:
	    length = height / 2;
	    break;
       case TIMELINE_MINOR:
	    length = height / 4;
	    break;
       case TIMELINE_NONE:
	    length = 0;
	    break;
       }

       if (length)
	    gdk_draw_line (ruler->backing_store, gc,
			   pos, height + ythickness, 
			   pos, height - length + ythickness);
       if (tick->label)
	    gdk_draw_string (ruler->backing_store, font, gc,
			     pos + 2, ythickness + font->ascent - 1,
			     tick->label);
  }

  timeline_free(ticklist);

#else

  /* determine the scale
   *  We calculate the text size as for the vruler instead of using
   *  text_width = gdk_string_width(font, unit_str), so that the result
   *  for the scale looks consistent with an accompanying vruler
   */
  /* this works out the width of a text string */
  scale = ceil (ruler->max_size / ruler->metric->pixels_per_unit);
  sprintf (unit_str, "%d", scale);
  text_width = strlen (unit_str) * digit_height + 1;

  /* this works out the scale to use (a modulo affair) based on the width
   * of the text */
  for (scale = 0; scale < MAXIMUM_SCALES; scale++)
    if (ruler->metric->ruler_scale[scale] * fabs(increment) > 2 * text_width)
      break;

  if (scale == MAXIMUM_SCALES)
    scale = MAXIMUM_SCALES - 1;

  /* drawing starts here */
  length = 0;
  for (i = MAXIMUM_SUBDIVIDE - 1; i >= 0; i--)
    {
      subd_incr = (gfloat) ruler->metric->ruler_scale[scale] / 
	          (gfloat) ruler->metric->subdivide[i];
      if (subd_incr * fabs(increment) <= MINIMUM_INCR) 
	continue;

      /* Calculate the length of the tickmarks. Make sure that
       * this length increases for each set of ticks
       */
      ideal_length = height / (i + 1) - 1;
      if (ideal_length > ++length)
	length = ideal_length;

      if (lower < upper)
	{
	  start = floor (lower / subd_incr) * subd_incr;
	  end   = ceil  (upper / subd_incr) * subd_incr;
	}
      else
	{
	  start = floor (upper / subd_incr) * subd_incr;
	  end   = ceil  (lower / subd_incr) * subd_incr;
	}

  
      for (cur = start; cur <= end; cur += subd_incr)
	{
	  pos = ROUND ((cur - lower) * increment);

	  gdk_draw_line (ruler->backing_store, gc,
			 pos, height + ythickness, 
			 pos, height - length + ythickness);

	  /* draw label */
	  if (i == 0)
	    {
	      sprintf (unit_str, "%d", (int) cur);
	      gdk_draw_string (ruler->backing_store, font, gc,
			       pos + 2, ythickness + font->ascent - 1,
			       unit_str);
	    }
	}
    }
#endif
}

static void
gtk_htimeline_draw_pos (GtkRuler *ruler)
{
  GtkWidget *widget;
  GdkGC *gc;
  int i;
  gint x, y;
  gint width, height;
  gint bs_width, bs_height;
  gint xthickness;
  gint ythickness;
  gfloat increment;

  g_return_if_fail (ruler != NULL);
  g_return_if_fail (GTK_IS_HTIMELINE (ruler));

  if (GTK_WIDGET_DRAWABLE (ruler))
    {
      widget = GTK_WIDGET (ruler);

      gc = widget->style->fg_gc[GTK_STATE_NORMAL];
      xthickness = widget->style->klass->xthickness;
      ythickness = widget->style->klass->ythickness;
      width = widget->allocation.width;
      height = widget->allocation.height - ythickness * 2;

      bs_width = height / 2;
      bs_width |= 1;  /* make sure it's odd */
      bs_height = bs_width / 2 + 1;

      if ((bs_width > 0) && (bs_height > 0))
	{
	  /*  If a backing store exists, restore the ruler  */
	  if (ruler->backing_store && ruler->non_gr_exp_gc)
	    gdk_draw_pixmap (ruler->widget.window,
			     ruler->non_gr_exp_gc,
			     ruler->backing_store,
			     ruler->xsrc, ruler->ysrc,
			     ruler->xsrc, ruler->ysrc,
			     bs_width, bs_height);

	  increment = (gfloat) width / (ruler->upper - ruler->lower);

	  x = ROUND ((ruler->position - ruler->lower) * increment) + (xthickness - bs_width) / 2 - 1;
	  y = (height + bs_height) / 2 + ythickness;

	  for (i = 0; i < bs_height; i++)
	    gdk_draw_line (widget->window, gc,
			   x + i, y + i,
			   x + bs_width - 1 - i, y + i);


	  ruler->xsrc = x;
	  ruler->ysrc = y;
	}
    }
}
