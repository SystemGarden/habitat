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

#ifndef __GTK_HTIMELINE_H__
#define __GTK_HTIMELINE_H__


#include <gdk/gdk.h>
#include <gtk/gtkruler.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_HTIMELINE(obj)          GTK_CHECK_CAST (obj, gtk_htimeline_get_type (), GtkHTimeline)
#define GTK_HTIMELINE_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_htimeline_get_type (), GtkHTimelineClass)
#define GTK_IS_HTIMELINE(obj)       GTK_CHECK_TYPE (obj, gtk_htimeline_get_type ())


typedef struct _GtkHTimeline       GtkHTimeline;
typedef struct _GtkHTimelineClass  GtkHTimelineClass;

struct _GtkHTimeline
{
  GtkRuler ruler;
};

struct _GtkHTimelineClass
{
  GtkRulerClass parent_class;
};


guint      gtk_htimeline_get_type (void);
GtkWidget* gtk_htimeline_new      (void);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_HTIMELINE_H__ */
