/*
 * Habitat Gtk GUI implementation
 *
 * Nigel Stuckey, November 1999
 * Copyright System Garden Limited 1999-2001. All rights reserved.
 */

#define CLOCKWORK_PROG "bin/clockwork"
#define PROBE_PROG "probe"

int is_clockwork_running(char **key, char **user, char **tty, char **datestr);
int is_clockwork_runable();
