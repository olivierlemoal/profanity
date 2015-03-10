/*
 * preferences.h
 *
 * Copyright (C) 2012 - 2015 James Booth <boothj5@gmail.com>
 *
 * This file is part of Profanity.
 *
 * Profanity is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Profanity is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Profanity.  If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link the code of portions of this program with the OpenSSL library under
 * certain conditions as described in each individual source file, and
 * distribute linked combinations including the two.
 *
 * You must obey the GNU General Public License in all respects for all of the
 * code used other than OpenSSL. If you modify file(s) with this exception, you
 * may extend this exception to your version of the file(s), but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version. If you delete this exception statement from all
 * source files in the program, then also delete it here.
 *
 */

#ifndef PREFERENCES_H
#define PREFERENCES_H

#include "config.h"

#include <glib.h>
#ifdef HAVE_NCURSESW_NCURSES_H
#include <ncursesw/ncurses.h>
#elif HAVE_NCURSES_H
#include <ncurses.h>
#endif

#define PREFS_MIN_LOG_SIZE 64
#define PREFS_MAX_LOG_SIZE 1048580

// represents all settings in .profrc
// each enum value is mapped to a group and key in .profrc (see preferences.c)
typedef enum {
    PREF_SPLASH,
    PREF_BEEP,
    PREF_VERCHECK,
    PREF_THEME,
    PREF_TITLEBAR_SHOW,
    PREF_TITLEBAR_GOODBYE,
    PREF_FLASH,
    PREF_INTYPE,
    PREF_HISTORY,
    PREF_CARBONS,
    PREF_MOUSE,
    PREF_OCCUPANTS,
    PREF_OCCUPANTS_SIZE,
    PREF_ROSTER,
    PREF_ROSTER_SIZE,
    PREF_ROSTER_OFFLINE,
    PREF_ROSTER_RESOURCE,
    PREF_ROSTER_BY,
    PREF_MUC_PRIVILEGES,
    PREF_PRESENCE,
    PREF_WRAP,
    PREF_TIME,
    PREF_TIME_STATUSBAR,
    PREF_STATUSES,
    PREF_STATUSES_CONSOLE,
    PREF_STATUSES_CHAT,
    PREF_STATUSES_MUC,
    PREF_STATES,
    PREF_OUTTYPE,
    PREF_NOTIFY_TYPING,
    PREF_NOTIFY_TYPING_CURRENT,
    PREF_NOTIFY_MESSAGE,
    PREF_NOTIFY_MESSAGE_CURRENT,
    PREF_NOTIFY_MESSAGE_TEXT,
    PREF_NOTIFY_ROOM,
    PREF_NOTIFY_ROOM_CURRENT,
    PREF_NOTIFY_ROOM_TEXT,
    PREF_NOTIFY_INVITE,
    PREF_NOTIFY_SUB,
    PREF_CHLOG,
    PREF_GRLOG,
    PREF_AUTOAWAY_CHECK,
    PREF_AUTOAWAY_MODE,
    PREF_AUTOAWAY_MESSAGE,
    PREF_CONNECT_ACCOUNT,
    PREF_DEFAULT_ACCOUNT,
    PREF_LOG_ROTATE,
    PREF_LOG_SHARED,
    PREF_OTR_LOG,
    PREF_OTR_WARN,
    PREF_OTR_POLICY,
    PREF_RESOURCE_TITLE,
    PREF_RESOURCE_MESSAGE,
    PREF_INPBLOCK_DYNAMIC
} preference_t;

typedef struct prof_alias_t {
    gchar *name;
    gchar *value;
} ProfAlias;

void prefs_load(void);
void prefs_close(void);

char * prefs_find_login(char *prefix);
void prefs_reset_login_search(void);
char * prefs_autocomplete_boolean_choice(const char * const prefix);
void prefs_reset_boolean_choice(void);

gint prefs_get_gone(void);
void prefs_set_gone(gint value);

void prefs_set_notify_remind(gint period);
gint prefs_get_notify_remind(void);

void prefs_set_max_log_size(gint value);
gint prefs_get_max_log_size(void);
gint prefs_get_priority(void);
void prefs_set_reconnect(gint value);
gint prefs_get_reconnect(void);
void prefs_set_autoping(gint value);
gint prefs_get_autoping(void);
gint prefs_get_inpblock(void);
void prefs_set_inpblock(gint value);

void prefs_set_occupants_size(gint value);
gint prefs_get_occupants_size(void);
void prefs_set_roster_size(gint value);
gint prefs_get_roster_size(void);

gint prefs_get_autoaway_time(void);
void prefs_set_autoaway_time(gint value);

void prefs_add_login(const char *jid);

gboolean prefs_add_alias(const char * const name, const char * const value);
gboolean prefs_remove_alias(const char * const name);
char* prefs_get_alias(const char * const name);
GList* prefs_get_aliases(void);
void prefs_free_aliases(GList *aliases);

gboolean prefs_get_boolean(preference_t pref);
void prefs_set_boolean(preference_t pref, gboolean value);
char * prefs_get_string(preference_t pref);
void prefs_free_string(char *pref);
void prefs_set_string(preference_t pref, char *value);

#endif
