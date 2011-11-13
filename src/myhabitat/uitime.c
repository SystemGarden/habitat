/*
 * Habitat GUI time widgets
 *
 * Nigel Stuckey, August 2010
 * Copyright System Garden Ltd 2010. All rights reserved
 */
#include <time.h>
#include "../iiab/util.h"
#include "../iiab/nmalloc.h"
#include "../iiab/elog.h"
#include "../iiab/table.h"
#include "uitime.h"
#include "uichoice.h"
#include "main.h"
#include "uilog.h"
#include "rcache.h"
#include "fileroute.h"
#include "uivis.h"
#include "uidata.h"

/* flag affecting the reload and redraw of data in the slider signal handler */
int uitime_prevent_reload=1;

/* possible data available, remembered from the previous call */
time_t uitime_avail_oldest=0, uitime_avail_youngest=0;

/* current viewed range */
time_t uitime_view_oldest=0, uitime_view_youngest=0;

/* link to the current route from uidata */
extern gchar *uidata_ringpurl;

/* link to the current dynamic data function (if no route) from uidata */
extern TABLE (*uidata_ringdatacb)(time_t, time_t);

/* data type of the current route from uidata */
extern FILEROUTE_TYPE uidata_type;


/* Clear the remembered data in uitime, used when starting new rings */
void uitime_forget_data()
{
     uitime_avail_oldest   = 0;
     uitime_avail_youngest = 0;
     uitime_view_oldest    = 0;
     uitime_view_youngest  = 0;
}


/*
 * Set the time slider to run from from_t to to_t (in Unix time_t)
 * representing potential data and set the slider position to show 
 * the most recent openage_t seconds, representing the start of 
 * visualised data.
 * Openage_t has a reasonable default if passed -1 : it reuses the 
 * previous oldest view if set, or one day if not.
 * Available and viewed data is remembered as a global and triggers ring
 * data loading and redrawing via the slider's signal (via the Gtk 
 * vlaue-changed signal which eventually calls uitime_slider_change() ).
 */
void uitime_set_slider(time_t from_t, time_t to_t, time_t openage_t)
{
     GtkWidget *from_w, *to_w, *current_w, *slider_w;
     time_t current_t, previous_t;
     char *str;

     /*fprintf(stderr, "uitime_set_slider() - from_t=%ld to_t=%ld "
	     "openage_t=%ld\n",
	     from_t, to_t, openage_t);*/

     /* Time should contain something in order to display.
      * For now, don't change the existing display but tell the user there
      * is not data to display */
     if (to_t == 0) {
          elog_printf(INFO, "No data to display, leaving old display");
	  return;
     }

     /* Just plain wrong! the dates are reversed */
     if (from_t > to_t) {
          elog_printf(INFO, "Crazy mixed up data dates, leaving old display");
	  return;
     }

     /* Single data elements dont make sense when graphing */
     if (from_t == to_t) {
       /* log here is too chatty */
       /*elog_printf(INFO, "Only one sample, can't draw chart");*/
     }

     /* gather the widgets */
     from_w    = get_widget("view_timescale_min");
     to_w      = get_widget("view_timescale_max");
     current_w = get_widget("view_timescale_current");
     slider_w  = get_widget("view_timescale_slide");

     /* set the min and max times */
     str = util_shortadaptdatetime(from_t);
     /*g_print("uitime_set_slider() min str=%s\n", str);*/
     gtk_label_set_text(GTK_LABEL(from_w), str);
     str = util_shortadaptdatetime(to_t);
     /*g_print("uitime_set_slider() max str=%s\n", str);*/
     gtk_label_set_text(GTK_LABEL(to_w), str);

     /* set the current time, which is the minimum data viewed */
     if (openage_t != -1) {
          if (to_t - openage_t < from_t)
	       current_t = from_t;	/* no further back than the oldest */
	  else
	       current_t = to_t - openage_t;	/* slider inside range */
     } else {
          /* -1 passed to look for default or reuse of previous setting */
          if ( uitime_view_oldest && uitime_avail_oldest && 
	       uitime_view_oldest >= uitime_avail_oldest &&
	       uitime_avail_youngest &&
	       uitime_view_oldest <  uitime_avail_youngest ) {

	       /* reuse the previous slider date as its within the 
		* available data range */
	       current_t = uitime_view_oldest;

	  } else {

	       /* We can't use the last date, so use a defaut */
	       if (to_t - UITIME_INITIAL_RANGE < from_t)
		    /* less than initial range avail */
		    current_t = from_t;
	       else
		    /* set to initial range */
		    current_t = to_t - UITIME_INITIAL_RANGE;
	  }
     }
#if 0
     char *diffstr = util_approxtimedist(current_t, to_t);
     str = util_strjoin(util_decdatetime(current_t), " ", diffstr, NULL);;
     g_print("current str=%s (%lu-%lu)\n", str, current_t, to_t);
     gtk_label_set_text(GTK_LABEL(current_w), str);
     nfree(diffstr);
#endif

     /* Set the slider with min and max limits, but dont allow to redraw */
     previous_t = gtk_range_get_value(GTK_RANGE(slider_w));
     g_signal_handlers_block_by_func(G_OBJECT(slider_w), 
				     G_CALLBACK(uitime_on_slider_value_changed),
				     NULL);
     if (from_t != to_t)
          gtk_range_set_range(GTK_RANGE(slider_w), (float)from_t, (float)to_t);
     gtk_range_set_value(GTK_RANGE(slider_w), current_t);
     g_signal_handlers_unblock_by_func(G_OBJECT(slider_w), 
				     G_CALLBACK(uitime_on_slider_value_changed),
				     NULL);

     /* Record change for file/class scope, also used for redrawing */
     uitime_avail_oldest = from_t;
     uitime_avail_youngest = to_t;

#if 0
     g_print("uitime_set_slider() - setting GtkRange [%lu..(%lu)..%lu], "
	     "prev=%lu, setting value\n", 
	     from_t, current_t, to_t, previous_t);
#endif

     /* redraw */
     uitime_slider_change(current_t, to_t);
}


/* set the view within an already established slider */
void uitime_set_slider_view(time_t from_t, time_t to_t)
{
     /* check that the slider has been set up */
     if ( ! (uitime_avail_oldest && uitime_avail_youngest) )
          return;

     /* below is a hack that basically ignores the to date */
     /* TBD: make a better version of uitme_set_slider that takes for args */
     uitime_set_slider(uitime_avail_oldest, uitime_avail_youngest,
		       time(NULL) - from_t);
}


/* callback to format the slider value string when the slider is moved */
G_MODULE_EXPORT gchar* 
uitime_on_slider_format_value (GtkScale *scale, gdouble value)
{
#if 0
     /* simple return */
     return g_strdup(util_shortadaptreldatetime((time_t) value,
						uitime_avail_youngest));
#endif

     char *str, *diffstr;

     /* complex return - simple + diff */
     if (value > 0 && uitime_avail_youngest > 0) {
          diffstr = util_approxtimedist((time_t) value, uitime_avail_youngest);
	  str = g_strdup_printf("%s for %s", 
				util_shortadaptreldatetime((time_t) value,
							uitime_avail_youngest),
				diffstr);
	  nfree(diffstr);
     } else {
          /* slider not set up or ring info not yet known */
          str = g_strdup("-");
     }

#if 0
     /* debug slider values */
     g_print("current str=%s (%lu-%lu)\n", str, (time_t) value,
	     uitime_avail_youngest);
#endif

     return str;
}


/* callback for slider value change, used to trigger a data fetch and redraw
 * if changed */
G_MODULE_EXPORT void
uitime_on_slider_value_changed (GtkRange      *slider,
				gpointer      user_data)
{
     time_t slider_t;

     /* get the value from the slider */
     slider_t = gtk_range_get_value(slider);

#if 0
     g_print("uitime_on_slider_value_changed() to %lu (%s)\n", slider_t,
	     util_decdatetime(slider_t));
#endif

     uitime_slider_change(slider_t, uitime_avail_youngest);
}

/* Functions below are a simple toggle to stop uitime_slider_change() 
 * loading new data into cache or redrawing. Used to stop lots of unwanted 
 * redraws */
void uitime_prevent_slider_reload() { uitime_prevent_reload = 1; }
void uitime_allow_slider_reload()   { uitime_prevent_reload = 0; }

/* 
 * Load and draw the currently active ring between two times.
 * Time range is (slider_from..slider_to) which is requested from the
 * cache, fetching data as necessary and calls the redrawing of 
 * the visualisation.
 * Unfortunately, the slider can't express the youngest figure if it
 * is not youngest data available. By hey, you can't have it all.
 * If uitme_prevent_reload is set, the routine does nothing.
 *
 * Uses uidata_ringpurl and uidata_ringdatacb from uidata.c
 */
void uitime_slider_change(time_t slider_from, time_t slider_to)
{
     enum rcache_load_status cache;
     GtkWidget *chart_btn;

     if (uitime_prevent_reload) {
          /* asked to do nothing */
          g_print("uitime_slider_change() - uitime_prevent_reload set; returning\n");
	  return;
     }

     if (slider_from > slider_to) {
          /* not all the arguments are in, some times casused by 
	   * premiture events */
          g_print("uitime_slider_change() - from > to; returning\n");
	  return;
     }

     if (!uitime_avail_oldest) {
          /* do nothing if not previously been called by uitime_slider_set()
	   * which we treat as initialisation for a valid ring */
          g_print("uitime_slider_change() - unitialialised; returning\n");
	  return;
     }

     if (!slider_from) {
          g_print("uitime_slider_change() - no slider_from parameter; returning\n");
	  return;
     }

     if (slider_from == uitime_view_oldest &&
	 slider_to   == uitime_view_youngest) {
          /* if boundaries are the same as before, do nothing */
          /*g_print("uitime_slider_change() - no change from remembered data; returning\n");*/
	  return;
     }

     /* --- Start loading data from now --- */
     uilog_setprogress("Loading data", 0.3, 0);

     if (uidata_ringpurl) {
          /* request data to be loaded from a ROUTE via the rcache (rather
	   * than a function [below]); the rcache will decide if anything 
	   * new needs to be loaded to give us the complete range */
          if (uidata_type == FILEROUTE_TYPE_TEXT ||
	      uidata_type == FILEROUTE_TYPE_UNKNOWN ||
	      /* the below is a hack for now until we work out wether
	       * the data contains a time column */
	      uidata_type == FILEROUTE_TYPE_TSV || 
	      uidata_type == FILEROUTE_TYPE_CSV || 
	      uidata_type == FILEROUTE_TYPE_PSV || 
	      uidata_type == FILEROUTE_TYPE_SSV) {
	       /* time plays no part in the drawing of this data, so dim the 
		* time slider */
	       cache = rcache_request(uidata_ringpurl, 0, 0, uidata_type);
	  } else {
	       cache = rcache_request(uidata_ringpurl, slider_from, slider_to,
				      uidata_type);
	  }
	  if (cache == RCACHE_LOAD_FAIL) {
	       /* if there is a total fail, assume that it didn't work */
	       uivis_change_view(UIVIS_SPLASH);
	       elog_printf(FATAL, "<big><b>Unable to Load Data</b></big>\n\n"
			   "Unable to load data for the ring '%s'. "
			   "Check the log messages for more details",
			   uidata_ringpurl);
	       uilog_clearprogress();
	       return;
	  }
	  if (cache == RCACHE_LOAD_HOLE) {
	       /* Partial fails (1) assume that there is holey data and that
		* its ok to log but not alert */
	       elog_printf(INFO, "Gap in data between %s and %s, "
			   "unable to update. Older data exists",
			   util_decdatetime (uitime_view_oldest), 
			   util_sdecdatetime(uitime_view_youngest) );
	       uilog_clearprogress();
	       return;
	  }
	  if (cache == RCACHE_LOAD_TIMETABLE) {
	       /* Success! Tabular data loaded with _time */
	       /* Don't need to change the visualisation, as it will have
		* been chosen by the user */
	       /*
	       uivis_change_view(UIVIS_CHART);
	       uidata_illuminate_vis_btns(UIVIS_CHART); */
	       uidata_illuminate_time();
	  } else if (cache == RCACHE_LOAD_TABLE) {
	       /* Success! Tabular data loaded without _time */
	       /* If the current visualisation is CHART, then we have to 
		* change down to TABLE as we can't display charts (yet)
		* without a time base */
	       chart_btn = get_widget("ringview_chart_btn");
	       if (gtk_toggle_tool_button_get_active(
				GTK_TOGGLE_TOOL_BUTTON(chart_btn))) {
		 
		    uivis_change_view(UIVIS_TABLE);
		    uidata_illuminate_vis_btns(UIVIS_TABLE);
	       }
	       uidata_deilluminate_time();
	  } else {
	       /* Success! Text data */
	       uivis_change_view(UIVIS_TEXT);
	       uidata_illuminate_vis_btns(UIVIS_TEXT);
	       uidata_deilluminate_time();
	  }
     } else {
          /* no purl, so this should be dynamic. This is handled by 
	   * uivis_draw() */
     }

     /* redraw visualisation and update the remembered time */
     uitime_view_oldest   = slider_from;
     uitime_view_youngest = slider_to;

#if 0
     g_print("redrawing the visualisation from %s to %s\n",
	     util_decdatetime (uitime_view_oldest), 
	     util_sdecdatetime(uitime_view_youngest) );
#endif

     uilog_setprogress("Drawing data", 0.6, 0);

     uivis_draw(uidata_ringpurl, uidata_ringdatacb, 
		uitime_view_oldest, uitime_view_youngest);

     uilog_clearprogress();
}

/* callback to update visualisation bounds window and then display it */
G_MODULE_EXPORT void 
uitime_on_bounds_win (GtkObject *object, gpointer user_data)
{
     GtkWidget *databounds_win;
     GtkCalendar *databounds_first_calendar, *databounds_last_calendar;
     struct tm oldest_tm, youngest_tm;

     /* get widgets */
     databounds_win = get_widget("databounds_win");
     databounds_first_calendar = GTK_CALENDAR (
	gtk_builder_get_object(gui_builder, "databounds_first_calendar"));
     databounds_last_calendar = GTK_CALENDAR ( 
	gtk_builder_get_object(gui_builder, "databounds_last_calendar"));

     /* convert time_t to gmtimes */
     gmtime_r(&uitime_view_oldest, &oldest_tm);
     gmtime_r(&uitime_view_youngest, &youngest_tm);

     /* set the bounds to the time slider */
     gtk_calendar_select_month(databounds_first_calendar, 
			       oldest_tm.tm_mon, 1900+oldest_tm.tm_year);
     gtk_calendar_select_day(databounds_first_calendar,
			     oldest_tm.tm_mday);
     gtk_calendar_select_month(databounds_last_calendar, 
			       youngest_tm.tm_mon, 1900+youngest_tm.tm_year);
     gtk_calendar_select_day(databounds_last_calendar, 
			     youngest_tm.tm_mday);

     /* show window */
     gtk_window_present(GTK_WINDOW(databounds_win));
}


/* callback to update the visualisation bounds from the databounds window */
G_MODULE_EXPORT void 
uitime_on_bounds_set (GtkObject *object, gpointer user_data)
{
     GtkWidget *databounds_win;
     GtkCalendar *databounds_first_calendar, *databounds_last_calendar;
     struct tm oldest_tm = {0,0,0,0,0,0,0,0,0};
     struct tm youngest_tm = {0,0,0,0,0,0,0,0,0};
     time_t from_t, to_t;

     /* get widgets */
     databounds_win = get_widget("databounds_win");
     databounds_first_calendar = GTK_CALENDAR (
	gtk_builder_get_object(gui_builder, "databounds_first_calendar"));
     databounds_last_calendar = GTK_CALENDAR ( 
	gtk_builder_get_object(gui_builder, "databounds_last_calendar"));

     /* get the dates from the calendar widgets */
     gtk_calendar_get_date(databounds_first_calendar, 
			   (guint *) &oldest_tm.tm_year,
			   (guint *) &oldest_tm.tm_mon, 
			   (guint *) &oldest_tm.tm_mday);
     gtk_calendar_get_date(databounds_last_calendar, 
			   (guint *) &youngest_tm.tm_year,
			   (guint *) &youngest_tm.tm_mon, 
			   (guint *) &youngest_tm.tm_mday);
     oldest_tm.tm_year -= 1900;
     youngest_tm.tm_year -= 1900;
     from_t = mktime(&oldest_tm);
     to_t   = mktime(&youngest_tm);

     uitime_set_slider_view(from_t, to_t);
}


/* callback to update the visualisation bounds to see the maximum possible */
G_MODULE_EXPORT void 
uitime_on_data_everything (GtkObject *object, gpointer user_data)
{
     if (uitime_avail_youngest && uitime_avail_oldest)
       uitime_set_slider_view(uitime_avail_oldest, uitime_avail_youngest);
}


