/*
 * Habitat GUI printing code
 *
 * Nigel Stuckey, January 2011
 * Copyright System Garden Ltd 2011. All rights reserved
 */

#include <gtk/gtk.h>
#include "uiprint.h"
#include "main.h"

/* callback to print */
static GtkPrintSettings *uiprint_settings = NULL;
G_MODULE_EXPORT void 
uiprint_on_print (GtkObject *object, gpointer user_data)
{
     GtkPrintOperation *print;
     GtkPrintOperationResult res;

     print = gtk_print_operation_new ();
     if (uiprint_settings != NULL)
          gtk_print_operation_set_print_settings (print, uiprint_settings);
     g_signal_connect (print, "begin_print", G_CALLBACK (uiprint_begin_print), 
		       NULL);
     g_signal_connect (print, "draw_page", G_CALLBACK (uiprint_draw_page), 
		       NULL);

     res = gtk_print_operation_run (print, 
				    GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
				    NULL /*GTK_WINDOW (main_window)*/, NULL);
     if (res == GTK_PRINT_OPERATION_RESULT_APPLY) {
          if (uiprint_settings != NULL)
	       g_object_unref (uiprint_settings);
	  uiprint_settings = g_object_ref (gtk_print_operation_get_print_settings (print));
     }

     g_object_unref (print);
}


void uiprint_begin_print(GtkPrintOperation *operation,
			 GtkPrintContext   *context,
			 gpointer           user_data)
{
}


#define HEADER_HEIGHT 10

/* print a page */
void uiprint_draw_page(GtkPrintOperation *operation,
		       GtkPrintContext   *context,
		       gint               page_nr,
		       gpointer           user_data)
{
     cairo_t *cr;
     gdouble scale;
     GdkPixbuf *pixbuf;
     GtkWidget *w;

     cr = gtk_print_context_get_cairo_context (context);

     w = get_widget("visualisation_notebook");
     
#if 0
     /* get the window from the visualisation mode */
     switch (uivis_vis_mode) {
     case UIVIS_TEXT:
          break;
     case UIVIS_HTML:
       w = get_widget("");
          break;
     case UIVIS_TABLE:
       w = get_widget("");
	  break;
     case UIVIS_CHART:
          break;
     case UIVIS_INFO:
     case UIVIS_SPLASH:
     case UIVIS_WHATNEXT:
     default:
	  break;
     }
#endif

     /* make a screenshot of visualisation */
     pixbuf = gdk_pixbuf_get_from_drawable(NULL, w->window, NULL,
                               0, 0, 0, 0,
                               w->allocation.width,
                               w->allocation.height);

     /* calculate scale factor */
     scale = gtk_print_context_get_width (context) / w->allocation.width;
  
     /* scale, keep aspect ratio */
     cairo_scale(cr, scale, scale);

     gdk_cairo_set_source_pixbuf(cr, pixbuf, 0.0, 0.0);

     cairo_paint(cr);

#if 0
     PangoLayout *layout;
     gdouble scale, text_height;
     gint layout_height;
     PangoFontDescription *desc;

     cairo_rectangle (cr, 0, 0, width, HEADER_HEIGHT);
  
     cairo_set_source_rgb (cr, 0.8, 0.8, 0.8);
     cairo_fill (cr);
  
     layout = gtk_print_context_create_pango_layout (context);
  
     desc = pango_font_description_from_string ("sans 14");
     pango_layout_set_font_description (layout, desc);
     pango_font_description_free (desc);
  
     pango_layout_set_text (layout, "some text", -1);
     pango_layout_set_width (layout, width * PANGO_SCALE);
     pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
     		      
     pango_layout_get_size (layout, NULL, &layout_height);
     text_height = (gdouble)layout_height / PANGO_SCALE;
  
     cairo_move_to (cr, width / 2,  (HEADER_HEIGHT - text_height) / 2);
     pango_cairo_show_layout (cr, layout);
  
     g_object_unref (layout);
#endif

}
