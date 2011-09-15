/*
 * Collection of old functions, just in case we need them again.
 * Not to be compiled into the app
 */

uilog_init()
{
#if 0
     /* Attach GtkTreeView tables for each tab to their models */
     /* -- Disabled as glade should do this, but it soemtimes quite flakey
      *    so if you re-enable this code, it will put the columns back -- */
     gtk_tree_view_set_model( GTK_TREE_VIEW(debugtab), 
			      GTK_TREE_MODEL(logstore) );
     gtk_tree_view_set_model( GTK_TREE_VIEW(diagtab), 
			      GTK_TREE_MODEL(logstore) );
     gtk_tree_view_set_model( GTK_TREE_VIEW(normtab), 
			      GTK_TREE_MODEL(logstore) );

     uilog_create_treeview_cols(normtab,  logstore);
     uilog_create_treeview_cols(diagtab,  logstore);
     uilog_create_treeview_cols(debugtab, logstore);
#endif
}

/* Create a treeview table columns, suitable for viewing logs and will
 * bind to the passed log store model.
 * Use this of Glade stops working and you need to add columns 
 * long hand with code!! */
void uilog_create_treeview_cols(GtkWidget *treeview, GtkListStore *logstore) {
     GtkTreeViewColumn *viewcol;
     GtkCellRenderer   *renderer;

     /* Set up columns */
     renderer = gtk_cell_renderer_text_new ();
     viewcol = gtk_tree_view_column_new_with_attributes("Time", renderer,
							"text", 0,
							NULL);
     gtk_tree_view_column_set_sizing(viewcol, GTK_TREE_VIEW_COLUMN_FIXED);
     gtk_tree_view_column_set_fixed_width(viewcol, 100);
     gtk_tree_view_append_column( GTK_TREE_VIEW(treeview), viewcol);

     renderer = gtk_cell_renderer_text_new ();
     viewcol = gtk_tree_view_column_new_with_attributes("Severity", renderer,
							"text", 1,
							NULL);
     gtk_tree_view_column_set_sizing(viewcol, GTK_TREE_VIEW_COLUMN_FIXED);
     gtk_tree_view_column_set_fixed_width(viewcol, 55);
     gtk_tree_view_append_column( GTK_TREE_VIEW(treeview), viewcol);

     renderer = gtk_cell_renderer_text_new ();
     viewcol = gtk_tree_view_column_new_with_attributes("Message", renderer,
							"text", 2,
							NULL);
     gtk_tree_view_column_set_sizing(viewcol, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
     gtk_tree_view_append_column( GTK_TREE_VIEW(treeview), viewcol);

     renderer = gtk_cell_renderer_text_new ();
     viewcol = gtk_tree_view_column_new_with_attributes("Function", renderer,
							"text", 3,
							NULL);
     gtk_tree_view_column_set_sizing(viewcol, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
     gtk_tree_view_append_column( GTK_TREE_VIEW(treeview), viewcol);

     renderer = gtk_cell_renderer_text_new ();
     viewcol = gtk_tree_view_column_new_with_attributes("File", renderer,
							"text", 4,
							NULL);
     gtk_tree_view_column_set_sizing(viewcol, GTK_TREE_VIEW_COLUMN_FIXED);
     gtk_tree_view_column_set_fixed_width(viewcol, 80);
     gtk_tree_view_append_column( GTK_TREE_VIEW(treeview), viewcol);

     renderer = gtk_cell_renderer_text_new ();
     viewcol = gtk_tree_view_column_new_with_attributes("Line", renderer,
							"text", 5,
							NULL);
     gtk_tree_view_column_set_sizing(viewcol, GTK_TREE_VIEW_COLUMN_FIXED);
     gtk_tree_view_column_set_fixed_width(viewcol, 40);
     gtk_tree_view_append_column( GTK_TREE_VIEW(treeview), viewcol);

     /*g_object_set(renderer, "text", "Boooo!", NULL);*/
				  
}
