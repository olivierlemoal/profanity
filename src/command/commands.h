/*
 * commands.h
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

#ifndef COMMANDS_H
#define COMMANDS_H

// Command help strings
typedef struct cmd_help_t {
    const gchar *usage;
    const gchar *short_help;
    const gchar *long_help[50];
} CommandHelp;

/*
 * Command structure
 *
 * cmd - The command string including leading '/'
 * func - The function to execute for the command
 * parser - The function used to parse arguments
 * min_args - Minimum number of arguments
 * max_args - Maximum number of arguments
 * help - A help struct containing usage info etc
 */
typedef struct cmd_t {
    gchar *cmd;
    gboolean (*func)(gchar **args, struct cmd_help_t help);
    gchar** (*parser)(const char * const inp, int min, int max, gboolean *result);
    int min_args;
    int max_args;
    void (*setting_func)(void);
    CommandHelp help;
} Command;

gboolean cmd_about(gchar **args, struct cmd_help_t help);
gboolean cmd_account(gchar **args, struct cmd_help_t help);
gboolean cmd_autoaway(gchar **args, struct cmd_help_t help);
gboolean cmd_autoconnect(gchar **args, struct cmd_help_t help);
gboolean cmd_autoping(gchar **args, struct cmd_help_t help);
gboolean cmd_away(gchar **args, struct cmd_help_t help);
gboolean cmd_beep(gchar **args, struct cmd_help_t help);
gboolean cmd_caps(gchar **args, struct cmd_help_t help);
gboolean cmd_chat(gchar **args, struct cmd_help_t help);
gboolean cmd_chlog(gchar **args, struct cmd_help_t help);
gboolean cmd_clear(gchar **args, struct cmd_help_t help);
gboolean cmd_close(gchar **args, struct cmd_help_t help);
gboolean cmd_connect(gchar **args, struct cmd_help_t help);
gboolean cmd_decline(gchar **args, struct cmd_help_t help);
gboolean cmd_disco(gchar **args, struct cmd_help_t help);
gboolean cmd_disconnect(gchar **args, struct cmd_help_t help);
gboolean cmd_dnd(gchar **args, struct cmd_help_t help);
gboolean cmd_flash(gchar **args, struct cmd_help_t help);
gboolean cmd_gone(gchar **args, struct cmd_help_t help);
gboolean cmd_grlog(gchar **args, struct cmd_help_t help);
gboolean cmd_group(gchar **args, struct cmd_help_t help);
gboolean cmd_help(gchar **args, struct cmd_help_t help);
gboolean cmd_history(gchar **args, struct cmd_help_t help);
gboolean cmd_carbons(gchar **args, struct cmd_help_t help);
gboolean cmd_info(gchar **args, struct cmd_help_t help);
gboolean cmd_intype(gchar **args, struct cmd_help_t help);
gboolean cmd_invite(gchar **args, struct cmd_help_t help);
gboolean cmd_invites(gchar **args, struct cmd_help_t help);
gboolean cmd_join(gchar **args, struct cmd_help_t help);
gboolean cmd_leave(gchar **args, struct cmd_help_t help);
gboolean cmd_log(gchar **args, struct cmd_help_t help);
gboolean cmd_mouse(gchar **args, struct cmd_help_t help);
gboolean cmd_msg(gchar **args, struct cmd_help_t help);
gboolean cmd_nick(gchar **args, struct cmd_help_t help);
gboolean cmd_notify(gchar **args, struct cmd_help_t help);
gboolean cmd_online(gchar **args, struct cmd_help_t help);
gboolean cmd_otr(gchar **args, struct cmd_help_t help);
gboolean cmd_outtype(gchar **args, struct cmd_help_t help);
gboolean cmd_prefs(gchar **args, struct cmd_help_t help);
gboolean cmd_priority(gchar **args, struct cmd_help_t help);
gboolean cmd_quit(gchar **args, struct cmd_help_t help);
gboolean cmd_reconnect(gchar **args, struct cmd_help_t help);
gboolean cmd_room(gchar **args, struct cmd_help_t help);
gboolean cmd_rooms(gchar **args, struct cmd_help_t help);
gboolean cmd_bookmark(gchar **args, struct cmd_help_t help);
gboolean cmd_roster(gchar **args, struct cmd_help_t help);
gboolean cmd_software(gchar **args, struct cmd_help_t help);
gboolean cmd_splash(gchar **args, struct cmd_help_t help);
gboolean cmd_states(gchar **args, struct cmd_help_t help);
gboolean cmd_status(gchar **args, struct cmd_help_t help);
gboolean cmd_statuses(gchar **args, struct cmd_help_t help);
gboolean cmd_sub(gchar **args, struct cmd_help_t help);
gboolean cmd_theme(gchar **args, struct cmd_help_t help);
gboolean cmd_tiny(gchar **args, struct cmd_help_t help);
gboolean cmd_titlebar(gchar **args, struct cmd_help_t help);
gboolean cmd_vercheck(gchar **args, struct cmd_help_t help);
gboolean cmd_who(gchar **args, struct cmd_help_t help);
gboolean cmd_win(gchar **args, struct cmd_help_t help);
gboolean cmd_wins(gchar **args, struct cmd_help_t help);
gboolean cmd_xa(gchar **args, struct cmd_help_t help);
gboolean cmd_alias(gchar **args, struct cmd_help_t help);
gboolean cmd_xmlconsole(gchar **args, struct cmd_help_t help);
gboolean cmd_ping(gchar **args, struct cmd_help_t help);
gboolean cmd_form(gchar **args, struct cmd_help_t help);
gboolean cmd_occupants(gchar **args, struct cmd_help_t help);
gboolean cmd_kick(gchar **args, struct cmd_help_t help);
gboolean cmd_ban(gchar **args, struct cmd_help_t help);
gboolean cmd_subject(gchar **args, struct cmd_help_t help);
gboolean cmd_affiliation(gchar **args, struct cmd_help_t help);
gboolean cmd_role(gchar **args, struct cmd_help_t help);
gboolean cmd_privileges(gchar **args, struct cmd_help_t help);
gboolean cmd_presence(gchar **args, struct cmd_help_t help);
gboolean cmd_wrap(gchar **args, struct cmd_help_t help);
gboolean cmd_time(gchar **args, struct cmd_help_t help);
gboolean cmd_resource(gchar **args, struct cmd_help_t help);
gboolean cmd_inpblock(gchar **args, struct cmd_help_t help);

gboolean cmd_form_field(char *tag, gchar **args);

#endif
