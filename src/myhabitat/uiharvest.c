/*
 * MyHabitat Gtk GUI Harvest handling, implementing the callbacks and
 * connections for talking to the Harvest repository.
 *
 * Nigel Stuckey, July 2010
 * Copyright System Garden Ltd 2004,2010. All rights reserved
 */

#include <time.h>
#include <string.h>
#include <gtk/gtk.h>
#include "../iiab/elog.h"
#include "../iiab/cf.h"
#include "../iiab/iiab.h"
#include "../iiab/route.h"
#include "../iiab/rt_sqlrs.h"
#include "../iiab/nmalloc.h"
#include "../iiab/util.h"
#include "uiharvest.h"
#include "uilog.h"
#include "main.h"


/* Initialise Harvest class in MyHabitat by loading the repository details
 * into the gui if they have been set up */
void harv_init() {
     harv_populate_gui();
}

void harv_fini() {
}


/*
 * Populate harv_win with stored repository data
 */
void harv_populate_gui ()
{
     GtkWidget *harv_username_entry, *harv_password_entry,  *harv_org_entry, 
       *harv_proxy_detail_check, *harv_enable_check, *harv_send_data_check,
       *harv_proxy_host_entry, *harv_proxy_port_entry, *harv_proxy_user_entry,
       *harv_proxy_pass_entry;
     char *geturl, *puturl;
     char *userpwd=NULL, *proxy=NULL, *proxyuserpwd=NULL, *sslkeypwd=NULL;
     char *proxy_host, *mproxy_host, *proxy_user;
     TABLE auth;
     CF_VALS cookies;
     char *username=NULL, *organisation=NULL, *password=NULL;
     char *cookiejar, *cert=NULL, *host;
     int len, rowkey;

     /* GUI widgets */
     harv_enable_check       = get_widget("harv_enable_check");
     harv_send_data_check    = get_widget("harv_send_data_check");
     harv_username_entry     = get_widget("harv_username_entry");
     harv_password_entry     = get_widget("harv_password_entry");
     harv_org_entry          = get_widget("harv_org_entry");
     harv_proxy_detail_check = get_widget("harv_proxy_detail_check");
     harv_proxy_host_entry   = get_widget("harv_proxy_host_entry");
     harv_proxy_port_entry   = get_widget("harv_proxy_port_entry");
     harv_proxy_user_entry   = get_widget("harv_proxy_user_entry");
     harv_proxy_pass_entry   = get_widget("harv_proxy_pass_entry");

     /* get repository urls & account details */
     geturl = cf_getstr(iiab_cf, RT_SQLRS_GET_URLKEY);
     puturl = cf_getstr(iiab_cf, RT_SQLRS_PUT_URLKEY);
     rt_sqlrs_get_credentials("myhabitat configuration", &auth, &cookies, 
			      &cookiejar);
     if (cookies) {
          username     = cf_getstr(cookies, "__username");
	  password     = cf_getstr(cookies, "__password");
	  organisation = cf_getstr(cookies, "__repository");
     }

     /* populate GUI */
     if (username)
          gtk_entry_set_text(GTK_ENTRY(harv_username_entry), username);
     if (password)
          gtk_entry_set_text(GTK_ENTRY(harv_password_entry), password);
     if (organisation)
          gtk_entry_set_text(GTK_ENTRY(harv_org_entry), organisation);

     /* If URL and account set up then repository is active */
     if (geturl && *geturl && username && password && organisation) {
          /* toggle set to on and populate */
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(harv_enable_check),
					TRUE);
     } else {
          /* toggle set to off */
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(harv_enable_check),
					FALSE);
     }

     /* Report partial data */
     if (username && password && organisation) {
          /* Harvest account set up */
          if (geturl == NULL || *geturl == '\0') {
	       elog_printf(FATAL, "Repository loction not set up for reading "
			   "but have a Harvest account");
	  }
          if (puturl == NULL || *puturl == '\0') {
	       elog_printf(FATAL, "Repository loction not set up for writing "
			   "but have a Harvest account");
	  }
     } else if (username || password || organisation) {
          /* Harvest account partially set up */
          elog_printf(FATAL, "<big><b>Harvest account details not "
		      "complete</b></big>\n"
		      "Check username, password and organisation have "
		      "been filled in");
     }

     /* to be completed, the insertion of the SSL key */
     /*gtk_text_insert(GTK_TEXT(repos_key_entry), NULL, NULL, NULL, 
       sslkey, length(sslkey));*/


     /* even if the repository is off and the fields greyed out, 
      * still populate the remainder of the gui */

     /* --- authentication section --- */

     /* Get host from the get url and look up in the auth table */
     if (geturl && *geturl) {
         host = strstr(geturl, "://");
	 if (!host) {
	     elog_printf(ERROR, "url '%s' in unrecognisable format", geturl);
	     if (auth) table_destroy(auth);
	     if (cookies) cf_destroy(cookies);
	     if (cookiejar) nfree(cookiejar);
	     return;
	 }
	 host += 3;
	 len = strcspn(host, ":/");
	 if (len) {
	     host = xnmemdup(host, len+1);
	     host[len] = '\0';
	 } else {
	     host = xnstrdup("localhost");
	 }
     }

     /* lookup auth and proxy config */
     if (auth) {
	  rowkey = table_search(auth, "host", host);
	  if (rowkey != -1) {
	       userpwd      = table_getcurrentcell(auth, "userpwd");
	       proxy        = table_getcurrentcell(auth, "proxy");
	       proxyuserpwd = table_getcurrentcell(auth, "proxyuserpwd");
	       sslkeypwd    = table_getcurrentcell(auth, "sslkeypwd");
	       cert         = table_getcurrentcell(auth, "cert");
	  }
#if 0
/* no realm details in harvest -- a repository may have it */
	  if (userpwd && *userpwd) {
	       /* user[:pwd] is the format */
	       len = strcspn(userpwd, ":");
	       realm_user = xnmemdup(userpwd, len+1);
	       realm_user[len] = '\0';
	       gtk_entry_set_text(GTK_ENTRY(repos_realm_user_entry), 
				  realm_user);
	       if (userpwd[len])
		    gtk_entry_set_text(GTK_ENTRY(repos_realm_pw_entry), 
				       userpwd+len);
	       nfree(realm_user);
	  }
#endif
	  if (proxy && *proxy) {
	       /* [driver://]host[:port] is the format */
	       proxy_host = strstr(proxy, "://");
	       if (proxy_host)
		    proxy_host += 3;
	       else
		    proxy_host = proxy;
	       len = strcspn(proxy_host, ":");
	       mproxy_host = xnmemdup(proxy_host, len+1);
	       mproxy_host[len] = '\0';
	       gtk_entry_set_text(GTK_ENTRY(harv_proxy_host_entry), 
				  mproxy_host);

	       /* match [:port] */
	       if (proxy_host[len] && proxy_host[len+1])
		    gtk_entry_set_text(GTK_ENTRY(harv_proxy_port_entry), 
				       proxy_host+len+1);
	       nfree(mproxy_host);
	  }
	  if (proxyuserpwd && *proxyuserpwd) {
	       /* user[:pwd] is the format */
	       len = strcspn(proxyuserpwd, ":");
	       proxy_user = xnmemdup(proxyuserpwd, len+1);
	       proxy_user[len] = '\0';
	       gtk_entry_set_text(GTK_ENTRY(harv_proxy_user_entry), 
				  proxy_user);

	       if (proxyuserpwd[len])
		    gtk_entry_set_text(GTK_ENTRY(harv_proxy_pass_entry), 
				       proxyuserpwd+len);
	       nfree(proxy_user);
	  }

#if 0
          /* the following not currently used in form */
	  /* if (sslkeypwd)
	       gtk_entry_set_text(GTK_ENTRY(harv_puturl_entry), puturl);
	  */
	  if (cert && *cert)
	       gtk_text_insert(GTK_TEXT(repos_key_entry), NULL, NULL, NULL, 
			       cert, strlen(cert));
#endif
     }

     if (geturl && *geturl && host)
          nfree(host);
     if (auth) table_destroy(auth);
     if (cookies) cf_destroy(cookies);
     if (cookiejar) nfree(cookiejar);
}

/* Enable the use of harvest browsing. Show the account details for connection 
 * and also the proxy option */
G_MODULE_EXPORT void 
harv_on_enable (GtkObject *object, gpointer user_data)
{
     int vis;

     if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(object)) )
          vis=0;	/* button is on, change to make invisible */
     else
          vis=1;

     harv_account_detail_visibility(vis);
}


/* Send data to Harvest. Enable the use of replication to the repository. 
 * Must have a valid account to harvest */
G_MODULE_EXPORT void 
harv_on_send (GtkObject *object, gpointer user_data)
{
}


/* Show or hide Harvest proxy detail visibility */
void harv_account_detail_visibility(int visible)
{
     GtkWidget *harv_account_label, *harv_username_prompt, 
       *harv_password_prompt, *harv_org_prompt, *harv_username_entry, 
       *harv_password_entry,  *harv_org_entry, *harv_proxy_detail_check,
       *harv_description_label, *harv_getaccount_btn;

     harv_account_label      = get_widget("harv_account_label");
     harv_username_prompt    = get_widget("harv_username_prompt");
     harv_password_prompt    = get_widget("harv_password_prompt");
     harv_org_prompt         = get_widget("harv_org_prompt");
     harv_username_entry     = get_widget("harv_username_entry");
     harv_password_entry     = get_widget("harv_password_entry");
     harv_org_entry          = get_widget("harv_org_entry");
     harv_proxy_detail_check = get_widget("harv_proxy_detail_check");
     harv_description_label  = get_widget("harv_description_label");
     harv_getaccount_btn     = get_widget("harv_getaccount_btn");

     if (visible) {
          gtk_widget_hide(harv_account_label);
	  gtk_widget_hide(harv_username_prompt);
	  gtk_widget_hide(harv_password_prompt);
	  gtk_widget_hide(harv_org_prompt);
	  gtk_widget_hide(harv_username_entry);
	  gtk_widget_hide(harv_password_entry);
	  gtk_widget_hide(harv_org_entry);
	  gtk_widget_hide(harv_proxy_detail_check);
	  gtk_widget_hide(harv_description_label);
	  gtk_widget_hide(harv_getaccount_btn);
	  harv_proxy_detail_visibility(1);
     } else {
          gtk_widget_show(harv_account_label);
	  gtk_widget_show(harv_username_prompt);
	  gtk_widget_show(harv_password_prompt);
	  gtk_widget_show(harv_org_prompt);
	  gtk_widget_show(harv_username_entry);
	  gtk_widget_show(harv_password_entry);
	  gtk_widget_show(harv_org_entry);
	  gtk_widget_show(harv_proxy_detail_check);
	  gtk_widget_show(harv_description_label);
	  gtk_widget_show(harv_getaccount_btn);
	  if ( gtk_toggle_button_get_active(
			     GTK_TOGGLE_BUTTON(harv_proxy_detail_check)) )
	       harv_proxy_detail_visibility(0);
     }
}


/* Add or remove the proxy details in the harvest connecton window */
G_MODULE_EXPORT void 
harv_on_proxy_detail (GtkObject *object, gpointer user_data)
{
     int vis;

     if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(object)) )
          vis=0;	/* button is on, change to make invisible */
     else
          vis=1;

     harv_proxy_detail_visibility(vis);
}


/* Show or hide Harvest proxy detail visibility */
void harv_proxy_detail_visibility(int visible)
{
     GtkWidget *harv_proxy_host_prompt, *harv_proxy_port_prompt, 
       *harv_proxy_user_prompt, *harv_proxy_pass_prompt;
     GtkWidget *harv_proxy_host_entry, *harv_proxy_port_entry, 
       *harv_proxy_user_entry, *harv_proxy_pass_entry;

     harv_proxy_host_prompt = get_widget("harv_proxy_host_prompt");
     harv_proxy_port_prompt = get_widget("harv_proxy_port_prompt");
     harv_proxy_user_prompt = get_widget("harv_proxy_user_prompt");
     harv_proxy_pass_prompt = get_widget("harv_proxy_pass_prompt");
     harv_proxy_host_entry  = get_widget("harv_proxy_host_entry");
     harv_proxy_port_entry  = get_widget("harv_proxy_port_entry");
     harv_proxy_user_entry  = get_widget("harv_proxy_user_entry");
     harv_proxy_pass_entry  = get_widget("harv_proxy_pass_entry");

     if (visible) {
          gtk_widget_hide(harv_proxy_host_prompt);
	  gtk_widget_hide(harv_proxy_port_prompt);
	  gtk_widget_hide(harv_proxy_user_prompt);
	  gtk_widget_hide(harv_proxy_pass_prompt);
	  gtk_widget_hide(harv_proxy_host_entry);
	  gtk_widget_hide(harv_proxy_port_entry);
	  gtk_widget_hide(harv_proxy_user_entry);
	  gtk_widget_hide(harv_proxy_pass_entry);
     } else {
          gtk_widget_show(harv_proxy_host_prompt);
	  gtk_widget_show(harv_proxy_port_prompt);
	  gtk_widget_show(harv_proxy_user_prompt);
	  gtk_widget_show(harv_proxy_pass_prompt);
	  gtk_widget_show(harv_proxy_host_entry);
	  gtk_widget_show(harv_proxy_port_entry);
	  gtk_widget_show(harv_proxy_user_entry);
	  gtk_widget_show(harv_proxy_pass_entry);
     }
}


/* Configure the repository given the details in the GUI */
G_MODULE_EXPORT void 
harv_on_ok (GtkObject *object, gpointer user_data)
{
     harv_save_gui();
}


/* Save the contents of the GUI (the repository details) into the rt_sqlrs
 * configuration files */
void harv_save_gui()
{
     GtkWidget *harv_username_entry, *harv_password_entry,  *harv_org_entry, 
       *harv_proxy_detail_check, *harv_enable_check, *harv_send_data_check,
       *harv_proxy_host_entry, *harv_proxy_port_entry, *harv_proxy_user_entry,
       *harv_proxy_pass_entry;
     char *geturl, *puturl;
     TABLE auth;
     CF_VALS cookies;
     char *cert=NULL, *host=NULL;
     TREE *authrow;
     char *harv_user, *harv_pw, *harv_repos, *harv_sslkey;
     char *proxy_user, *proxy_pw, *proxy_host, *proxy_port;
     char *userpwd=NULL, *proxy=NULL, *proxyuserpwd=NULL;
     int enabled, len;
     char *proxy_cfg_nameval[] = {"host", "userpwd", "proxy", "proxyuserpwd",
				  "sslkeypwd", "cert", NULL};

     /* GUI widgets */
     harv_enable_check       = get_widget("harv_enable_check");
     harv_send_data_check    = get_widget("harv_send_data_check");
     harv_username_entry     = get_widget("harv_username_entry");
     harv_password_entry     = get_widget("harv_password_entry");
     harv_org_entry          = get_widget("harv_org_entry");
     harv_proxy_detail_check = get_widget("harv_proxy_detail_check");
     harv_proxy_host_entry   = get_widget("harv_proxy_host_entry");
     harv_proxy_port_entry   = get_widget("harv_proxy_port_entry");
     harv_proxy_user_entry   = get_widget("harv_proxy_user_entry");
     harv_proxy_pass_entry   = get_widget("harv_proxy_pass_entry");

     /* grab the datum from each widget */
     enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
					     (harv_enable_check));

     harv_user  = (char*)gtk_entry_get_text( GTK_ENTRY(harv_username_entry) );
     harv_pw    = (char*)gtk_entry_get_text( GTK_ENTRY(harv_password_entry) );
     harv_repos = (char*)gtk_entry_get_text( GTK_ENTRY(harv_org_entry) );

     proxy_user = (char*)gtk_entry_get_text( GTK_ENTRY(harv_proxy_user_entry) );
     proxy_pw   = (char*)gtk_entry_get_text( GTK_ENTRY(harv_proxy_pass_entry) );
     proxy_host = (char*)gtk_entry_get_text( GTK_ENTRY(harv_proxy_host_entry) );
     proxy_port = (char*)gtk_entry_get_text( GTK_ENTRY(harv_proxy_port_entry) );

     /* dont do anything unless we have the route of harvest, the reason 
      * being that we need a URL to index the cookies et al */
     geturl = cf_getstr(iiab_cf, RT_SQLRS_GET_URLKEY);
     puturl = cf_getstr(iiab_cf, RT_SQLRS_PUT_URLKEY);
     if (geturl == NULL || *geturl == '\0') {
          elog_printf(FATAL, "Repository loction not set up, so unable to "
		      "save configuration details. Please check configuration "
		      "sources for %s, which sould contain Harvest's address",
		      RT_SQLRS_GET_URLKEY);
	  return;
     }

     if ( ! (*harv_user && *harv_pw && *harv_repos) ) {
          uilog_modal_alert("Need full Harvest account details", 
			    "Unable to save repository details until username, "
			    "password and organisation are complete");
	  return;
     }

     /* save repository account details */
     cookies = cf_create();
     cf_putstr(cookies, "__username",   harv_user);
     cf_putstr(cookies, "__password",   harv_pw);
     cf_putstr(cookies, "__repository", harv_repos);
     if (!rt_sqlrs_put_cookies_cred("myhabitat cookie configuration", cookies))
          elog_printf(FATAL, "Unable to save repository account details");
     cf_destroy(cookies);

     /* -- save the authorisation in a table -- */
     /* Get host from the get url and look up in the auth table */
     if (geturl && *geturl) {
         host = strstr(geturl, "://");
	 if (!host) {
	     elog_printf(ERROR, "url '%s' in unrecognisable format", geturl);
	     return;
	 }
	 host += 3;
	 len = strcspn(host, ":/");
	 if (len) {
	     host = xnmemdup(host, len+1);
	     host[len] = '\0';
	 } else {
	     host = xnstrdup("localhost");
	 }
     }

     /* proxy host and port -- [driver://]host[:port] is the format */
     if (proxy_host && *proxy_host) {
          if (proxy_port && *proxy_port)
	       proxy = util_strjoin("http://", proxy_host, ":", proxy_port, 
				    NULL);
	  else
	       proxy = util_strjoin("http://", proxy_host, NULL);

	  /* proxy host account -- user[:pwd] is the format */
	  if (proxy_user && *proxy_user) {
	       if (proxy_pw && *proxy_pw)
		    proxyuserpwd = util_strjoin(proxy_user, ":", proxy_pw, 
						NULL);
	       else
		    proxyuserpwd = xnstrdup(proxy_user);
	  }
	  cert = "";
	  harv_sslkey = "";

	  /* create the structure (TREE* into TABLE) and save it (RT_SQLRS) */
	  authrow = tree_create();
	  tree_add(authrow, "host",         host);
	  tree_add(authrow, "userpwd",      userpwd);
	  tree_add(authrow, "proxy",        proxy);
	  tree_add(authrow, "proxyuserpwd", proxyuserpwd);
	  tree_add(authrow, "sslkeypwd",    harv_sslkey);
	  tree_add(authrow, "cert",         cert);

	  auth = table_create_a(proxy_cfg_nameval);
	  table_addrow_noalloc(auth, authrow);
	  if (!rt_sqlrs_put_proxy_cred("myhabitat configuration", auth))
	       elog_printf(ERROR, "Unable to save proxy details");

	  /* clear everything up */
	  table_destroy(auth);
	  tree_destroy(authrow);
	  nfree(userpwd);
	  nfree(proxy);
	  nfree(proxyuserpwd);
     }

     if (host)
          nfree(host);
}


/* Test that the login details work with harvest */
G_MODULE_EXPORT void 
harv_on_test (GtkObject *object, gpointer user_data)
{
     GtkWidget *harv_username_entry, *harv_password_entry,  *harv_org_entry, 
       *harv_proxy_detail_check, *harv_enable_check, *harv_send_data_check,
       *harv_proxy_host_entry, *harv_proxy_port_entry, *harv_proxy_user_entry,
       *harv_proxy_pass_entry;
     char *geturl, *puturl;
     TABLE auth;
     CF_VALS cookies;
     char *cert=NULL, *host;
     TREE *authrow;
     char *harv_user, *harv_pw, *harv_repos, *harv_sslkey;
     char *proxy_user, *proxy_pw, *proxy_host, *proxy_port;
     char *userpwd=NULL, *proxy=NULL, *proxyuserpwd=NULL;
     int enabled, len;
     char *proxy_cfg_nameval[] = {"host", "userpwd", "proxy", "proxyuserpwd",
				  "sslkeypwd", "cert", NULL};

     /* GUI widgets */
     harv_username_entry     = get_widget("harv_username_entry");
     harv_password_entry     = get_widget("harv_password_entry");
     harv_org_entry          = get_widget("harv_org_entry");
     harv_proxy_host_entry   = get_widget("harv_proxy_host_entry");
     harv_proxy_port_entry   = get_widget("harv_proxy_port_entry");
     harv_proxy_user_entry   = get_widget("harv_proxy_user_entry");
     harv_proxy_pass_entry   = get_widget("harv_proxy_pass_entry");

     /* grab the datum from each widget */
     enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
					     (harv_enable_check));

     harv_user  = (char*)gtk_entry_get_text( GTK_ENTRY(harv_username_entry) );
     harv_pw    = (char*)gtk_entry_get_text( GTK_ENTRY(harv_password_entry) );
     harv_repos = (char*)gtk_entry_get_text( GTK_ENTRY(harv_org_entry) );

     proxy_user = (char*)gtk_entry_get_text( GTK_ENTRY(harv_proxy_user_entry) );
     proxy_pw   = (char*)gtk_entry_get_text( GTK_ENTRY(harv_proxy_pass_entry) );
     proxy_host = (char*)gtk_entry_get_text( GTK_ENTRY(harv_proxy_host_entry) );
     proxy_port = (char*)gtk_entry_get_text( GTK_ENTRY(harv_proxy_port_entry) );

     /* don't do anything unless we have the route of harvest, the reason 
      * being that we need a URL to index the cookies et al */
     geturl = cf_getstr(iiab_cf, RT_SQLRS_GET_URLKEY);
     puturl = cf_getstr(iiab_cf, RT_SQLRS_PUT_URLKEY);
     if (geturl == NULL || *geturl == '\0') {
          elog_printf(FATAL, "Repository loction not set up, so unable to "
		      "test. Please check configuration sources for %s, "
		      "which sould contain Harvest's address",
		      RT_SQLRS_GET_URLKEY);
	  return;
     }

     /* make a call to harvest to see if we can login */
     /* TO BE DONE */
}


/*  */
G_MODULE_EXPORT void 
harv_on_help (GtkObject *object, gpointer user_data)
{
}


/*  */
G_MODULE_EXPORT void 
harv_on_get_account (GtkObject *object, gpointer user_data)
{
}


