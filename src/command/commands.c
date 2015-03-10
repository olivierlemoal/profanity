/*
 * commands.c
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

#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <glib.h>
#ifdef HAVE_LIBOTR
#include <libotr/proto.h>
#endif

#include "chat_session.h"
#include "command/commands.h"
#include "command/command.h"
#include "common.h"
#include "config/accounts.h"
#include "config/account.h"
#include "config/preferences.h"
#include "config/theme.h"
#include "contact.h"
#include "roster_list.h"
#include "jid.h"
#include "log.h"
#include "muc.h"
#ifdef HAVE_LIBOTR
#include "otr/otr.h"
#endif
#include "profanity.h"
#include "tools/autocomplete.h"
#include "tools/parser.h"
#include "tools/tinyurl.h"
#include "xmpp/xmpp.h"
#include "xmpp/bookmark.h"
#include "ui/ui.h"
#include "ui/windows.h"

static void _update_presence(const resource_presence_t presence,
    const char * const show, gchar **args);
static gboolean _cmd_set_boolean_preference(gchar *arg, struct cmd_help_t help,
    const char * const display, preference_t pref);
static int _strtoi(char *str, int *saveptr, int min, int max);
static void _cmd_show_filtered_help(char *heading, gchar *cmd_filter[], int filter_size);
static gint _compare_commands(Command *a, Command *b);
static void _who_room(gchar **args, struct cmd_help_t help);
static void _who_roster(gchar **args, struct cmd_help_t help);

extern GHashTable *commands;

gboolean
cmd_connect(gchar **args, struct cmd_help_t help)
{
    gboolean result = FALSE;

    jabber_conn_status_t conn_status = jabber_get_connection_status();

    if ((conn_status != JABBER_DISCONNECTED) && (conn_status != JABBER_STARTED)) {
        cons_show("You are either connected already, or a login is in process.");
        result = TRUE;
    } else {
        gchar *opt_keys[] = { "server", "port", NULL };
        gboolean parsed;

        GHashTable *options = parse_options(&args[args[0] ? 1 : 0], opt_keys, &parsed);
        if (!parsed) {
            cons_show("Usage: %s", help.usage);
            cons_show("");
            return TRUE;
        }

        char *altdomain = g_hash_table_lookup(options, "server");

        int port = 0;
        if (g_hash_table_contains(options, "port")) {
            char *port_str = g_hash_table_lookup(options, "port");
            if (_strtoi(port_str, &port, 1, 65535) != 0) {
                port = 0;
                cons_show("");
                return TRUE;
            }
        }

        char *user = args[0];
        char *def = prefs_get_string(PREF_DEFAULT_ACCOUNT);
        if(!user){
            if(def){
                user = def;
                cons_show("Using default account %s.", user);
            } else {
                cons_show("No default account.");
                g_free(def);
                return TRUE;
            }
        }

        char *lower = g_utf8_strdown(user, -1);
        char *jid;
        g_free(def);
        def = NULL;

        ProfAccount *account = accounts_get_account(lower);
        if (account != NULL) {
            jid = account_create_full_jid(account);
            if(account->eval_password){
                // Evaluate as shell command to retrieve password
                GString *cmd = g_string_append(g_string_new(account->eval_password), " 2>/dev/null");
                FILE *stream = popen(cmd->str, "r");
                g_string_free(cmd, TRUE);
                if(stream){
                    // Limit to READ_BUF_SIZE bytes to prevent overflows in the case of a poorly chosen command
                    account->password = g_malloc(READ_BUF_SIZE);
                    if(!account->password){
                        log_error("Failed to allocate enough memory to read eval_password output");
                        cons_show("Error evaluating password, see logs for details.");
                        return TRUE;
                    }
                    account->password = fgets(account->password, READ_BUF_SIZE, stream);
                    pclose(stream);
                    if(!account->password){
                        log_error("No result from eval_password.");
                        cons_show("Error evaluating password, see logs for details.");
                        return TRUE;
                    }
                    // strip trailing newline
                    if (g_str_has_suffix(account->password, "\n")) {
                        account->password[strlen(account->password)-1] = '\0';
                    }
                } else {
                    log_error("popen failed when running eval_password.");
                    cons_show("Error evaluating password, see logs for details.");
                    return TRUE;
                }
            } else if (!account->password) {
                account->password = ui_ask_password();
            }
            cons_show("Connecting with account %s as %s", account->name, jid);
            if(g_hash_table_contains(options, "port") || g_hash_table_contains(options, "server"))
                cons_show("Ignoring extra connect options. Please set them with /account set");
            conn_status = jabber_connect_with_account(account);
            account_free(account);
        } else {
            char *passwd = ui_ask_password();
            jid = strdup(lower);
            cons_show("Connecting as %s", jid);
            conn_status = jabber_connect_with_details(jid, passwd, altdomain, port);
            free(passwd);
        }
        g_free(lower);

        if (conn_status == JABBER_DISCONNECTED) {
            cons_show_error("Connection attempt for %s failed.", jid);
            log_info("Connection attempt for %s failed", jid);
        }

        options_destroy(options);

        free(jid);

        result = TRUE;
    }

    return result;
}

gboolean
cmd_account(gchar **args, struct cmd_help_t help)
{
    char *command = args[0];

    if (command == NULL) {
        if (jabber_get_connection_status() != JABBER_CONNECTED) {
            cons_show("Usage: %s", help.usage);
        } else {
            ProfAccount *account = accounts_get_account(jabber_get_account_name());
            cons_show_account(account);
            account_free(account);
        }
    } else if (strcmp(command, "list") == 0) {
        gchar **accounts = accounts_get_list();
        cons_show_account_list(accounts);
        g_strfreev(accounts);
    } else if (strcmp(command, "show") == 0) {
        char *account_name = args[1];
        if (account_name == NULL) {
            cons_show("Usage: %s", help.usage);
        } else {
            ProfAccount *account = accounts_get_account(account_name);
            if (account == NULL) {
                cons_show("No such account.");
                cons_show("");
            } else {
                cons_show_account(account);
                account_free(account);
            }
        }
    } else if (strcmp(command, "add") == 0) {
        char *account_name = args[1];
        if (account_name == NULL) {
            cons_show("Usage: %s", help.usage);
        } else {
            accounts_add(account_name, NULL, 0);
            cons_show("Account created.");
            cons_show("");
        }
    } else if (strcmp(command, "remove") == 0) {
        char *account_name = args[1];
        if(!account_name) {
            cons_show("Usage: %s", help.usage);
        } else {
            char *def = prefs_get_string(PREF_DEFAULT_ACCOUNT);
            if(accounts_remove(account_name)){
                cons_show("Account %s removed.", account_name);
                if(def && strcmp(def, account_name) == 0){
                    prefs_set_string(PREF_DEFAULT_ACCOUNT, NULL);
                    cons_show("Default account removed because the corresponding account was removed.");
                }
            } else {
                cons_show("Failed to remove account %s.", account_name);
                cons_show("Either the account does not exist, or an unknown error occurred.");
            }
            cons_show("");
            g_free(def);
        }
    } else if (strcmp(command, "enable") == 0) {
        char *account_name = args[1];
        if (account_name == NULL) {
            cons_show("Usage: %s", help.usage);
        } else {
            if (accounts_enable(account_name)) {
                cons_show("Account enabled.");
                cons_show("");
            } else {
                cons_show("No such account: %s", account_name);
                cons_show("");
            }
        }
    } else if (strcmp(command, "disable") == 0) {
        char *account_name = args[1];
        if (account_name == NULL) {
            cons_show("Usage: %s", help.usage);
        } else {
            if (accounts_disable(account_name)) {
                cons_show("Account disabled.");
                cons_show("");
            } else {
                cons_show("No such account: %s", account_name);
                cons_show("");
            }
        }
    } else if (strcmp(command, "rename") == 0) {
        if (g_strv_length(args) != 3) {
            cons_show("Usage: %s", help.usage);
        } else {
            char *account_name = args[1];
            char *new_name = args[2];

            if (accounts_rename(account_name, new_name)) {
                cons_show("Account renamed.");
                cons_show("");
            } else {
                cons_show("Either account %s doesn't exist, or account %s already exists.", account_name, new_name);
                cons_show("");
            }
        }
    } else if (strcmp(command, "default") == 0) {
        if(g_strv_length(args) == 1){
            char *def = prefs_get_string(PREF_DEFAULT_ACCOUNT);

            if(def){
                cons_show("The default account is %s.", def);
                free(def);
            } else {
                cons_show("No default account.");
            }
        } else if(g_strv_length(args) == 2){
            if(strcmp(args[1], "off") == 0){
                prefs_set_string(PREF_DEFAULT_ACCOUNT, NULL);
                cons_show("Removed default account.");
            } else {
                cons_show("Usage: %s", help.usage);
            }
        } else if(g_strv_length(args) == 3) {
            if(strcmp(args[1], "set") == 0){
                if(accounts_get_account(args[2])){
                    prefs_set_string(PREF_DEFAULT_ACCOUNT, args[2]);
                    cons_show("Default account set to %s.", args[2]);
                } else {
                    cons_show("Account %s does not exist.", args[2]);
                }
            } else {
                cons_show("Usage: %s", help.usage);
            }
        } else {
            cons_show("Usage: %s", help.usage);
        }
    } else if (strcmp(command, "set") == 0) {
        if (g_strv_length(args) != 4) {
            cons_show("Usage: %s", help.usage);
        } else {
            char *account_name = args[1];
            char *property = args[2];
            char *value = args[3];

            if (!accounts_account_exists(account_name)) {
                cons_show("Account %s doesn't exist", account_name);
                cons_show("");
            } else {
                if (strcmp(property, "jid") == 0) {
                    Jid *jid = jid_create(args[3]);
                    if (jid == NULL) {
                        cons_show("Malformed jid: %s", value);
                    } else {
                        accounts_set_jid(account_name, jid->barejid);
                        cons_show("Updated jid for account %s: %s", account_name, jid->barejid);
                        if (jid->resourcepart != NULL) {
                            accounts_set_resource(account_name, jid->resourcepart);
                            cons_show("Updated resource for account %s: %s", account_name, jid->resourcepart);
                        }
                        cons_show("");
                    }
                    jid_destroy(jid);
                } else if (strcmp(property, "server") == 0) {
                    accounts_set_server(account_name, value);
                    cons_show("Updated server for account %s: %s", account_name, value);
                    cons_show("");
                } else if (strcmp(property, "port") == 0) {
                    int port;
                    if (_strtoi(value, &port, 1, 65535) != 0) {
                        cons_show("");
                        return TRUE;
                    } else {
                        accounts_set_port(account_name, port);
                        cons_show("Updated port for account %s: %s", account_name, value);
                        cons_show("");
                    }
                } else if (strcmp(property, "resource") == 0) {
                    accounts_set_resource(account_name, value);
                    cons_show("Updated resource for account %s: %s", account_name, value);
                    cons_show("");
                } else if (strcmp(property, "password") == 0) {
                    if(accounts_get_account(account_name)->eval_password != NULL) {
                        cons_show("Cannot set password when eval_password is set.");
                    } else {
                        accounts_set_password(account_name, value);
                        cons_show("Updated password for account %s", account_name);
                        cons_show("");
                    }
                } else if (strcmp(property, "eval_password") == 0) {
                    if(accounts_get_account(account_name)->password != NULL) {
                        cons_show("Cannot set eval_password when password is set.");
                    } else {
                        accounts_set_eval_password(account_name, value);
                        cons_show("Updated eval_password for account %s", account_name);
                        cons_show("");
                    }
                } else if (strcmp(property, "muc") == 0) {
                    accounts_set_muc_service(account_name, value);
                    cons_show("Updated muc service for account %s: %s", account_name, value);
                    cons_show("");
                } else if (strcmp(property, "nick") == 0) {
                    accounts_set_muc_nick(account_name, value);
                    cons_show("Updated muc nick for account %s: %s", account_name, value);
                    cons_show("");
                } else if (strcmp(property, "otr") == 0) {
                    if ((g_strcmp0(value, "manual") != 0)
                            && (g_strcmp0(value, "opportunistic") != 0)
                            && (g_strcmp0(value, "always") != 0)) {
                        cons_show("OTR policy must be one of: manual, opportunistic or always.");
                    } else {
                        accounts_set_otr_policy(account_name, value);
                        cons_show("Updated OTR policy for account %s: %s", account_name, value);
                        cons_show("");
                    }
                } else if (strcmp(property, "status") == 0) {
                    if (!valid_resource_presence_string(value) && (strcmp(value, "last") != 0)) {
                        cons_show("Invalid status: %s", value);
                    } else {
                        accounts_set_login_presence(account_name, value);
                        cons_show("Updated login status for account %s: %s", account_name, value);
                    }
                    cons_show("");
                } else if (valid_resource_presence_string(property)) {
                        int intval;

                        if (_strtoi(value, &intval, -128, 127) == 0) {
                            resource_presence_t presence_type = resource_presence_from_string(property);
                            switch (presence_type)
                            {
                                case (RESOURCE_ONLINE):
                                    accounts_set_priority_online(account_name, intval);
                                    break;
                                case (RESOURCE_CHAT):
                                    accounts_set_priority_chat(account_name, intval);
                                    break;
                                case (RESOURCE_AWAY):
                                    accounts_set_priority_away(account_name, intval);
                                    break;
                                case (RESOURCE_XA):
                                    accounts_set_priority_xa(account_name, intval);
                                    break;
                                case (RESOURCE_DND):
                                    accounts_set_priority_dnd(account_name, intval);
                                    break;
                            }

                            jabber_conn_status_t conn_status = jabber_get_connection_status();
                            if (conn_status == JABBER_CONNECTED) {
                                char *connected_account = jabber_get_account_name();
                                resource_presence_t last_presence = accounts_get_last_presence(connected_account);

                                if (presence_type == last_presence) {
                                    char *message = jabber_get_presence_message();
                                    presence_update(last_presence, message, 0);
                                }
                            }
                            cons_show("Updated %s priority for account %s: %s", property, account_name, value);
                            cons_show("");
                        }
                } else {
                    cons_show("Invalid property: %s", property);
                    cons_show("");
                }
            }
        }
    } else if (strcmp(command, "clear") == 0) {
        if (g_strv_length(args) != 3) {
            cons_show("Usage: %s", help.usage);
        } else {
            char *account_name = args[1];
            char *property = args[2];

            if (!accounts_account_exists(account_name)) {
                cons_show("Account %s doesn't exist", account_name);
                cons_show("");
            } else {
                if (strcmp(property, "password") == 0) {
                    accounts_clear_password(account_name);
                    cons_show("Removed password for account %s", account_name);
                    cons_show("");
                } else if (strcmp(property, "eval_password") == 0) {
                    accounts_clear_eval_password(account_name);
                    cons_show("Removed eval password for account %s", account_name);
                    cons_show("");
                } else if (strcmp(property, "server") == 0) {
                    accounts_clear_server(account_name);
                    cons_show("Removed server for account %s", account_name);
                    cons_show("");
                } else if (strcmp(property, "port") == 0) {
                    accounts_clear_port(account_name);
                    cons_show("Removed port for account %s", account_name);
                    cons_show("");
                } else if (strcmp(property, "otr") == 0) {
                    accounts_clear_otr(account_name);
                    cons_show("OTR policy removed for account %s", account_name);
                    cons_show("");
                } else {
                    cons_show("Invalid property: %s", property);
                    cons_show("");
                }
            }
        }
    } else {
        cons_show("");
    }

    return TRUE;
}

gboolean
cmd_sub(gchar **args, struct cmd_help_t help)
{
    jabber_conn_status_t conn_status = jabber_get_connection_status();

    if (conn_status != JABBER_CONNECTED) {
        cons_show("You are currently not connected.");
        return TRUE;
    }

    char *subcmd, *jid;
    subcmd = args[0];
    jid = args[1];

    if (subcmd == NULL) {
        cons_show("Usage: %s", help.usage);
        return TRUE;
    }

    if (strcmp(subcmd, "sent") == 0) {
        cons_show_sent_subs();
        return TRUE;
    }

    if (strcmp(subcmd, "received") == 0) {
        cons_show_received_subs();
        return TRUE;
    }

    win_type_t win_type = ui_current_win_type();
    if ((win_type != WIN_CHAT) && (jid == NULL)) {
        cons_show("You must specify a contact.");
        return TRUE;
    }

    if (jid == NULL) {
        ProfChatWin *chatwin = wins_get_current_chat();
        jid = chatwin->barejid;
    }

    Jid *jidp = jid_create(jid);

    if (strcmp(subcmd, "allow") == 0) {
        presence_subscription(jidp->barejid, PRESENCE_SUBSCRIBED);
        cons_show("Accepted subscription for %s", jidp->barejid);
        log_info("Accepted subscription for %s", jidp->barejid);
    } else if (strcmp(subcmd, "deny") == 0) {
        presence_subscription(jidp->barejid, PRESENCE_UNSUBSCRIBED);
        cons_show("Deleted/denied subscription for %s", jidp->barejid);
        log_info("Deleted/denied subscription for %s", jidp->barejid);
    } else if (strcmp(subcmd, "request") == 0) {
        presence_subscription(jidp->barejid, PRESENCE_SUBSCRIBE);
        cons_show("Sent subscription request to %s.", jidp->barejid);
        log_info("Sent subscription request to %s.", jidp->barejid);
    } else if (strcmp(subcmd, "show") == 0) {
        PContact contact = roster_get_contact(jidp->barejid);
        if ((contact == NULL) || (p_contact_subscription(contact) == NULL)) {
            if (win_type == WIN_CHAT) {
                ui_current_print_line("No subscription information for %s.", jidp->barejid);
            } else {
                cons_show("No subscription information for %s.", jidp->barejid);
            }
        } else {
            if (win_type == WIN_CHAT) {
                if (p_contact_pending_out(contact)) {
                    ui_current_print_line("%s subscription status: %s, request pending.",
                        jidp->barejid, p_contact_subscription(contact));
                } else {
                    ui_current_print_line("%s subscription status: %s.", jidp->barejid,
                        p_contact_subscription(contact));
                }
            } else {
                if (p_contact_pending_out(contact)) {
                    cons_show("%s subscription status: %s, request pending.",
                        jidp->barejid, p_contact_subscription(contact));
                } else {
                    cons_show("%s subscription status: %s.", jidp->barejid,
                        p_contact_subscription(contact));
                }
            }
        }
    } else {
        cons_show("Usage: %s", help.usage);
    }

    jid_destroy(jidp);

    return TRUE;
}

gboolean
cmd_disconnect(gchar **args, struct cmd_help_t help)
{
    if (jabber_get_connection_status() == JABBER_CONNECTED) {
        char *jid = strdup(jabber_get_fulljid());
        cons_show("%s logged out successfully.", jid);
        jabber_disconnect();
        roster_clear();
        muc_invites_clear();
        chat_sessions_clear();
        ui_disconnected();
        free(jid);
    } else {
        cons_show("You are not currently connected.");
    }

    return TRUE;
}

gboolean
cmd_quit(gchar **args, struct cmd_help_t help)
{
    log_info("Profanity is shutting down...");
    exit(0);
    return FALSE;
}

gboolean
cmd_wins(gchar **args, struct cmd_help_t help)
{
    if (args[0] == NULL) {
        cons_show_wins();
    } else if (strcmp(args[0], "tidy") == 0) {
        ui_tidy_wins();
    } else if (strcmp(args[0], "prune") == 0) {
        ui_prune_wins();
    } else if (strcmp(args[0], "swap") == 0) {
        if ((args[1] == NULL) || (args[2] == NULL)) {
            cons_show("Usage: %s", help.usage);
        } else {
            int source_win = atoi(args[1]);
            int target_win = atoi(args[2]);
            if ((source_win == 1) || (target_win == 1)) {
                cons_show("Cannot move console window.");
            } else if (source_win == 10 || target_win == 10) {
                cons_show("Window 10 does not exist");
            } else if (source_win != target_win) {
                gboolean swapped = ui_swap_wins(source_win, target_win);
                if (swapped) {
                    cons_show("Swapped windows %d <-> %d", source_win, target_win);
                } else {
                    cons_show("Window %d does not exist", source_win);
                }
            } else {
                cons_show("Same source and target window supplied.");
            }
        }
    }

    return TRUE;
}

gboolean
cmd_win(gchar **args, struct cmd_help_t help)
{
    int num = atoi(args[0]);
    gboolean switched = ui_switch_win(num);
    if (switched == FALSE) {
        cons_show("Window %d does not exist.", num);
    }
    return TRUE;
}

gboolean
cmd_help(gchar **args, struct cmd_help_t help)
{
    int num_args = g_strv_length(args);
    if (num_args == 0) {
        cons_help();
    } else if (strcmp(args[0], "commands") == 0) {
        cons_show("");
        cons_show("All commands");
        cons_show("");

        GList *ordered_commands = NULL;
        GHashTableIter iter;
        gpointer key;
        gpointer value;

        g_hash_table_iter_init(&iter, commands);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            ordered_commands = g_list_insert_sorted(ordered_commands, value, (GCompareFunc)_compare_commands);
        }

        GList *curr = ordered_commands;
        while (curr != NULL) {
            Command *cmd = curr->data;
            cons_show("%-12s: %s", cmd->cmd, cmd->help.short_help);
            curr = g_list_next(curr);
        }
        g_list_free(ordered_commands);
        g_list_free(curr);

        cons_show("");
        cons_show("Use /help [command] without the leading slash, for help on a specific command");
        cons_show("");

    } else if (strcmp(args[0], "basic") == 0) {
        gchar *filter[] = { "/about", "/clear", "/close", "/connect",
            "/disconnect", "/help", "/msg", "/join", "/quit", "/vercheck",
            "/wins", "/ping" };
        _cmd_show_filtered_help("Basic commands", filter, ARRAY_SIZE(filter));

    } else if (strcmp(args[0], "chatting") == 0) {
        gchar *filter[] = { "/chlog", "/otr", "/gone", "/history",
            "/info", "/intype", "/msg", "/notify", "/outtype", "/status",
            "/close", "/clear", "/tiny" };
        _cmd_show_filtered_help("Chat commands", filter, ARRAY_SIZE(filter));

    } else if (strcmp(args[0], "groupchat") == 0) {
        gchar *filter[] = { "/close", "/clear", "/decline", "/grlog",
            "/invite", "/invites", "/join", "/leave", "/notify", "/msg", "/room",
            "/rooms", "/tiny", "/who", "/nick", "/privileges", "/info", "/occupants" };
        _cmd_show_filtered_help("Groupchat commands", filter, ARRAY_SIZE(filter));

    } else if (strcmp(args[0], "presences") == 0) {
        gchar *filter[] = { "/autoaway", "/away", "/chat", "/dnd",
            "/online", "/priority", "/account", "/status", "/statuses", "/who",
            "/xa" };
        _cmd_show_filtered_help("Presence commands", filter, ARRAY_SIZE(filter));

    } else if (strcmp(args[0], "contacts") == 0) {
        gchar *filter[] = { "/group", "/roster", "/sub", "/who" };
        _cmd_show_filtered_help("Roster commands", filter, ARRAY_SIZE(filter));

    } else if (strcmp(args[0], "service") == 0) {
        gchar *filter[] = { "/caps", "/disco", "/info", "/software", "/rooms" };
        _cmd_show_filtered_help("Service discovery commands", filter, ARRAY_SIZE(filter));

    } else if (strcmp(args[0], "settings") == 0) {
        gchar *filter[] = { "/account", "/autoaway", "/autoping", "/autoconnect", "/beep",
            "/carbons", "/chlog", "/flash", "/gone", "/grlog", "/history", "/intype",
            "/log", "/mouse", "/notify", "/outtype", "/prefs", "/priority",
            "/reconnect", "/roster", "/splash", "/states", "/statuses", "/theme",
            "/titlebar", "/vercheck", "/privileges", "/occupants", "/presence", "/wrap" };
        _cmd_show_filtered_help("Settings commands", filter, ARRAY_SIZE(filter));

    } else if (strcmp(args[0], "navigation") == 0) {
        cons_navigation_help();
    } else {
        char *cmd = args[0];
        char cmd_with_slash[1 + strlen(cmd) + 1];
        sprintf(cmd_with_slash, "/%s", cmd);

        const gchar **help_text = NULL;
        Command *command = g_hash_table_lookup(commands, cmd_with_slash);

        if (command != NULL) {
            help_text = command->help.long_help;
        }

        cons_show("");
        if (help_text != NULL) {
            ProfWin *console = wins_get_console();
            ui_show_lines(console, help_text);
        } else {
            cons_show("No such command.");
        }

        cons_show("");
    }

    return TRUE;
}

gboolean
cmd_about(gchar **args, struct cmd_help_t help)
{
    ui_about();
    return TRUE;
}

gboolean
cmd_prefs(gchar **args, struct cmd_help_t help)
{
    if (args[0] == NULL) {
        cons_prefs();
        cons_show("Use the /account command for preferences for individual accounts.");
    } else if (strcmp(args[0], "ui") == 0) {
        cons_show("");
        cons_show_ui_prefs();
        cons_show("");
    } else if (strcmp(args[0], "desktop") == 0) {
        cons_show("");
        cons_show_desktop_prefs();
        cons_show("");
    } else if (strcmp(args[0], "chat") == 0) {
        cons_show("");
        cons_show_chat_prefs();
        cons_show("");
    } else if (strcmp(args[0], "log") == 0) {
        cons_show("");
        cons_show_log_prefs();
        cons_show("");
    } else if (strcmp(args[0], "conn") == 0) {
        cons_show("");
        cons_show_connection_prefs();
        cons_show("");
    } else if (strcmp(args[0], "presence") == 0) {
        cons_show("");
        cons_show_presence_prefs();
        cons_show("");
    } else if (strcmp(args[0], "otr") == 0) {
        cons_show("");
        cons_show_otr_prefs();
        cons_show("");
    } else {
        cons_show("Usage: %s", help.usage);
    }

    return TRUE;
}

gboolean
cmd_theme(gchar **args, struct cmd_help_t help)
{
    // list themes
    if (g_strcmp0(args[0], "list") == 0) {
        GSList *themes = theme_list();
        cons_show_themes(themes);
        g_slist_free_full(themes, g_free);

    // load a theme
    } else if (g_strcmp0(args[0], "load") == 0) {
        if (args[1] == NULL) {
            cons_show("Usage: %s", help.usage);
        } else if (theme_load(args[1])) {
            ui_load_colours();
            prefs_set_string(PREF_THEME, args[1]);
            if (prefs_get_boolean(PREF_ROSTER)) {
                ui_show_roster();
            } else {
                ui_hide_roster();
            }
            if (prefs_get_boolean(PREF_OCCUPANTS)) {
                ui_show_all_room_rosters();
            } else {
                ui_hide_all_room_rosters();
            }
            ui_redraw();
            cons_show("Loaded theme: %s", args[1]);
        } else {
            cons_show("Couldn't find theme: %s", args[1]);
        }

    // show colours
    } else if (g_strcmp0(args[0], "colours") == 0) {
        cons_theme_colours();
    } else {
        cons_show("Usage: %s", help.usage);
    }

    return TRUE;
}

static void
_who_room(gchar **args, struct cmd_help_t help)
{
    if ((g_strv_length(args) == 2) && (args[1] != NULL)) {
        cons_show("Argument group is not applicable to chat rooms.");
        return;
    }

    // bad arg
    if (args[0] != NULL &&
            (g_strcmp0(args[0], "online") != 0) &&
            (g_strcmp0(args[0], "available") != 0) &&
            (g_strcmp0(args[0], "unavailable") != 0) &&
            (g_strcmp0(args[0], "away") != 0) &&
            (g_strcmp0(args[0], "chat") != 0) &&
            (g_strcmp0(args[0], "xa") != 0) &&
            (g_strcmp0(args[0], "dnd") != 0) &&
            (g_strcmp0(args[0], "any") != 0) &&
            (g_strcmp0(args[0], "moderator") != 0) &&
            (g_strcmp0(args[0], "participant") != 0) &&
            (g_strcmp0(args[0], "visitor") != 0) &&
            (g_strcmp0(args[0], "owner") != 0) &&
            (g_strcmp0(args[0], "admin") != 0) &&
            (g_strcmp0(args[0], "member") != 0) &&
            (g_strcmp0(args[0], "outcast") != 0)) {
        cons_show("Usage: %s", help.usage);
        return;
    }

    ProfMucWin *mucwin = wins_get_current_muc();

    // presence filter
    if (args[0] == NULL ||
            (g_strcmp0(args[0], "online") == 0) ||
            (g_strcmp0(args[0], "available") == 0) ||
            (g_strcmp0(args[0], "unavailable") == 0) ||
            (g_strcmp0(args[0], "away") == 0) ||
            (g_strcmp0(args[0], "chat") == 0) ||
            (g_strcmp0(args[0], "xa") == 0) ||
            (g_strcmp0(args[0], "dnd") == 0) ||
            (g_strcmp0(args[0], "any") == 0)) {

        char *presence = args[0];
        GList *occupants = muc_roster(mucwin->roomjid);

        // no arg, show all contacts
        if ((presence == NULL) || (g_strcmp0(presence, "any") == 0)) {
            ui_room_roster(mucwin->roomjid, occupants, NULL);

        // available
        } else if (strcmp("available", presence) == 0) {
            GList *filtered = NULL;

            while (occupants != NULL) {
                Occupant *occupant = occupants->data;
                if (muc_occupant_available(occupant)) {
                    filtered = g_list_append(filtered, occupant);
                }
                occupants = g_list_next(occupants);
            }

            ui_room_roster(mucwin->roomjid, filtered, "available");

        // unavailable
        } else if (strcmp("unavailable", presence) == 0) {
            GList *filtered = NULL;

            while (occupants != NULL) {
                Occupant *occupant = occupants->data;
                if (!muc_occupant_available(occupant)) {
                    filtered = g_list_append(filtered, occupant);
                }
                occupants = g_list_next(occupants);
            }

            ui_room_roster(mucwin->roomjid, filtered, "unavailable");

        // show specific status
        } else {
            GList *filtered = NULL;

            while (occupants != NULL) {
                Occupant *occupant = occupants->data;
                const char *presence_str = string_from_resource_presence(occupant->presence);
                if (strcmp(presence_str, presence) == 0) {
                    filtered = g_list_append(filtered, occupant);
                }
                occupants = g_list_next(occupants);
            }

            ui_room_roster(mucwin->roomjid, filtered, presence);
        }

        g_list_free(occupants);

    // role or affiliation filter
    } else {
        if (g_strcmp0(args[0], "moderator") == 0) {
            ui_show_room_role_list(mucwin, MUC_ROLE_MODERATOR);
            return;
        }
        if (g_strcmp0(args[0], "participant") == 0) {
            ui_show_room_role_list(mucwin, MUC_ROLE_PARTICIPANT);
            return;
        }
        if (g_strcmp0(args[0], "visitor") == 0) {
            ui_show_room_role_list(mucwin, MUC_ROLE_VISITOR);
            return;
        }

        if (g_strcmp0(args[0], "owner") == 0) {
            ui_show_room_affiliation_list(mucwin, MUC_AFFILIATION_OWNER);
            return;
        }
        if (g_strcmp0(args[0], "admin") == 0) {
            ui_show_room_affiliation_list(mucwin, MUC_AFFILIATION_ADMIN);
            return;
        }
        if (g_strcmp0(args[0], "member") == 0) {
            ui_show_room_affiliation_list(mucwin, MUC_AFFILIATION_MEMBER);
            return;
        }
        if (g_strcmp0(args[0], "outcast") == 0) {
            ui_show_room_affiliation_list(mucwin, MUC_AFFILIATION_OUTCAST);
            return;
        }
    }
}

static void
_who_roster(gchar **args, struct cmd_help_t help)
{
    char *presence = args[0];

    // bad arg
    if ((presence != NULL)
            && (strcmp(presence, "online") != 0)
            && (strcmp(presence, "available") != 0)
            && (strcmp(presence, "unavailable") != 0)
            && (strcmp(presence, "offline") != 0)
            && (strcmp(presence, "away") != 0)
            && (strcmp(presence, "chat") != 0)
            && (strcmp(presence, "xa") != 0)
            && (strcmp(presence, "dnd") != 0)
            && (strcmp(presence, "any") != 0)) {
        cons_show("Usage: %s", help.usage);
        return;
    }

    char *group = NULL;
    if ((g_strv_length(args) == 2) && (args[1] != NULL)) {
        group = args[1];
    }

    cons_show("");
    GSList *list = NULL;
    if (group != NULL) {
        list = roster_get_group(group);
        if (list == NULL) {
            cons_show("No such group: %s.", group);
            return;
        }
    } else {
        list = roster_get_contacts();
        if (list == NULL) {
            cons_show("No contacts in roster.");
            return;
        }
    }

    // no arg, show all contacts
    if ((presence == NULL) || (g_strcmp0(presence, "any") == 0)) {
        if (group != NULL) {
            if (list == NULL) {
                cons_show("No contacts in group %s.", group);
            } else {
                cons_show("%s:", group);
                cons_show_contacts(list);
            }
        } else {
            if (list == NULL) {
                cons_show("You have no contacts.");
            } else {
                cons_show("All contacts:");
                cons_show_contacts(list);
            }
        }

    // available
    } else if (strcmp("available", presence) == 0) {
        GSList *filtered = NULL;

        GSList *curr = list;
        while (curr != NULL) {
            PContact contact = curr->data;
            if (p_contact_is_available(contact)) {
                filtered = g_slist_append(filtered, contact);
            }
            curr = g_slist_next(curr);
        }

        if (group != NULL) {
            if (filtered == NULL) {
                cons_show("No contacts in group %s are %s.", group, presence);
            } else {
                cons_show("%s (%s):", group, presence);
                cons_show_contacts(filtered);
            }
        } else {
            if (filtered == NULL) {
                cons_show("No contacts are %s.", presence);
            } else {
                cons_show("Contacts (%s):", presence);
                cons_show_contacts(filtered);
            }
        }
        g_slist_free(filtered);

    // unavailable
    } else if (strcmp("unavailable", presence) == 0) {
        GSList *filtered = NULL;

        GSList *curr = list;
        while (curr != NULL) {
            PContact contact = curr->data;
            if (!p_contact_is_available(contact)) {
                filtered = g_slist_append(filtered, contact);
            }
            curr = g_slist_next(curr);
        }

        if (group != NULL) {
            if (filtered == NULL) {
                cons_show("No contacts in group %s are %s.", group, presence);
            } else {
                cons_show("%s (%s):", group, presence);
                cons_show_contacts(filtered);
            }
        } else {
            if (filtered == NULL) {
                cons_show("No contacts are %s.", presence);
            } else {
                cons_show("Contacts (%s):", presence);
                cons_show_contacts(filtered);
            }
        }
        g_slist_free(filtered);

    // online, available resources
    } else if (strcmp("online", presence) == 0) {
        GSList *filtered = NULL;

        GSList *curr = list;
        while (curr != NULL) {
            PContact contact = curr->data;
            if (p_contact_has_available_resource(contact)) {
                filtered = g_slist_append(filtered, contact);
            }
            curr = g_slist_next(curr);
        }

        if (group != NULL) {
            if (filtered == NULL) {
                cons_show("No contacts in group %s are %s.", group, presence);
            } else {
                cons_show("%s (%s):", group, presence);
                cons_show_contacts(filtered);
            }
        } else {
            if (filtered == NULL) {
                cons_show("No contacts are %s.", presence);
            } else {
                cons_show("Contacts (%s):", presence);
                cons_show_contacts(filtered);
            }
        }
        g_slist_free(filtered);

    // offline, no available resources
    } else if (strcmp("offline", presence) == 0) {
        GSList *filtered = NULL;

        GSList *curr = list;
        while (curr != NULL) {
            PContact contact = curr->data;
            if (!p_contact_has_available_resource(contact)) {
                filtered = g_slist_append(filtered, contact);
            }
            curr = g_slist_next(curr);
        }

        if (group != NULL) {
            if (filtered == NULL) {
                cons_show("No contacts in group %s are %s.", group, presence);
            } else {
                cons_show("%s (%s):", group, presence);
                cons_show_contacts(filtered);
            }
        } else {
            if (filtered == NULL) {
                cons_show("No contacts are %s.", presence);
            } else {
                cons_show("Contacts (%s):", presence);
                cons_show_contacts(filtered);
            }
        }
        g_slist_free(filtered);

    // show specific status
    } else {
        GSList *filtered = NULL;

        GSList *curr = list;
        while (curr != NULL) {
            PContact contact = curr->data;
            if (strcmp(p_contact_presence(contact), presence) == 0) {
                filtered = g_slist_append(filtered, contact);
            }
            curr = g_slist_next(curr);
        }

        if (group != NULL) {
            if (filtered == NULL) {
                cons_show("No contacts in group %s are %s.", group, presence);
            } else {
                cons_show("%s (%s):", group, presence);
                cons_show_contacts(filtered);
            }
        } else {
            if (filtered == NULL) {
                cons_show("No contacts are %s.", presence);
            } else {
                cons_show("Contacts (%s):", presence);
                cons_show_contacts(filtered);
            }
        }
        g_slist_free(filtered);
    }

    g_slist_free(list);
}

gboolean
cmd_who(gchar **args, struct cmd_help_t help)
{
    jabber_conn_status_t conn_status = jabber_get_connection_status();
    win_type_t win_type = ui_current_win_type();

    if (conn_status != JABBER_CONNECTED) {
        cons_show("You are not currently connected.");
    } else if (win_type == WIN_MUC) {
        _who_room(args, help);
    } else {
        _who_roster(args, help);
    }

    if (win_type != WIN_CONSOLE && win_type != WIN_MUC) {
        ui_statusbar_new(1);
    }

    return TRUE;
}

gboolean
cmd_msg(gchar **args, struct cmd_help_t help)
{
    char *usr = args[0];
    char *msg = args[1];

    jabber_conn_status_t conn_status = jabber_get_connection_status();
    win_type_t win_type = ui_current_win_type();

    if (conn_status != JABBER_CONNECTED) {
        cons_show("You are not currently connected.");
        return TRUE;
    }

    if (win_type == WIN_MUC) {
        ProfMucWin *mucwin = wins_get_current_muc();
        if (muc_roster_contains_nick(mucwin->roomjid, usr)) {
            GString *full_jid = g_string_new(mucwin->roomjid);
            g_string_append(full_jid, "/");
            g_string_append(full_jid, usr);

            if (msg != NULL) {
                message_send_private(full_jid->str, msg);
                ui_outgoing_private_msg("me", full_jid->str, msg);
            } else {
                ui_new_private_win(full_jid->str);
            }

            g_string_free(full_jid, TRUE);

        } else {
            ui_current_print_line("No such participant \"%s\" in room.", usr);
        }

        return TRUE;

    } else {
        // get barejid
        char *barejid = roster_barejid_from_name(usr);
        if (barejid == NULL) {
            barejid = usr;
        }

        if (msg != NULL) {
#ifdef HAVE_LIBOTR
            if (otr_is_secure(barejid)) {
                char *encrypted = otr_encrypt_message(barejid, msg);
                if (encrypted != NULL) {
                    message_send_chat_encrypted(barejid, encrypted);
                    otr_free_message(encrypted);
                    ui_outgoing_chat_msg("me", barejid, msg);

                    if (((win_type == WIN_CHAT) || (win_type == WIN_CONSOLE)) && prefs_get_boolean(PREF_CHLOG)) {
                        const char *jid = jabber_get_fulljid();
                        Jid *jidp = jid_create(jid);
                        char *pref_otr_log = prefs_get_string(PREF_OTR_LOG);
                        if (strcmp(pref_otr_log, "on") == 0) {
                            chat_log_chat(jidp->barejid, barejid, msg, PROF_OUT_LOG, NULL);
                        } else if (strcmp(pref_otr_log, "redact") == 0) {
                            chat_log_chat(jidp->barejid, barejid, "[redacted]", PROF_OUT_LOG, NULL);
                        }
                        prefs_free_string(pref_otr_log);
                        jid_destroy(jidp);
                    }
                } else {
                    cons_show_error("Failed to encrypt and send message,");
                }
            } else {
                prof_otrpolicy_t policy = otr_get_policy(barejid);

                if (policy == PROF_OTRPOLICY_ALWAYS) {
                    cons_show_error("Failed to send message. Please check OTR policy");
                    return TRUE;
                } else if (policy == PROF_OTRPOLICY_OPPORTUNISTIC) {
                    GString *otr_message = g_string_new(msg);
                    g_string_append(otr_message, OTRL_MESSAGE_TAG_BASE);
                    g_string_append(otr_message, OTRL_MESSAGE_TAG_V2);
                    message_send_chat_encrypted(barejid, otr_message->str);

                    g_string_free(otr_message, TRUE);
                } else {
                    message_send_chat(barejid, msg);
                }
                ui_outgoing_chat_msg("me", barejid, msg);

                if (((win_type == WIN_CHAT) || (win_type == WIN_CONSOLE)) && prefs_get_boolean(PREF_CHLOG)) {
                    const char *jid = jabber_get_fulljid();
                    Jid *jidp = jid_create(jid);
                    chat_log_chat(jidp->barejid, barejid, msg, PROF_OUT_LOG, NULL);
                    jid_destroy(jidp);
                }
            }
            return TRUE;
#else
            message_send_chat(barejid, msg);
            ui_outgoing_chat_msg("me", barejid, msg);

            if (((win_type == WIN_CHAT) || (win_type == WIN_CONSOLE)) && prefs_get_boolean(PREF_CHLOG)) {
                const char *jid = jabber_get_fulljid();
                Jid *jidp = jid_create(jid);
                chat_log_chat(jidp->barejid, barejid, msg, PROF_OUT_LOG, NULL);
                jid_destroy(jidp);
            }
            return TRUE;
#endif

        } else { // msg == NULL
            ui_new_chat_win(barejid);
#ifdef HAVE_LIBOTR
            if (otr_is_secure(barejid)) {
                ui_gone_secure(barejid, otr_is_trusted(barejid));
            }
#endif
            return TRUE;
        }
    }
}

gboolean
cmd_group(gchar **args, struct cmd_help_t help)
{
    jabber_conn_status_t conn_status = jabber_get_connection_status();

    if (conn_status != JABBER_CONNECTED) {
        cons_show("You are not currently connected.");
        return TRUE;
    }

    // list all groups
    if (args[0] == NULL) {
        GSList *groups = roster_get_groups();
        GSList *curr = groups;
        if (curr != NULL) {
            cons_show("Groups:");
            while (curr != NULL) {
                cons_show("  %s", curr->data);
                curr = g_slist_next(curr);
            }

            g_slist_free_full(groups, g_free);
        } else {
            cons_show("No groups.");
        }
        return TRUE;
    }

    // show contacts in group
    if (strcmp(args[0], "show") == 0) {
        char *group = args[1];
        if (group == NULL) {
            cons_show("Usage: %s", help.usage);
            return TRUE;
        }

        GSList *list = roster_get_group(group);
        cons_show_roster_group(group, list);
        return TRUE;
    }

    // add contact to group
    if (strcmp(args[0], "add") == 0) {
        char *group = args[1];
        char *contact = args[2];

        if ((group == NULL) || (contact == NULL)) {
            cons_show("Usage: %s", help.usage);
            return TRUE;
        }

        char *barejid = roster_barejid_from_name(contact);
        if (barejid == NULL) {
            barejid = contact;
        }

        PContact pcontact = roster_get_contact(barejid);
        if (pcontact == NULL) {
            cons_show("Contact not found in roster: %s", barejid);
            return TRUE;
        }

        if (p_contact_in_group(pcontact, group)) {
            const char *display_name = p_contact_name_or_jid(pcontact);
            ui_contact_already_in_group(display_name, group);
        } else {
            roster_send_add_to_group(group, pcontact);
        }

        return TRUE;
    }

    // remove contact from group
    if (strcmp(args[0], "remove") == 0) {
        char *group = args[1];
        char *contact = args[2];

        if ((group == NULL) || (contact == NULL)) {
            cons_show("Usage: %s", help.usage);
            return TRUE;
        }

        char *barejid = roster_barejid_from_name(contact);
        if (barejid == NULL) {
            barejid = contact;
        }

        PContact pcontact = roster_get_contact(barejid);
        if (pcontact == NULL) {
            cons_show("Contact not found in roster: %s", barejid);
            return TRUE;
        }

        if (!p_contact_in_group(pcontact, group)) {
            const char *display_name = p_contact_name_or_jid(pcontact);
            ui_contact_not_in_group(display_name, group);
        } else {
            roster_send_remove_from_group(group, pcontact);
        }

        return TRUE;
    }

    cons_show("Usage: %s", help.usage);
    return TRUE;
}

gboolean
cmd_roster(gchar **args, struct cmd_help_t help)
{
    jabber_conn_status_t conn_status = jabber_get_connection_status();

    if (conn_status != JABBER_CONNECTED) {
        cons_show("You are not currently connected.");
        return TRUE;
    }

    // show roster
    if (args[0] == NULL) {
        GSList *list = roster_get_contacts();
        cons_show_roster(list);
        g_slist_free(list);
        return TRUE;

    // show roster, only online contacts
    } else if(g_strcmp0(args[0], "online") == 0){
        GSList *list = roster_get_contacts_online();
        cons_show_roster(list);
        g_slist_free(list);
        return TRUE;

    // set roster size
    } else if (g_strcmp0(args[0], "size") == 0) {
        int intval = 0;
        if (!args[1]) {
            cons_show("Usage: %s", help.usage);
            return TRUE;
        } else if (_strtoi(args[1], &intval, 1, 99) == 0) {
            prefs_set_roster_size(intval);
            cons_show("Roster screen size set to: %d%%", intval);
            if (prefs_get_boolean(PREF_ROSTER)) {
                wins_resize_all();
            }
            return TRUE;
        } else {
            return TRUE;
        }

    // show/hide roster
    } else if (g_strcmp0(args[0], "show") == 0) {
        if (args[1] == NULL) {
            cons_show("Roster enabled.");
            prefs_set_boolean(PREF_ROSTER, TRUE);
            ui_show_roster();
            return TRUE;
        } else if (g_strcmp0(args[1], "offline") == 0) {
            cons_show("Roster offline enabled");
            prefs_set_boolean(PREF_ROSTER_OFFLINE, TRUE);
            rosterwin_roster();
            return TRUE;
        } else if (g_strcmp0(args[1], "resource") == 0) {
            cons_show("Roster resource enabled");
            prefs_set_boolean(PREF_ROSTER_RESOURCE, TRUE);
            rosterwin_roster();
            return TRUE;
        } else {
            cons_show("Usage: %s", help.usage);
            return TRUE;
        }
    } else if (g_strcmp0(args[0], "hide") == 0) {
        if (args[1] == NULL) {
            cons_show("Roster disabled.");
            prefs_set_boolean(PREF_ROSTER, FALSE);
            ui_hide_roster();
            return TRUE;
        } else if (g_strcmp0(args[1], "offline") == 0) {
            cons_show("Roster offline disabled");
            prefs_set_boolean(PREF_ROSTER_OFFLINE, FALSE);
            rosterwin_roster();
            return TRUE;
        } else if (g_strcmp0(args[1], "resource") == 0) {
            cons_show("Roster resource disabled");
            prefs_set_boolean(PREF_ROSTER_RESOURCE, FALSE);
            rosterwin_roster();
            return TRUE;
        } else {
            cons_show("Usage: %s", help.usage);
            return TRUE;
        }
    // roster grouping
    } else if (g_strcmp0(args[0], "by") == 0) {
        if (g_strcmp0(args[1], "group") == 0) {
            cons_show("Grouping roster by roster group");
            prefs_set_string(PREF_ROSTER_BY, "group");
            rosterwin_roster();
            return TRUE;
        } else if (g_strcmp0(args[1], "presence") == 0) {
            cons_show("Grouping roster by presence");
            prefs_set_string(PREF_ROSTER_BY, "presence");
            rosterwin_roster();
            return TRUE;
        } else if (g_strcmp0(args[1], "none") == 0) {
            cons_show("Roster grouping disabled");
            prefs_set_string(PREF_ROSTER_BY, "none");
            rosterwin_roster();
            return TRUE;
        } else {
            cons_show("Usage: %s", help.usage);
            return TRUE;
        }
    // add contact
    } else if (strcmp(args[0], "add") == 0) {
        char *jid = args[1];
        if (jid == NULL) {
            cons_show("Usage: %s", help.usage);
        } else {
            char *name = args[2];
            roster_send_add_new(jid, name);
        }
        return TRUE;

    // remove contact
    } else if (strcmp(args[0], "remove") == 0) {
        char *jid = args[1];
        if (jid == NULL) {
            cons_show("Usage: %s", help.usage);
        } else {
            roster_send_remove(jid);
        }
        return TRUE;

    // change nickname
    } else if (strcmp(args[0], "nick") == 0) {
        char *jid = args[1];
        if (jid == NULL) {
            cons_show("Usage: %s", help.usage);
            return TRUE;
        }

        char *name = args[2];
        if (name == NULL) {
            cons_show("Usage: %s", help.usage);
            return TRUE;
        }

        // contact does not exist
        PContact contact = roster_get_contact(jid);
        if (contact == NULL) {
            cons_show("Contact not found in roster: %s", jid);
            return TRUE;
        }

        const char *barejid = p_contact_barejid(contact);
        roster_change_name(contact, name);
        GSList *groups = p_contact_groups(contact);
        roster_send_name_change(barejid, name, groups);

        cons_show("Nickname for %s set to: %s.", jid, name);

        return TRUE;

    // remove nickname
    } else if (strcmp(args[0], "clearnick") == 0) {
        char *jid = args[1];
        if (jid == NULL) {
            cons_show("Usage: %s", help.usage);
            return TRUE;
        }

        // contact does not exist
        PContact contact = roster_get_contact(jid);
        if (contact == NULL) {
            cons_show("Contact not found in roster: %s", jid);
            return TRUE;
        }

        const char *barejid = p_contact_barejid(contact);
        roster_change_name(contact, NULL);
        GSList *groups = p_contact_groups(contact);
        roster_send_name_change(barejid, NULL, groups);

        cons_show("Nickname for %s removed.", jid);

        return TRUE;
    } else {
        cons_show("Usage: %s", help.usage);
        return TRUE;
    }
}

gboolean
cmd_resource(gchar **args, struct cmd_help_t help)
{
    char *cmd = args[0];
    char *setting = NULL;
    if (g_strcmp0(cmd, "message") == 0) {
        setting = args[1];
        if (!setting) {
            cons_show("Usage: %s", help.usage);
            return TRUE;
        } else {
            return _cmd_set_boolean_preference(setting, help, "Message resource", PREF_RESOURCE_MESSAGE);
        }
    } else if (g_strcmp0(cmd, "title") == 0) {
        setting = args[1];
        if (!setting) {
            cons_show("Usage: %s", help.usage);
            return TRUE;
        } else {
            return _cmd_set_boolean_preference(setting, help, "Title resource", PREF_RESOURCE_TITLE);
        }
    }

    ProfWin *current = wins_get_current();
    if (current->type != WIN_CHAT) {
        cons_show("Resource can only be changed in chat windows.");
        return TRUE;
    }
    ProfChatWin *chatwin = (ProfChatWin*)current;

    if (g_strcmp0(cmd, "set") == 0) {
        char *resource = args[1];
        if (!resource) {
            cons_show("Usage: %s", help.usage);
            return TRUE;
        }

#ifdef HAVE_LIBOTR
        if (otr_is_secure(chatwin->barejid)) {
            cons_show("Cannot choose resource during an OTR session.");
            return TRUE;
        }
#endif

        PContact contact = roster_get_contact(chatwin->barejid);
        if (!contact) {
            cons_show("Cannot choose resource for contact not in roster.");
            return TRUE;
        }

        if (!p_contact_get_resource(contact, resource)) {
            cons_show("No such resource %s.", resource);
            return TRUE;
        }

        chatwin->resource_override = strdup(resource);
        chat_state_free(chatwin->state);
        chatwin->state = chat_state_new();
        chat_session_resource_override(chatwin->barejid, resource);
        return TRUE;

    } else if (g_strcmp0(cmd, "off") == 0) {
        FREE_SET_NULL(chatwin->resource_override);
        chat_state_free(chatwin->state);
        chatwin->state = chat_state_new();
        chat_session_remove(chatwin->barejid);
        return TRUE;
    } else {
        cons_show("Usage: %s", help.usage);
        return TRUE;
    }
}

gboolean
cmd_status(gchar **args, struct cmd_help_t help)
{
    char *usr = args[0];

    jabber_conn_status_t conn_status = jabber_get_connection_status();
    win_type_t win_type = ui_current_win_type();

    if (conn_status != JABBER_CONNECTED) {
        cons_show("You are not currently connected.");
        return TRUE;
    }

    switch (win_type)
    {
        case WIN_MUC:
            if (usr != NULL) {
                ProfMucWin *mucwin = wins_get_current_muc();
                ProfWin *window = (ProfWin*) mucwin;
                Occupant *occupant = muc_roster_item(mucwin->roomjid, usr);
                if (occupant) {
                    win_show_occupant(window, occupant);
                } else {
                    win_save_vprint(window, '-', NULL, 0, 0, "", "No such participant \"%s\" in room.", usr);
                }
            } else {
                ui_current_print_line("You must specify a nickname.");
            }
            break;
        case WIN_CHAT:
            if (usr != NULL) {
                ui_current_print_line("No parameter required when in chat.");
            } else {
                ProfChatWin *chatwin = wins_get_current_chat();
                ProfWin *window = (ProfWin*) chatwin;
                PContact pcontact = roster_get_contact(chatwin->barejid);
                if (pcontact != NULL) {
                    win_show_contact(window, pcontact);
                } else {
                    win_save_println(window, "Error getting contact info.");
                }
            }
            break;
        case WIN_PRIVATE:
            if (usr != NULL) {
                ui_current_print_line("No parameter required when in chat.");
            } else {
                ProfPrivateWin *privatewin = wins_get_current_private();
                ProfWin *window = (ProfWin*) privatewin;
                Jid *jid = jid_create(privatewin->fulljid);
                Occupant *occupant = muc_roster_item(jid->barejid, jid->resourcepart);
                if (occupant) {
                    win_show_occupant(window, occupant);
                } else {
                    win_save_println(window, "Error getting contact info.");
                }
                jid_destroy(jid);
            }
            break;
        case WIN_CONSOLE:
            if (usr != NULL) {
                char *usr_jid = roster_barejid_from_name(usr);
                if (usr_jid == NULL) {
                    usr_jid = usr;
                }
                cons_show_status(usr_jid);
            } else {
                cons_show("Usage: %s", help.usage);
            }
            break;
        default:
            break;
    }

    return TRUE;
}

gboolean
cmd_info(gchar **args, struct cmd_help_t help)
{
    char *usr = args[0];

    jabber_conn_status_t conn_status = jabber_get_connection_status();
    win_type_t win_type = ui_current_win_type();
    PContact pcontact = NULL;

    if (conn_status != JABBER_CONNECTED) {
        cons_show("You are not currently connected.");
        return TRUE;
    }

    switch (win_type)
    {
        case WIN_MUC:
            if (usr) {
                ProfMucWin *mucwin = wins_get_current_muc();
                Occupant *occupant = muc_roster_item(mucwin->roomjid, usr);
                if (occupant) {
                    ProfWin *current = wins_get_current();
                    win_show_occupant_info(current, mucwin->roomjid, occupant);
                } else {
                    ui_current_print_line("No such occupant \"%s\" in room.", usr);
                }
            } else {
                ProfMucWin *mucwin = wins_get_current_muc();
                iq_room_info_request(mucwin->roomjid);
                ui_show_room_info(mucwin);
                return TRUE;
            }
            break;
        case WIN_CHAT:
            if (usr) {
                ui_current_print_line("No parameter required when in chat.");
            } else {
                ProfChatWin *chatwin = wins_get_current_chat();
                ProfWin *window = (ProfWin*) chatwin;
                PContact pcontact = roster_get_contact(chatwin->barejid);
                if (pcontact != NULL) {
                    win_show_info(window, pcontact);
                } else {
                    win_save_println(window, "Error getting contact info.");
                }
            }
            break;
        case WIN_PRIVATE:
            if (usr) {
                ui_current_print_line("No parameter required when in chat.");
            } else {
                ProfPrivateWin *privatewin = wins_get_current_private();
                ProfWin *window = (ProfWin*) privatewin;
                Jid *jid = jid_create(privatewin->fulljid);
                Occupant *occupant = muc_roster_item(jid->barejid, jid->resourcepart);
                if (occupant) {
                    win_show_occupant_info(window, jid->barejid, occupant);
                } else {
                    win_save_println(window, "Error getting contact info.");
                }
                jid_destroy(jid);
            }
            break;
        case WIN_CONSOLE:
            if (usr) {
                char *usr_jid = roster_barejid_from_name(usr);
                if (usr_jid == NULL) {
                    usr_jid = usr;
                }
                pcontact = roster_get_contact(usr_jid);
                if (pcontact != NULL) {
                    cons_show_info(pcontact);
                } else {
                    cons_show("No such contact \"%s\" in roster.", usr);
                }
            } else {
                cons_show("Usage: %s", help.usage);
            }
            break;
        default:
            break;
    }

    return TRUE;
}

gboolean
cmd_caps(gchar **args, struct cmd_help_t help)
{
    jabber_conn_status_t conn_status = jabber_get_connection_status();
    win_type_t win_type = ui_current_win_type();
    PContact pcontact = NULL;
    Occupant *occupant = NULL;

    if (conn_status != JABBER_CONNECTED) {
        cons_show("You are not currently connected.");
        return TRUE;
    }

    switch (win_type)
    {
        case WIN_MUC:
            if (args[0] != NULL) {
                ProfMucWin *mucwin = wins_get_current_muc();
                occupant = muc_roster_item(mucwin->roomjid, args[0]);
                if (occupant) {
                    Jid *jidp = jid_create_from_bare_and_resource(mucwin->roomjid, args[0]);
                    cons_show_caps(jidp->fulljid, occupant->presence);
                    jid_destroy(jidp);
                } else {
                    cons_show("No such participant \"%s\" in room.", args[0]);
                }
            } else {
                cons_show("No nickname supplied to /caps in chat room.");
            }
            break;
        case WIN_CHAT:
        case WIN_CONSOLE:
            if (args[0] != NULL) {
                Jid *jid = jid_create(args[0]);

                if (jid->fulljid == NULL) {
                    cons_show("You must provide a full jid to the /caps command.");
                } else {
                    pcontact = roster_get_contact(jid->barejid);
                    if (pcontact == NULL) {
                        cons_show("Contact not found in roster: %s", jid->barejid);
                    } else {
                        Resource *resource = p_contact_get_resource(pcontact, jid->resourcepart);
                        if (resource == NULL) {
                            cons_show("Could not find resource %s, for contact %s", jid->barejid, jid->resourcepart);
                        } else {
                            cons_show_caps(jid->fulljid, resource->presence);
                        }
                    }
                }
                jid_destroy(jid);
            } else {
                cons_show("You must provide a jid to the /caps command.");
            }
            break;
        case WIN_PRIVATE:
            if (args[0] != NULL) {
                cons_show("No parameter needed to /caps when in private chat.");
            } else {
                ProfPrivateWin *privatewin = wins_get_current_private();
                Jid *jid = jid_create(privatewin->fulljid);
                if (jid) {
                    occupant = muc_roster_item(jid->barejid, jid->resourcepart);
                    cons_show_caps(jid->resourcepart, occupant->presence);
                    jid_destroy(jid);
                }
            }
            break;
        default:
            break;
    }

    return TRUE;
}


gboolean
cmd_software(gchar **args, struct cmd_help_t help)
{
    jabber_conn_status_t conn_status = jabber_get_connection_status();
    win_type_t win_type = ui_current_win_type();
    Occupant *occupant = NULL;

    if (conn_status != JABBER_CONNECTED) {
        cons_show("You are not currently connected.");
        return TRUE;
    }

    switch (win_type)
    {
        case WIN_MUC:
            if (args[0] != NULL) {
                ProfMucWin *mucwin = wins_get_current_muc();
                occupant = muc_roster_item(mucwin->roomjid, args[0]);
                if (occupant) {
                    Jid *jid = jid_create_from_bare_and_resource(mucwin->roomjid, args[0]);
                    iq_send_software_version(jid->fulljid);
                    jid_destroy(jid);
                } else {
                    cons_show("No such participant \"%s\" in room.", args[0]);
                }
            } else {
                cons_show("No nickname supplied to /software in chat room.");
            }
            break;
        case WIN_CHAT:
        case WIN_CONSOLE:
            if (args[0] != NULL) {
                Jid *jid = jid_create(args[0]);

                if (jid == NULL || jid->fulljid == NULL) {
                    cons_show("You must provide a full jid to the /software command.");
                } else {
                    iq_send_software_version(jid->fulljid);
                }
                jid_destroy(jid);
            } else {
                cons_show("You must provide a jid to the /software command.");
            }
            break;
        case WIN_PRIVATE:
            if (args[0] != NULL) {
                cons_show("No parameter needed to /software when in private chat.");
            } else {
                ProfPrivateWin *privatewin = wins_get_current_private();
                iq_send_software_version(privatewin->fulljid);
            }
            break;
        default:
            break;
    }

    return TRUE;
}

gboolean
cmd_join(gchar **args, struct cmd_help_t help)
{
    jabber_conn_status_t conn_status = jabber_get_connection_status();
    if (conn_status != JABBER_CONNECTED) {
        cons_show("You are not currently connected.");
        return TRUE;
    }

    if (args[0] == NULL) {
        cons_show("Usage: %s", help.usage);
        cons_show("");
        return TRUE;
    }

    Jid *room_arg = jid_create(args[0]);
    if (room_arg == NULL) {
        cons_show_error("Specified room has incorrect format.");
        cons_show("");
        return TRUE;
    }

    char *room = NULL;
    char *nick = NULL;
    char *passwd = NULL;
    GString *room_str = g_string_new("");
    char *account_name = jabber_get_account_name();
    ProfAccount *account = accounts_get_account(account_name);

    // full room jid supplied (room@server)
    if (room_arg->localpart != NULL) {
        room = args[0];

    // server not supplied (room), use account preference
    } else {
        g_string_append(room_str, args[0]);
        g_string_append(room_str, "@");
        g_string_append(room_str, account->muc_service);
        room = room_str->str;
    }

    // Additional args supplied
    gchar *opt_keys[] = { "nick", "password", NULL };
    gboolean parsed;

    GHashTable *options = parse_options(&args[1], opt_keys, &parsed);
    if (!parsed) {
        cons_show("Usage: %s", help.usage);
        cons_show("");
        return TRUE;
    }

    nick = g_hash_table_lookup(options, "nick");
    passwd = g_hash_table_lookup(options, "password");

    options_destroy(options);

    // In the case that a nick wasn't provided by the optional args...
    if (nick == NULL) {
        nick = account->muc_nick;
    }

    if (!muc_active(room)) {
        presence_join_room(room, nick, passwd);
        muc_join(room, nick, passwd, FALSE);
    } else if (muc_roster_complete(room)) {
        ui_switch_to_room(room);
    }

    jid_destroy(room_arg);
    g_string_free(room_str, TRUE);
    account_free(account);

    return TRUE;
}

gboolean
cmd_invite(gchar **args, struct cmd_help_t help)
{
    char *contact = args[0];
    char *reason = args[1];
    jabber_conn_status_t conn_status = jabber_get_connection_status();

    if (conn_status != JABBER_CONNECTED) {
        cons_show("You are not currently connected.");
        return TRUE;
    }

    if (ui_current_win_type() != WIN_MUC) {
        cons_show("You must be in a chat room to send an invite.");
        return TRUE;
    }

    char *usr_jid = roster_barejid_from_name(contact);
    if (usr_jid == NULL) {
        usr_jid = contact;
    }

    ProfMucWin *mucwin = wins_get_current_muc();
    message_send_invite(mucwin->roomjid, usr_jid, reason);
    if (reason != NULL) {
        cons_show("Room invite sent, contact: %s, room: %s, reason: \"%s\".",
            contact, mucwin->roomjid, reason);
    } else {
        cons_show("Room invite sent, contact: %s, room: %s.",
            contact, mucwin->roomjid);
    }

    return TRUE;
}

gboolean
cmd_invites(gchar **args, struct cmd_help_t help)
{
    GSList *invites = muc_invites();
    cons_show_room_invites(invites);
    g_slist_free_full(invites, g_free);
    return TRUE;
}

gboolean
cmd_decline(gchar **args, struct cmd_help_t help)
{
    if (!muc_invites_contain(args[0])) {
        cons_show("No such invite exists.");
    } else {
        muc_invites_remove(args[0]);
        cons_show("Declined invite to %s.", args[0]);
    }

    return TRUE;
}

gboolean
cmd_form_field(char *tag, gchar **args)
{
    ProfWin *current = wins_get_current();
    if (current->type != WIN_MUC_CONFIG) {
        return TRUE;
    }

    ProfMucConfWin *confwin = (ProfMucConfWin*)current;
    DataForm *form = confwin->form;
    if (form) {
        if (!form_tag_exists(form, tag)) {
            ui_current_print_line("Form does not contain a field with tag %s", tag);
            return TRUE;
        }

        form_field_type_t field_type = form_get_field_type(form, tag);
        char *cmd = NULL;
        char *value = NULL;
        gboolean valid = FALSE;
        gboolean added = FALSE;
        gboolean removed = FALSE;

        switch (field_type) {
        case FIELD_BOOLEAN:
            value = args[0];
            if (g_strcmp0(value, "on") == 0) {
                form_set_value(form, tag, "1");
                ui_current_print_line("Field updated...");
                ui_show_form_field(current, form, tag);
            } else if (g_strcmp0(value, "off") == 0) {
                form_set_value(form, tag, "0");
                ui_current_print_line("Field updated...");
                ui_show_form_field(current, form, tag);
            } else {
                ui_current_print_line("Invalid command, usage:");
                ui_show_form_field_help(confwin, tag);
                ui_current_print_line("");
            }
            break;

        case FIELD_TEXT_PRIVATE:
        case FIELD_TEXT_SINGLE:
        case FIELD_JID_SINGLE:
            value = args[0];
            if (value == NULL) {
                ui_current_print_line("Invalid command, usage:");
                ui_show_form_field_help(confwin, tag);
                ui_current_print_line("");
            } else {
                form_set_value(form, tag, value);
                ui_current_print_line("Field updated...");
                ui_show_form_field(current, form, tag);
            }
            break;
        case FIELD_LIST_SINGLE:
            value = args[0];
            if ((value == NULL) || !form_field_contains_option(form, tag, value)) {
                ui_current_print_line("Invalid command, usage:");
                ui_show_form_field_help(confwin, tag);
                ui_current_print_line("");
            } else {
                form_set_value(form, tag, value);
                ui_current_print_line("Field updated...");
                ui_show_form_field(current, form, tag);
            }
            break;

        case FIELD_TEXT_MULTI:
            cmd = args[0];
            if (cmd) {
                value = args[1];
            }
            if ((g_strcmp0(cmd, "add") != 0) && (g_strcmp0(cmd, "remove"))) {
                ui_current_print_line("Invalid command, usage:");
                ui_show_form_field_help(confwin, tag);
                ui_current_print_line("");
                break;
            }
            if (value == NULL) {
                ui_current_print_line("Invalid command, usage:");
                ui_show_form_field_help(confwin, tag);
                ui_current_print_line("");
                break;
            }
            if (g_strcmp0(cmd, "add") == 0) {
                form_add_value(form, tag, value);
                ui_current_print_line("Field updated...");
                ui_show_form_field(current, form, tag);
                break;
            }
            if (g_strcmp0(args[0], "remove") == 0) {
                if (!g_str_has_prefix(value, "val")) {
                    ui_current_print_line("Invalid command, usage:");
                    ui_show_form_field_help(confwin, tag);
                    ui_current_print_line("");
                    break;
                }
                if (strlen(value) < 4) {
                    ui_current_print_line("Invalid command, usage:");
                    ui_show_form_field_help(confwin, tag);
                    ui_current_print_line("");
                    break;
                }

                int index = strtol(&value[3], NULL, 10);
                if ((index < 1) || (index > form_get_value_count(form, tag))) {
                    ui_current_print_line("Invalid command, usage:");
                    ui_show_form_field_help(confwin, tag);
                    ui_current_print_line("");
                    break;
                }

                removed = form_remove_text_multi_value(form, tag, index);
                if (removed) {
                    ui_current_print_line("Field updated...");
                    ui_show_form_field(current, form, tag);
                } else {
                    ui_current_print_line("Could not remove %s from %s", value, tag);
                }
            }
            break;
        case FIELD_LIST_MULTI:
            cmd = args[0];
            if (cmd) {
                value = args[1];
            }
            if ((g_strcmp0(cmd, "add") != 0) && (g_strcmp0(cmd, "remove"))) {
                ui_current_print_line("Invalid command, usage:");
                ui_show_form_field_help(confwin, tag);
                ui_current_print_line("");
                break;
            }
            if (value == NULL) {
                ui_current_print_line("Invalid command, usage:");
                ui_show_form_field_help(confwin, tag);
                ui_current_print_line("");
                break;
            }
            if (g_strcmp0(args[0], "add") == 0) {
                valid = form_field_contains_option(form, tag, value);
                if (valid) {
                    added = form_add_unique_value(form, tag, value);
                    if (added) {
                        ui_current_print_line("Field updated...");
                        ui_show_form_field(current, form, tag);
                    } else {
                        ui_current_print_line("Value %s already selected for %s", value, tag);
                    }
                } else {
                    ui_current_print_line("Invalid command, usage:");
                    ui_show_form_field_help(confwin, tag);
                    ui_current_print_line("");
                }
                break;
            }
            if (g_strcmp0(args[0], "remove") == 0) {
                valid = form_field_contains_option(form, tag, value);
                if (valid == TRUE) {
                    removed = form_remove_value(form, tag, value);
                    if (removed) {
                        ui_current_print_line("Field updated...");
                        ui_show_form_field(current, form, tag);
                    } else {
                        ui_current_print_line("Value %s is not currently set for %s", value, tag);
                    }
                } else {
                    ui_current_print_line("Invalid command, usage:");
                    ui_show_form_field_help(confwin, tag);
                    ui_current_print_line("");
                }
            }
            break;
        case FIELD_JID_MULTI:
            cmd = args[0];
            if (cmd) {
                value = args[1];
            }
            if ((g_strcmp0(cmd, "add") != 0) && (g_strcmp0(cmd, "remove"))) {
                ui_current_print_line("Invalid command, usage:");
                ui_show_form_field_help(confwin, tag);
                ui_current_print_line("");
                break;
            }
            if (value == NULL) {
                ui_current_print_line("Invalid command, usage:");
                ui_show_form_field_help(confwin, tag);
                ui_current_print_line("");
                break;
            }
            if (g_strcmp0(args[0], "add") == 0) {
                added = form_add_unique_value(form, tag, value);
                if (added) {
                    ui_current_print_line("Field updated...");
                    ui_show_form_field(current, form, tag);
                } else {
                    ui_current_print_line("JID %s already exists in %s", value, tag);
                }
                break;
            }
            if (g_strcmp0(args[0], "remove") == 0) {
                removed = form_remove_value(form, tag, value);
                if (removed) {
                    ui_current_print_line("Field updated...");
                    ui_show_form_field(current, form, tag);
                } else {
                    ui_current_print_line("Field %s does not contain %s", tag, value);
                }
            }
            break;

        default:
            break;
        }
    }

    return TRUE;
}

gboolean
cmd_form(gchar **args, struct cmd_help_t help)
{
    jabber_conn_status_t conn_status = jabber_get_connection_status();

    if (conn_status != JABBER_CONNECTED) {
        cons_show("You are not currently connected.");
        return TRUE;
    }

    win_type_t win_type = ui_current_win_type();
    if (win_type != WIN_MUC_CONFIG) {
        cons_show("Command '/form' does not apply to this window.");
        return TRUE;
    }

    if ((g_strcmp0(args[0], "submit") != 0) &&
            (g_strcmp0(args[0], "cancel") != 0) &&
            (g_strcmp0(args[0], "show") != 0) &&
            (g_strcmp0(args[0], "help") != 0)) {
        cons_show("Usage: %s", help.usage);
        return TRUE;
    }

    ProfMucConfWin *confwin = wins_get_current_muc_conf();

    if (g_strcmp0(args[0], "show") == 0) {
        ui_show_form(confwin);
        return TRUE;
    }

    if (g_strcmp0(args[0], "help") == 0) {
        char *tag = args[1];
        if (tag != NULL) {
            ui_show_form_field_help(confwin, tag);
        } else {
            ui_show_form_help(confwin);

            const gchar **help_text = NULL;
            Command *command = g_hash_table_lookup(commands, "/form");

            if (command != NULL) {
                help_text = command->help.long_help;
            }

            ui_show_lines((ProfWin*) confwin, help_text);
        }
        ui_current_print_line("");
        return TRUE;
    }

    if (g_strcmp0(args[0], "submit") == 0) {
        iq_submit_room_config(confwin->roomjid, confwin->form);

    }

    if (g_strcmp0(args[0], "cancel") == 0) {
        iq_room_config_cancel(confwin->roomjid);
    }

    if ((g_strcmp0(args[0], "submit") == 0) || (g_strcmp0(args[0], "cancel") == 0)) {
        if (confwin->form) {
            cmd_autocomplete_remove_form_fields(confwin->form);
        }
        wins_close_current();
        ProfWin *current = (ProfWin*)wins_get_muc(confwin->roomjid);
        if (current == NULL) {
            current = wins_get_console();
        }
        int num = wins_get_num(current);
        ui_switch_win(num);
    }

    return TRUE;
}

gboolean
cmd_kick(gchar **args, struct cmd_help_t help)
{
    jabber_conn_status_t conn_status = jabber_get_connection_status();

    if (conn_status != JABBER_CONNECTED) {
        cons_show("You are not currently connected.");
        return TRUE;
    }

    win_type_t win_type = ui_current_win_type();
    if (win_type != WIN_MUC) {
        cons_show("Command '/kick' only applies in chat rooms.");
        return TRUE;
    }

    ProfMucWin *mucwin = wins_get_current_muc();

    char *nick = args[0];
    if (nick) {
        if (muc_roster_contains_nick(mucwin->roomjid, nick)) {
            char *reason = args[1];
            iq_room_kick_occupant(mucwin->roomjid, nick, reason);
        } else {
            win_save_vprint((ProfWin*) mucwin, '!', NULL, 0, 0, "", "Occupant does not exist: %s", nick);
        }
    } else {
        cons_show("Usage: %s", help.usage);
    }

    return TRUE;
}

gboolean
cmd_ban(gchar **args, struct cmd_help_t help)
{
    jabber_conn_status_t conn_status = jabber_get_connection_status();

    if (conn_status != JABBER_CONNECTED) {
        cons_show("You are not currently connected.");
        return TRUE;
    }

    win_type_t win_type = ui_current_win_type();
    if (win_type != WIN_MUC) {
        cons_show("Command '/ban' only applies in chat rooms.");
        return TRUE;
    }

    ProfMucWin *mucwin = wins_get_current_muc();

    char *jid = args[0];
    if (jid) {
        char *reason = args[1];
        iq_room_affiliation_set(mucwin->roomjid, jid, "outcast", reason);
    } else {
        cons_show("Usage: %s", help.usage);
    }
    return TRUE;
}

gboolean
cmd_subject(gchar **args, struct cmd_help_t help)
{
    jabber_conn_status_t conn_status = jabber_get_connection_status();

    if (conn_status != JABBER_CONNECTED) {
        cons_show("You are not currently connected.");
        return TRUE;
    }

    win_type_t win_type = ui_current_win_type();
    if (win_type != WIN_MUC) {
        cons_show("Command '/room' does not apply to this window.");
        return TRUE;
    }

    ProfMucWin *mucwin = wins_get_current_muc();
    ProfWin *window = (ProfWin*) mucwin;

    if (args[0] == NULL) {
        char *subject = muc_subject(mucwin->roomjid);
        if (subject) {
            win_save_vprint(window, '!', NULL, NO_EOL, THEME_ROOMINFO, "", "Room subject: ");
            win_save_vprint(window, '!', NULL, NO_DATE, 0, "", "%s", subject);
        } else {
            win_save_print(window, '!', NULL, 0, THEME_ROOMINFO, "", "Room has no subject");
        }
        return TRUE;
    }

    if (g_strcmp0(args[0], "set") == 0) {
        if (args[1]) {
            message_send_groupchat_subject(mucwin->roomjid, args[1]);
        } else {
            cons_show("Usage: %s", help.usage);
        }
        return TRUE;
    }

    if (g_strcmp0(args[0], "clear") == 0) {
        message_send_groupchat_subject(mucwin->roomjid, NULL);
        return TRUE;
    }

    cons_show("Usage: %s", help.usage);
    return TRUE;
}

gboolean
cmd_affiliation(gchar **args, struct cmd_help_t help)
{
    jabber_conn_status_t conn_status = jabber_get_connection_status();

    if (conn_status != JABBER_CONNECTED) {
        cons_show("You are not currently connected.");
        return TRUE;
    }

    win_type_t win_type = ui_current_win_type();
    if (win_type != WIN_MUC) {
        cons_show("Command '/affiliation' does not apply to this window.");
        return TRUE;
    }

    char *cmd = args[0];
    if (cmd == NULL) {
        cons_show("Usage: %s", help.usage);
        return TRUE;
    }

    char *affiliation = args[1];
    if ((affiliation != NULL) &&
            (g_strcmp0(affiliation, "owner") != 0) &&
            (g_strcmp0(affiliation, "admin") != 0) &&
            (g_strcmp0(affiliation, "member") != 0) &&
            (g_strcmp0(affiliation, "none") != 0) &&
            (g_strcmp0(affiliation, "outcast") != 0)) {
        cons_show("Usage: %s", help.usage);
        return TRUE;
    }

    ProfMucWin *mucwin = wins_get_current_muc();

    if (g_strcmp0(cmd, "list") == 0) {
        if (!affiliation) {
            iq_room_affiliation_list(mucwin->roomjid, "owner");
            iq_room_affiliation_list(mucwin->roomjid, "admin");
            iq_room_affiliation_list(mucwin->roomjid, "member");
            iq_room_affiliation_list(mucwin->roomjid, "outcast");
        } else if (g_strcmp0(affiliation, "none") == 0) {
            win_save_print((ProfWin*) mucwin, '!', NULL, 0, 0, "", "Cannot list users with no affiliation.");
        } else {
            iq_room_affiliation_list(mucwin->roomjid, affiliation);
        }
        return TRUE;
    }

    if (g_strcmp0(cmd, "set") == 0) {
        if (!affiliation) {
            cons_show("Usage: %s", help.usage);
            return TRUE;
        }

        char *jid = args[2];
        if (jid == NULL) {
            cons_show("Usage: %s", help.usage);
            return TRUE;
        } else {
            char *reason = args[3];
            iq_room_affiliation_set(mucwin->roomjid, jid, affiliation, reason);
            return TRUE;
        }
    }

    cons_show("Usage: %s", help.usage);
    return TRUE;
}

gboolean
cmd_role(gchar **args, struct cmd_help_t help)
{
    jabber_conn_status_t conn_status = jabber_get_connection_status();

    if (conn_status != JABBER_CONNECTED) {
        cons_show("You are not currently connected.");
        return TRUE;
    }

    win_type_t win_type = ui_current_win_type();
    if (win_type != WIN_MUC) {
        cons_show("Command '/role' does not apply to this window.");
        return TRUE;
    }

    char *cmd = args[0];
    if (cmd == NULL) {
        cons_show("Usage: %s", help.usage);
        return TRUE;
    }

    char *role = args[1];
    if ((role != NULL ) &&
            (g_strcmp0(role, "visitor") != 0) &&
            (g_strcmp0(role, "participant") != 0) &&
            (g_strcmp0(role, "moderator") != 0) &&
            (g_strcmp0(role, "none") != 0)) {
        cons_show("Usage: %s", help.usage);
        return TRUE;
    }

    ProfMucWin *mucwin = wins_get_current_muc();

    if (g_strcmp0(cmd, "list") == 0) {
        if (!role) {
            iq_room_role_list(mucwin->roomjid, "moderator");
            iq_room_role_list(mucwin->roomjid, "participant");
            iq_room_role_list(mucwin->roomjid, "visitor");
        } else if (g_strcmp0(role, "none") == 0) {
            win_save_print((ProfWin*) mucwin, '!', NULL, 0, 0, "", "Cannot list users with no role.");
        } else {
            iq_room_role_list(mucwin->roomjid, role);
        }
        return TRUE;
    }

    if (g_strcmp0(cmd, "set") == 0) {
        if (!role) {
            cons_show("Usage: %s", help.usage);
            return TRUE;
        }

        char *nick = args[2];
        if (nick == NULL) {
            cons_show("Usage: %s", help.usage);
            return TRUE;
        } else {
            char *reason = args[3];
            iq_room_role_set(mucwin->roomjid, nick, role, reason);
            return TRUE;
        }
    }

    cons_show("Usage: %s", help.usage);
    return TRUE;
}

gboolean
cmd_room(gchar **args, struct cmd_help_t help)
{
    jabber_conn_status_t conn_status = jabber_get_connection_status();

    if (conn_status != JABBER_CONNECTED) {
        cons_show("You are not currently connected.");
        return TRUE;
    }

    win_type_t win_type = ui_current_win_type();
    if (win_type != WIN_MUC) {
        cons_show("Command '/room' does not apply to this window.");
        return TRUE;
    }

    if ((g_strcmp0(args[0], "accept") != 0) &&
            (g_strcmp0(args[0], "destroy") != 0) &&
            (g_strcmp0(args[0], "config") != 0)) {
        cons_show("Usage: %s", help.usage);
        return TRUE;
    }

    ProfMucWin *mucwin = wins_get_current_muc();
    ProfWin *window = (ProfWin*) mucwin;
    int num = wins_get_num(window);

    int ui_index = num;
    if (ui_index == 10) {
        ui_index = 0;
    }

    if (g_strcmp0(args[0], "accept") == 0) {
        gboolean requires_config = muc_requires_config(mucwin->roomjid);
        if (!requires_config) {
            win_save_print(window, '!', NULL, 0, THEME_ROOMINFO, "", "Current room does not require configuration.");
            return TRUE;
        } else {
            iq_confirm_instant_room(mucwin->roomjid);
            muc_set_requires_config(mucwin->roomjid, FALSE);
            win_save_print(window, '!', NULL, 0, THEME_ROOMINFO, "", "Room unlocked.");
            return TRUE;
        }
    }

    if (g_strcmp0(args[0], "destroy") == 0) {
        iq_destroy_room(mucwin->roomjid);
        return TRUE;
    }

    if (g_strcmp0(args[0], "config") == 0) {
        ProfMucConfWin *confwin = wins_get_muc_conf(mucwin->roomjid);

        if (confwin != NULL) {
            num = wins_get_num(window);
            ui_switch_win(num);
        } else {
            iq_request_room_config_form(mucwin->roomjid);
        }
        return TRUE;
    }

    return TRUE;
}

gboolean
cmd_occupants(gchar **args, struct cmd_help_t help)
{
    jabber_conn_status_t conn_status = jabber_get_connection_status();

    if (conn_status != JABBER_CONNECTED) {
        cons_show("You are not currently connected.");
        return TRUE;
    }

    if (g_strcmp0(args[0], "size") == 0) {
        int intval = 0;
        if (!args[1]) {
            cons_show("Usage: %s", help.usage);
            return TRUE;
        } else if (_strtoi(args[1], &intval, 1, 99) == 0) {
            prefs_set_occupants_size(intval);
            cons_show("Occupants screen size set to: %d%%", intval);
            wins_resize_all();
            return TRUE;
        }
    }

    if (g_strcmp0(args[0], "default") == 0) {
        if (g_strcmp0(args[1], "show") == 0) {
            cons_show("Occupant list enabled.");
            prefs_set_boolean(PREF_OCCUPANTS, TRUE);
            return TRUE;
        } else if (g_strcmp0(args[1], "hide") == 0) {
            cons_show("Occupant list disabled.");
            prefs_set_boolean(PREF_OCCUPANTS, FALSE);
            return TRUE;
        } else {
            cons_show("Usage: %s", help.usage);
            return TRUE;
        }
    }

    win_type_t win_type = ui_current_win_type();
    if (win_type != WIN_MUC) {
        cons_show("Cannot show/hide occupant list when not in chat room.");
        return TRUE;
    }

    ProfMucWin *mucwin = wins_get_current_muc();

    if (g_strcmp0(args[0], "show") == 0) {
        ui_room_show_occupants(mucwin->roomjid);
    } else if (g_strcmp0(args[0], "hide") == 0) {
        ui_room_hide_occupants(mucwin->roomjid);
    } else {
        cons_show("Usage: %s", help.usage);
    }

    return TRUE;
}

gboolean
cmd_rooms(gchar **args, struct cmd_help_t help)
{
    jabber_conn_status_t conn_status = jabber_get_connection_status();

    if (conn_status != JABBER_CONNECTED) {
        cons_show("You are not currently connected.");
        return TRUE;
    }

    if (args[0] == NULL) {
        ProfAccount *account = accounts_get_account(jabber_get_account_name());
        iq_room_list_request(account->muc_service);
        account_free(account);
    } else {
        iq_room_list_request(args[0]);
    }

    return TRUE;
}

gboolean
cmd_bookmark(gchar **args, struct cmd_help_t help)
{
    jabber_conn_status_t conn_status = jabber_get_connection_status();

    if (conn_status != JABBER_CONNECTED) {
        cons_show("You are not currently connected.");
        return TRUE;
    }

    win_type_t win_type = ui_current_win_type();

    gchar *cmd = args[0];
    if (win_type == WIN_MUC && cmd == NULL) {
        // default to current nickname, password, and autojoin "on"
        ProfMucWin *mucwin = wins_get_current_muc();
        char *nick = muc_nick(mucwin->roomjid);
        char *password = muc_password(mucwin->roomjid);
        gboolean added = bookmark_add(mucwin->roomjid, nick, password, "on");
        if (added) {
            ui_current_print_formatted_line('!', 0, "Bookmark added for %s.", mucwin->roomjid);
        } else {
            ui_current_print_formatted_line('!', 0, "Bookmark already exists for %s.", mucwin->roomjid);
        }
        return TRUE;

    } else {
        if (cmd == NULL) {
            cons_show("Usage: %s", help.usage);
            return TRUE;
        }

        if (strcmp(cmd, "list") == 0) {
            const GList *bookmarks = bookmark_get_list();
            cons_show_bookmarks(bookmarks);
        } else {
            char *jid = args[1];
            if (jid == NULL) {
                cons_show("Usage: %s", help.usage);
                cons_show("");
                return TRUE;
            }

            if (strcmp(cmd, "remove") == 0) {
                gboolean removed = bookmark_remove(jid);
                if (removed) {
                    cons_show("Bookmark removed for %s.", jid);
                } else {
                    cons_show("No bookmark exists for %s.", jid);
                }
                return TRUE;
            }

            if (strcmp(cmd, "join") == 0) {
                gboolean joined = bookmark_join(jid);
                if (!joined) {
                    cons_show("No bookmark exists for %s.", jid);
                }
                return TRUE;
            }

            gchar *opt_keys[] = { "autojoin", "nick", "password", NULL };
            gboolean parsed;

            GHashTable *options = parse_options(&args[2], opt_keys, &parsed);
            if (!parsed) {
                cons_show("Usage: %s", help.usage);
                cons_show("");
                return TRUE;
            }

            char *nick = g_hash_table_lookup(options, "nick");
            char *password = g_hash_table_lookup(options, "password");
            char *autojoin = g_hash_table_lookup(options, "autojoin");

            if (autojoin != NULL) {
                if ((strcmp(autojoin, "on") != 0) && (strcmp(autojoin, "off") != 0)) {
                    cons_show("Usage: %s", help.usage);
                    cons_show("");
                    return TRUE;
                }
            }

            if (strcmp(cmd, "add") == 0) {
                if (strchr(jid, '@')==NULL) {
                    cons_show("Can't add bookmark with JID '%s'; should be '%s@domain.tld'", jid, jid);
                } else {
                    gboolean added = bookmark_add(jid, nick, password, autojoin);
                    if (added) {
                        cons_show("Bookmark added for %s.", jid);
                    } else {
                        cons_show("Bookmark already exists, use /bookmark update to edit.");
                    }
                }
            } else if (strcmp(cmd, "update") == 0) {
                gboolean updated = bookmark_update(jid, nick, password, autojoin);
                if (updated) {
                    cons_show("Bookmark updated.");
                } else {
                    cons_show("No bookmark exists for %s.", jid);
                }
            } else {
                cons_show("Usage: %s", help.usage);
            }

            options_destroy(options);
        }
    }

    return TRUE;
}

gboolean
cmd_disco(gchar **args, struct cmd_help_t help)
{
    jabber_conn_status_t conn_status = jabber_get_connection_status();

    if (conn_status != JABBER_CONNECTED) {
        cons_show("You are not currenlty connected.");
        return TRUE;
    }

    GString *jid = g_string_new("");
    if (args[1] != NULL) {
        jid = g_string_append(jid, args[1]);
    } else {
        Jid *jidp = jid_create(jabber_get_fulljid());
        jid = g_string_append(jid, jidp->domainpart);
        jid_destroy(jidp);
    }

    if (g_strcmp0(args[0], "info") == 0) {
        iq_disco_info_request(jid->str);
    } else {
        iq_disco_items_request(jid->str);
    }

    g_string_free(jid, TRUE);

    return TRUE;
}

gboolean
cmd_nick(gchar **args, struct cmd_help_t help)
{
    jabber_conn_status_t conn_status = jabber_get_connection_status();

    if (conn_status != JABBER_CONNECTED) {
        cons_show("You are not currently connected.");
        return TRUE;
    }
    if (ui_current_win_type() != WIN_MUC) {
        cons_show("You can only change your nickname in a chat room window.");
        return TRUE;
    }

    ProfMucWin *mucwin = wins_get_current_muc();
    char *nick = args[0];
    presence_change_room_nick(mucwin->roomjid, nick);

    return TRUE;
}

gboolean
cmd_alias(gchar **args, struct cmd_help_t help)
{
    char *subcmd = args[0];

    if (strcmp(subcmd, "add") == 0) {
        char *alias = args[1];
        if (alias == NULL) {
            cons_show("Usage: %s", help.usage);
            return TRUE;
        } else {
            char *alias_p = alias;
            GString *ac_value = g_string_new("");
            if (alias[0] == '/') {
                g_string_append(ac_value, alias);
                alias_p = &alias[1];
            } else {
                g_string_append(ac_value, "/");
                g_string_append(ac_value, alias);
            }

            char *value = args[2];
            if (value == NULL) {
                cons_show("Usage: %s", help.usage);
                g_string_free(ac_value, TRUE);
                return TRUE;
            } else if (cmd_exists(ac_value->str)) {
                cons_show("Command or alias '%s' already exists.", ac_value->str);
                g_string_free(ac_value, TRUE);
                return TRUE;
            } else {
                prefs_add_alias(alias_p, value);
                cmd_autocomplete_add(ac_value->str);
                cmd_alias_add(alias_p);
                cons_show("Command alias added %s -> %s", ac_value->str, value);
                g_string_free(ac_value, TRUE);
                return TRUE;
            }
        }
    } else if (strcmp(subcmd, "remove") == 0) {
        char *alias = args[1];
        if (alias == NULL) {
            cons_show("Usage: %s", help.usage);
            return TRUE;
        } else {
            if (alias[0] == '/') {
                alias = &alias[1];
            }
            gboolean removed = prefs_remove_alias(alias);
            if (!removed) {
                cons_show("No such command alias /%s", alias);
            } else {
                GString *ac_value = g_string_new("/");
                g_string_append(ac_value, alias);
                cmd_autocomplete_remove(ac_value->str);
                cmd_alias_remove(alias);
                g_string_free(ac_value, TRUE);
                cons_show("Command alias removed -> /%s", alias);
            }
            return TRUE;
        }
    } else if (strcmp(subcmd, "list") == 0) {
        GList *aliases = prefs_get_aliases();
        cons_show_aliases(aliases);
        prefs_free_aliases(aliases);
        return TRUE;
    } else {
        cons_show("Usage: %s", help.usage);
        return TRUE;
    }
}

gboolean
cmd_tiny(gchar **args, struct cmd_help_t help)
{
    char *url = args[0];
    win_type_t win_type = ui_current_win_type();

    if (!tinyurl_valid(url)) {
        GString *error = g_string_new("/tiny, badly formed URL: ");
        g_string_append(error, url);
        cons_show_error(error->str);
        if (win_type != WIN_CONSOLE) {
            ui_current_error_line(error->str);
        }
        g_string_free(error, TRUE);
    } else if (win_type != WIN_CONSOLE) {
        char *tiny = tinyurl_get(url);

        if (tiny != NULL) {
            if (win_type == WIN_CHAT) {
                ProfChatWin *chatwin = wins_get_current_chat();
#ifdef HAVE_LIBOTR
                if (otr_is_secure(chatwin->barejid)) {
                    char *encrypted = otr_encrypt_message(chatwin->barejid, tiny);
                    if (encrypted != NULL) {
                        message_send_chat_encrypted(chatwin->barejid, encrypted);
                        otr_free_message(encrypted);
                        if (prefs_get_boolean(PREF_CHLOG)) {
                            const char *jid = jabber_get_fulljid();
                            Jid *jidp = jid_create(jid);
                            char *pref_otr_log = prefs_get_string(PREF_OTR_LOG);
                            if (strcmp(pref_otr_log, "on") == 0) {
                                chat_log_chat(jidp->barejid, chatwin->barejid, tiny, PROF_OUT_LOG, NULL);
                            } else if (strcmp(pref_otr_log, "redact") == 0) {
                                chat_log_chat(jidp->barejid, chatwin->barejid, "[redacted]", PROF_OUT_LOG, NULL);
                            }
                            prefs_free_string(pref_otr_log);
                            jid_destroy(jidp);
                        }

                        ui_outgoing_chat_msg("me", chatwin->barejid, tiny);
                    } else {
                        cons_show_error("Failed to send message.");
                    }
                } else {
                    message_send_chat(chatwin->barejid, tiny);
                    if (prefs_get_boolean(PREF_CHLOG)) {
                        const char *jid = jabber_get_fulljid();
                        Jid *jidp = jid_create(jid);
                        chat_log_chat(jidp->barejid, chatwin->barejid, tiny, PROF_OUT_LOG, NULL);
                        jid_destroy(jidp);
                    }

                    ui_outgoing_chat_msg("me", chatwin->barejid, tiny);
                }
#else
                message_send_chat(chatwin->barejid, tiny);
                if (prefs_get_boolean(PREF_CHLOG)) {
                    const char *jid = jabber_get_fulljid();
                    Jid *jidp = jid_create(jid);
                    chat_log_chat(jidp->barejid, chatwin->barejid, tiny, PROF_OUT_LOG, NULL);
                    jid_destroy(jidp);
                }

                ui_outgoing_chat_msg("me", chatwin->barejid, tiny);
#endif
            } else if (win_type == WIN_PRIVATE) {
                ProfPrivateWin *privatewin = wins_get_current_private();
                message_send_private(privatewin->fulljid, tiny);
                ui_outgoing_private_msg("me", privatewin->fulljid, tiny);
            } else if (win_type == WIN_MUC) {
                ProfMucWin *mucwin = wins_get_current_muc();
                message_send_groupchat(mucwin->roomjid, tiny);
            }
            free(tiny);
        } else {
            cons_show_error("Couldn't get tinyurl.");
        }
    } else {
        cons_show("/tiny can only be used in chat windows");
    }

    return TRUE;
}

gboolean
cmd_clear(gchar **args, struct cmd_help_t help)
{
    ui_clear_current();
    return TRUE;
}

gboolean
cmd_close(gchar **args, struct cmd_help_t help)
{
    jabber_conn_status_t conn_status = jabber_get_connection_status();
    int index = 0;
    int count = 0;

    if (args[0] == NULL) {
        index = ui_current_win_index();
    } else if (strcmp(args[0], "all") == 0) {
        count = ui_close_all_wins();
        if (count == 0) {
            cons_show("No windows to close.");
        } else if (count == 1) {
            cons_show("Closed 1 window.");
        } else {
            cons_show("Closed %d windows.", count);
        }
        return TRUE;
    } else if (strcmp(args[0], "read") == 0) {
        count = ui_close_read_wins();
        if (count == 0) {
            cons_show("No windows to close.");
        } else if (count == 1) {
            cons_show("Closed 1 window.");
        } else {
            cons_show("Closed %d windows.", count);
        }
        return TRUE;
    } else {
        index = atoi(args[0]);
    }

    if (index < 0 || index == 10) {
        cons_show("No such window exists.");
        return TRUE;
    }

    if (index == 1) {
        cons_show("Cannot close console window.");
        return TRUE;
    }

    if (!ui_win_exists(index)) {
        cons_show("Window is not open.");
        return TRUE;
    }

    // check for unsaved form
    if (ui_win_has_unsaved_form(index)) {
        ProfWin *window = wins_get_current();
        if (wins_is_current(window)) {
            ui_current_print_line("You have unsaved changes, use /form submit or /form cancel");
        } else {
            cons_show("Cannot close form window with unsaved changes, use /form submit or /form cancel");
        }
        return TRUE;
    }

    // handle leaving rooms, or chat
    if (conn_status == JABBER_CONNECTED) {
        ui_close_connected_win(index);
    }

    // close the window
    ui_close_win(index);
    cons_show("Closed window %d", index);

    return TRUE;
}

gboolean
cmd_leave(gchar **args, struct cmd_help_t help)
{
    jabber_conn_status_t conn_status = jabber_get_connection_status();
    win_type_t win_type = ui_current_win_type();
    int index = ui_current_win_index();

    if (win_type != WIN_MUC) {
        cons_show("You can only use the /leave command in a chat room.");
        cons_alert();
        return TRUE;
    }

    // handle leaving rooms, or chat
    if (conn_status == JABBER_CONNECTED) {
        ui_close_connected_win(index);
    }

    // close the window
    ui_close_win(index);

    return TRUE;
}

gboolean
cmd_privileges(gchar **args, struct cmd_help_t help)
{
    gboolean result = _cmd_set_boolean_preference(args[0], help, "MUC privileges", PREF_MUC_PRIVILEGES);

    ui_redraw_all_room_rosters();

    return result;
}

gboolean
cmd_beep(gchar **args, struct cmd_help_t help)
{
    return _cmd_set_boolean_preference(args[0], help, "Sound", PREF_BEEP);
}

gboolean
cmd_presence(gchar **args, struct cmd_help_t help)
{
    return _cmd_set_boolean_preference(args[0], help, "Contact presence", PREF_PRESENCE);
}

gboolean
cmd_wrap(gchar **args, struct cmd_help_t help)
{
    gboolean result = _cmd_set_boolean_preference(args[0], help, "Word wrap", PREF_WRAP);

    wins_resize_all();

    return result;
}

gboolean
cmd_time(gchar **args, struct cmd_help_t help)
{
    if (g_strcmp0(args[0], "minutes") == 0) {
        prefs_set_string(PREF_TIME, "minutes");
        cons_show("Time precision set to minutes.");
        wins_resize_all();
        return TRUE;
    } else if (g_strcmp0(args[0], "seconds") == 0) {
        prefs_set_string(PREF_TIME, "seconds");
        cons_show("Time precision set to seconds.");
        wins_resize_all();
        return TRUE;
    } else if (g_strcmp0(args[0], "off") == 0) {
        prefs_set_string(PREF_TIME, "off");
        cons_show("Time display disabled.");
        wins_resize_all();
        return TRUE;
    } else {
        cons_show("Usage: %s", help.usage);
        return TRUE;
    }
}

gboolean
cmd_states(gchar **args, struct cmd_help_t help)
{
    gboolean result = _cmd_set_boolean_preference(args[0], help, "Sending chat states",
        PREF_STATES);

    // if disabled, disable outtype and gone
    if (result == TRUE && (strcmp(args[0], "off") == 0)) {
        prefs_set_boolean(PREF_OUTTYPE, FALSE);
        prefs_set_gone(0);
    }

    return result;
}

gboolean
cmd_titlebar(gchar **args, struct cmd_help_t help)
{
    if (g_strcmp0(args[0], "show") != 0 && g_strcmp0(args[0], "goodbye") != 0) {
        cons_show("Usage: %s", help.usage);
        return TRUE;
    }
    if (g_strcmp0(args[0], "show") == 0 && g_strcmp0(args[1], "off") == 0) {
        ui_clear_win_title();
    }
    if (g_strcmp0(args[0], "show") == 0) {
        return _cmd_set_boolean_preference(args[1], help, "Titlebar show", PREF_TITLEBAR_SHOW);
    } else {
        return _cmd_set_boolean_preference(args[1], help, "Titlebar goodbye", PREF_TITLEBAR_GOODBYE);
    }
}

gboolean
cmd_outtype(gchar **args, struct cmd_help_t help)
{
    gboolean result = _cmd_set_boolean_preference(args[0], help,
        "Sending typing notifications", PREF_OUTTYPE);

    // if enabled, enable states
    if (result == TRUE && (strcmp(args[0], "on") == 0)) {
        prefs_set_boolean(PREF_STATES, TRUE);
    }

    return result;
}

gboolean
cmd_gone(gchar **args, struct cmd_help_t help)
{
    char *value = args[0];

    gint period = atoi(value);
    prefs_set_gone(period);
    if (period == 0) {
        cons_show("Automatic leaving conversations after period disabled.");
    } else if (period == 1) {
        cons_show("Leaving conversations after 1 minute of inactivity.");
    } else {
        cons_show("Leaving conversations after %d minutes of inactivity.", period);
    }

    // if enabled, enable states
    if (period > 0) {
        prefs_set_boolean(PREF_STATES, TRUE);
    }

    return TRUE;
}


gboolean
cmd_notify(gchar **args, struct cmd_help_t help)
{
    char *kind = args[0];

    // bad kind
    if ((strcmp(kind, "message") != 0) && (strcmp(kind, "typing") != 0) &&
            (strcmp(kind, "remind") != 0) && (strcmp(kind, "invite") != 0) &&
            (strcmp(kind, "sub") != 0) && (strcmp(kind, "room") != 0)) {
        cons_show("Usage: %s", help.usage);

    // set message setting
    } else if (strcmp(kind, "message") == 0) {
        if (strcmp(args[1], "on") == 0) {
            cons_show("Message notifications enabled.");
            prefs_set_boolean(PREF_NOTIFY_MESSAGE, TRUE);
        } else if (strcmp(args[1], "off") == 0) {
            cons_show("Message notifications disabled.");
            prefs_set_boolean(PREF_NOTIFY_MESSAGE, FALSE);
        } else if (strcmp(args[1], "current") == 0) {
            if (g_strcmp0(args[2], "on") == 0) {
                cons_show("Current window message notifications enabled.");
                prefs_set_boolean(PREF_NOTIFY_MESSAGE_CURRENT, TRUE);
            } else if (g_strcmp0(args[2], "off") == 0) {
                cons_show("Current window message notifications disabled.");
                prefs_set_boolean(PREF_NOTIFY_MESSAGE_CURRENT, FALSE);
            } else {
                cons_show("Usage: /notify message current on|off");
            }
        } else if (strcmp(args[1], "text") == 0) {
            if (g_strcmp0(args[2], "on") == 0) {
                cons_show("Showing text in message notifications enabled.");
                prefs_set_boolean(PREF_NOTIFY_MESSAGE_TEXT, TRUE);
            } else if (g_strcmp0(args[2], "off") == 0) {
                cons_show("Showing text in message notifications disabled.");
                prefs_set_boolean(PREF_NOTIFY_MESSAGE_TEXT, FALSE);
            } else {
                cons_show("Usage: /notify message text on|off");
            }
        } else {
            cons_show("Usage: /notify message on|off");
        }

    // set room setting
    } else if (strcmp(kind, "room") == 0) {
        if (strcmp(args[1], "on") == 0) {
            cons_show("Chat room notifications enabled.");
            prefs_set_string(PREF_NOTIFY_ROOM, "on");
        } else if (strcmp(args[1], "off") == 0) {
            cons_show("Chat room notifications disabled.");
            prefs_set_string(PREF_NOTIFY_ROOM, "off");
        } else if (strcmp(args[1], "mention") == 0) {
            cons_show("Chat room notifications enabled on mention.");
            prefs_set_string(PREF_NOTIFY_ROOM, "mention");
        } else if (strcmp(args[1], "current") == 0) {
            if (g_strcmp0(args[2], "on") == 0) {
                cons_show("Current window chat room message notifications enabled.");
                prefs_set_boolean(PREF_NOTIFY_ROOM_CURRENT, TRUE);
            } else if (g_strcmp0(args[2], "off") == 0) {
                cons_show("Current window chat room message notifications disabled.");
                prefs_set_boolean(PREF_NOTIFY_ROOM_CURRENT, FALSE);
            } else {
                cons_show("Usage: /notify room current on|off");
            }
        } else if (strcmp(args[1], "text") == 0) {
            if (g_strcmp0(args[2], "on") == 0) {
                cons_show("Showing text in chat room message notifications enabled.");
                prefs_set_boolean(PREF_NOTIFY_ROOM_TEXT, TRUE);
            } else if (g_strcmp0(args[2], "off") == 0) {
                cons_show("Showing text in chat room message notifications disabled.");
                prefs_set_boolean(PREF_NOTIFY_ROOM_TEXT, FALSE);
            } else {
                cons_show("Usage: /notify room text on|off");
            }
        } else {
            cons_show("Usage: /notify room on|off|mention");
        }

    // set typing setting
    } else if (strcmp(kind, "typing") == 0) {
        if (strcmp(args[1], "on") == 0) {
            cons_show("Typing notifications enabled.");
            prefs_set_boolean(PREF_NOTIFY_TYPING, TRUE);
        } else if (strcmp(args[1], "off") == 0) {
            cons_show("Typing notifications disabled.");
            prefs_set_boolean(PREF_NOTIFY_TYPING, FALSE);
        } else if (strcmp(args[1], "current") == 0) {
            if (g_strcmp0(args[2], "on") == 0) {
                cons_show("Current window typing notifications enabled.");
                prefs_set_boolean(PREF_NOTIFY_TYPING_CURRENT, TRUE);
            } else if (g_strcmp0(args[2], "off") == 0) {
                cons_show("Current window typing notifications disabled.");
                prefs_set_boolean(PREF_NOTIFY_TYPING_CURRENT, FALSE);
            } else {
                cons_show("Usage: /notify typing current on|off");
            }
        } else {
            cons_show("Usage: /notify typing on|off");
        }

    // set invite setting
    } else if (strcmp(kind, "invite") == 0) {
        if (strcmp(args[1], "on") == 0) {
            cons_show("Chat room invite notifications enabled.");
            prefs_set_boolean(PREF_NOTIFY_INVITE, TRUE);
        } else if (strcmp(args[1], "off") == 0) {
            cons_show("Chat room invite notifications disabled.");
            prefs_set_boolean(PREF_NOTIFY_INVITE, FALSE);
        } else {
            cons_show("Usage: /notify invite on|off");
        }

    // set subscription setting
    } else if (strcmp(kind, "sub") == 0) {
        if (strcmp(args[1], "on") == 0) {
            cons_show("Subscription notifications enabled.");
            prefs_set_boolean(PREF_NOTIFY_SUB, TRUE);
        } else if (strcmp(args[1], "off") == 0) {
            cons_show("Subscription notifications disabled.");
            prefs_set_boolean(PREF_NOTIFY_SUB, FALSE);
        } else {
            cons_show("Usage: /notify sub on|off");
        }

    // set remind setting
    } else if (strcmp(kind, "remind") == 0) {
        gint period = atoi(args[1]);
        prefs_set_notify_remind(period);
        if (period == 0) {
            cons_show("Message reminders disabled.");
        } else if (period == 1) {
            cons_show("Message reminder period set to 1 second.");
        } else {
            cons_show("Message reminder period set to %d seconds.", period);
        }

    } else {
        cons_show("Unknown command: %s.", kind);
    }

    return TRUE;
}

gboolean
cmd_inpblock(gchar **args, struct cmd_help_t help)
{
    char *subcmd = args[0];
    char *value = args[1];
    int intval;

    if (g_strcmp0(subcmd, "timeout") == 0) {
        if (value == NULL) {
            cons_show("Usage: %s", help.usage);
            return TRUE;
        }

        if (_strtoi(value, &intval, 1, 1000) == 0) {
            cons_show("Input blocking set to %d milliseconds.", intval);
            prefs_set_inpblock(intval);
            ui_input_nonblocking(FALSE);
        }

        return TRUE;
    }

    if (g_strcmp0(subcmd, "dynamic") == 0) {
        if (value == NULL) {
            cons_show("Usage: %s", help.usage);
            return TRUE;
        }

        if (g_strcmp0(value, "on") != 0 && g_strcmp0(value, "off") != 0) {
            cons_show("Dynamic must be one of 'on' or 'off'");
            return TRUE;
        }

        return _cmd_set_boolean_preference(value, help, "Dynamic input blocking", PREF_INPBLOCK_DYNAMIC);
    }

    cons_show("Usage: %s", help.usage);

    return TRUE;
}

gboolean
cmd_log(gchar **args, struct cmd_help_t help)
{
    char *subcmd = args[0];
    char *value = args[1];
    int intval;

    if (strcmp(subcmd, "maxsize") == 0) {
        if (value == NULL) {
            cons_show("Usage: %s", help.usage);
            return TRUE;
        }
        if (_strtoi(value, &intval, PREFS_MIN_LOG_SIZE, INT_MAX) == 0) {
            prefs_set_max_log_size(intval);
            cons_show("Log maxinum size set to %d bytes", intval);
        }
        return TRUE;
    }

    if (strcmp(subcmd, "rotate") == 0) {
        if (value == NULL) {
            cons_show("Usage: %s", help.usage);
            return TRUE;
        }
        return _cmd_set_boolean_preference(value, help, "Log rotate", PREF_LOG_ROTATE);
    }

    if (strcmp(subcmd, "shared") == 0) {
        if (value == NULL) {
            cons_show("Usage: %s", help.usage);
            return TRUE;
        }
        gboolean result = _cmd_set_boolean_preference(value, help, "Shared log", PREF_LOG_SHARED);
        log_reinit();
        return result;
    }

    if (strcmp(subcmd, "where") == 0) {
        char *logfile = get_log_file_location();
        cons_show("Log file: %s", logfile);
        return TRUE;
    }

    cons_show("Usage: %s", help.usage);

    /* TODO: make 'level' subcommand for debug level */

    return TRUE;
}

gboolean
cmd_reconnect(gchar **args, struct cmd_help_t help)
{
    char *value = args[0];
    int intval;

    if (_strtoi(value, &intval, 0, INT_MAX) == 0) {
        prefs_set_reconnect(intval);
        if (intval == 0) {
            cons_show("Reconnect disabled.", intval);
        } else {
            cons_show("Reconnect interval set to %d seconds.", intval);
        }
    } else {
        cons_show("Usage: %s", help.usage);
    }

    return TRUE;
}

gboolean
cmd_autoping(gchar **args, struct cmd_help_t help)
{
    char *value = args[0];
    int intval;

    if (_strtoi(value, &intval, 0, INT_MAX) == 0) {
        prefs_set_autoping(intval);
        iq_set_autoping(intval);
        if (intval == 0) {
            cons_show("Autoping disabled.", intval);
        } else {
            cons_show("Autoping interval set to %d seconds.", intval);
        }
    } else {
        cons_show("Usage: %s", help.usage);
    }

    return TRUE;
}

gboolean
cmd_ping(gchar **args, struct cmd_help_t help)
{
    jabber_conn_status_t conn_status = jabber_get_connection_status();

    if (conn_status != JABBER_CONNECTED) {
        cons_show("You are not currenlty connected.");
        return TRUE;
    }

    iq_send_ping(args[0]);

    if (args[0] == NULL) {
        cons_show("Pinged server...");
    } else {
        cons_show("Pinged %s...", args[0]);
    }
    return TRUE;
}

gboolean
cmd_autoaway(gchar **args, struct cmd_help_t help)
{
    char *setting = args[0];
    char *value = args[1];
    int minutesval;

    if ((strcmp(setting, "mode") != 0) && (strcmp(setting, "time") != 0) &&
            (strcmp(setting, "message") != 0) && (strcmp(setting, "check") != 0)) {
        cons_show("Setting must be one of 'mode', 'time', 'message' or 'check'");
        return TRUE;
    }

    if (strcmp(setting, "mode") == 0) {
        if ((strcmp(value, "idle") != 0) && (strcmp(value, "away") != 0) &&
                (strcmp(value, "off") != 0)) {
            cons_show("Mode must be one of 'idle', 'away' or 'off'");
        } else {
            prefs_set_string(PREF_AUTOAWAY_MODE, value);
            cons_show("Auto away mode set to: %s.", value);
        }

        return TRUE;
    }

    if (strcmp(setting, "time") == 0) {
        if (_strtoi(value, &minutesval, 1, INT_MAX) == 0) {
            prefs_set_autoaway_time(minutesval);
            cons_show("Auto away time set to: %d minutes.", minutesval);
        }

        return TRUE;
    }

    if (strcmp(setting, "message") == 0) {
        if (strcmp(value, "off") == 0) {
            prefs_set_string(PREF_AUTOAWAY_MESSAGE, NULL);
            cons_show("Auto away message cleared.");
        } else {
            prefs_set_string(PREF_AUTOAWAY_MESSAGE, value);
            cons_show("Auto away message set to: \"%s\".", value);
        }

        return TRUE;
    }

    if (strcmp(setting, "check") == 0) {
        return _cmd_set_boolean_preference(value, help, "Online check",
            PREF_AUTOAWAY_CHECK);
    }

    return TRUE;
}

gboolean
cmd_priority(gchar **args, struct cmd_help_t help)
{
    jabber_conn_status_t conn_status = jabber_get_connection_status();

    if (conn_status != JABBER_CONNECTED) {
        cons_show("You are not currently connected.");
        return TRUE;
    }

    char *value = args[0];
    int intval;

    if (_strtoi(value, &intval, -128, 127) == 0) {
        accounts_set_priority_all(jabber_get_account_name(), intval);
        resource_presence_t last_presence = accounts_get_last_presence(jabber_get_account_name());
        presence_update(last_presence, jabber_get_presence_message(), 0);
        cons_show("Priority set to %d.", intval);
    }

    return TRUE;
}

gboolean
cmd_statuses(gchar **args, struct cmd_help_t help)
{
    if (strcmp(args[0], "console") != 0 &&
            strcmp(args[0], "chat") != 0 &&
            strcmp(args[0], "muc") != 0) {
        cons_show("Usage: %s", help.usage);
        return TRUE;
    }

    if (strcmp(args[1], "all") != 0 &&
            strcmp(args[1], "online") != 0 &&
            strcmp(args[1], "none") != 0) {
        cons_show("Usage: %s", help.usage);
        return TRUE;
    }

    if (strcmp(args[0], "console") == 0) {
        prefs_set_string(PREF_STATUSES_CONSOLE, args[1]);
        if (strcmp(args[1], "all") == 0) {
            cons_show("All presence updates will appear in the console.");
        } else if (strcmp(args[1], "online") == 0) {
            cons_show("Only online/offline presence updates will appear in the console.");
        } else {
            cons_show("Presence updates will not appear in the console.");
        }
    }

    if (strcmp(args[0], "chat") == 0) {
        prefs_set_string(PREF_STATUSES_CHAT, args[1]);
        if (strcmp(args[1], "all") == 0) {
            cons_show("All presence updates will appear in chat windows.");
        } else if (strcmp(args[1], "online") == 0) {
            cons_show("Only online/offline presence updates will appear in chat windows.");
        } else {
            cons_show("Presence updates will not appear in chat windows.");
        }
    }

    if (strcmp(args[0], "muc") == 0) {
        prefs_set_string(PREF_STATUSES_MUC, args[1]);
        if (strcmp(args[1], "all") == 0) {
            cons_show("All presence updates will appear in chat room windows.");
        } else if (strcmp(args[1], "online") == 0) {
            cons_show("Only join/leave presence updates will appear in chat room windows.");
        } else {
            cons_show("Presence updates will not appear in chat room windows.");
        }
    }

    return TRUE;
}

gboolean
cmd_vercheck(gchar **args, struct cmd_help_t help)
{
    int num_args = g_strv_length(args);

    if (num_args == 0) {
        cons_check_version(TRUE);
        return TRUE;
    } else {
        return _cmd_set_boolean_preference(args[0], help,
            "Version checking", PREF_VERCHECK);
    }
}

gboolean
cmd_xmlconsole(gchar **args, struct cmd_help_t help)
{
    if (!ui_xmlconsole_exists()) {
        ui_create_xmlconsole_win();
    } else {
        ui_open_xmlconsole_win();
    }

    return TRUE;
}

gboolean
cmd_flash(gchar **args, struct cmd_help_t help)
{
    return _cmd_set_boolean_preference(args[0], help,
        "Screen flash", PREF_FLASH);
}

gboolean
cmd_intype(gchar **args, struct cmd_help_t help)
{
    return _cmd_set_boolean_preference(args[0], help,
        "Show contact typing", PREF_INTYPE);
}

gboolean
cmd_splash(gchar **args, struct cmd_help_t help)
{
    return _cmd_set_boolean_preference(args[0], help,
        "Splash screen", PREF_SPLASH);
}

gboolean
cmd_autoconnect(gchar **args, struct cmd_help_t help)
{
    if (strcmp(args[0], "off") == 0) {
        prefs_set_string(PREF_CONNECT_ACCOUNT, NULL);
        cons_show("Autoconnect account disabled.");
    } else if (strcmp(args[0], "set") == 0) {
        prefs_set_string(PREF_CONNECT_ACCOUNT, args[1]);
        cons_show("Autoconnect account set to: %s.", args[1]);
    } else {
        cons_show("Usage: %s", help.usage);
    }
    return true;
}

gboolean
cmd_chlog(gchar **args, struct cmd_help_t help)
{
    gboolean result = _cmd_set_boolean_preference(args[0], help,
        "Chat logging", PREF_CHLOG);

    // if set to off, disable history
    if (result == TRUE && (strcmp(args[0], "off") == 0)) {
        prefs_set_boolean(PREF_HISTORY, FALSE);
    }

    return result;
}

gboolean
cmd_grlog(gchar **args, struct cmd_help_t help)
{
    gboolean result = _cmd_set_boolean_preference(args[0], help,
        "Groupchat logging", PREF_GRLOG);

    return result;
}

gboolean
cmd_mouse(gchar **args, struct cmd_help_t help)
{
    return _cmd_set_boolean_preference(args[0], help,
        "Mouse handling", PREF_MOUSE);
}

gboolean
cmd_history(gchar **args, struct cmd_help_t help)
{
    gboolean result = _cmd_set_boolean_preference(args[0], help,
        "Chat history", PREF_HISTORY);

    // if set to on, set chlog
    if (result == TRUE && (strcmp(args[0], "on") == 0)) {
        prefs_set_boolean(PREF_CHLOG, TRUE);
    }

    return result;
}

gboolean
cmd_carbons(gchar **args, struct cmd_help_t help)
{
    jabber_conn_status_t conn_status = jabber_get_connection_status();

    if (conn_status != JABBER_CONNECTED) {
        cons_show("You are not currently connected.");
        return TRUE;
    }

    gboolean result = _cmd_set_boolean_preference(args[0], help,
        "Message carbons preference", PREF_CARBONS);

    // enable carbons
    if (strcmp(args[0], "on") == 0) {
        iq_enable_carbons();
    }
    else if (strcmp(args[0], "off") == 0){
        iq_disable_carbons();
    }
    return result;
}

gboolean
cmd_away(gchar **args, struct cmd_help_t help)
{
    _update_presence(RESOURCE_AWAY, "away", args);
    return TRUE;
}

gboolean
cmd_online(gchar **args, struct cmd_help_t help)
{
    _update_presence(RESOURCE_ONLINE, "online", args);
    return TRUE;
}

gboolean
cmd_dnd(gchar **args, struct cmd_help_t help)
{
    _update_presence(RESOURCE_DND, "dnd", args);
    return TRUE;
}

gboolean
cmd_chat(gchar **args, struct cmd_help_t help)
{
    _update_presence(RESOURCE_CHAT, "chat", args);
    return TRUE;
}

gboolean
cmd_xa(gchar **args, struct cmd_help_t help)
{
    _update_presence(RESOURCE_XA, "xa", args);
    return TRUE;
}

gboolean
cmd_otr(gchar **args, struct cmd_help_t help)
{
#ifdef HAVE_LIBOTR
    if (args[0] == NULL) {
        cons_show("Usage: %s", help.usage);
        return TRUE;
    }

    if (strcmp(args[0], "log") == 0) {
        char *choice = args[1];
        if (g_strcmp0(choice, "on") == 0) {
            prefs_set_string(PREF_OTR_LOG, "on");
            cons_show("OTR messages will be logged as plaintext.");
            if (!prefs_get_boolean(PREF_CHLOG)) {
                cons_show("Chat logging is currently disabled, use '/chlog on' to enable.");
            }
        } else if (g_strcmp0(choice, "off") == 0) {
            prefs_set_string(PREF_OTR_LOG, "off");
            cons_show("OTR message logging disabled.");
        } else if (g_strcmp0(choice, "redact") == 0) {
            prefs_set_string(PREF_OTR_LOG, "redact");
            cons_show("OTR messages will be logged as '[redacted]'.");
            if (!prefs_get_boolean(PREF_CHLOG)) {
                cons_show("Chat logging is currently disabled, use '/chlog on' to enable.");
            }
        } else {
            cons_show("Usage: %s", help.usage);
        }
        return TRUE;

    } else if (strcmp(args[0], "warn") == 0) {
        gboolean result =  _cmd_set_boolean_preference(args[1], help,
            "OTR warning message", PREF_OTR_WARN);
        return result;

    } else if (strcmp(args[0], "libver") == 0) {
        char *version = otr_libotr_version();
        cons_show("Using libotr version %s", version);
        return TRUE;

    } else if (strcmp(args[0], "policy") == 0) {
        if (args[1] == NULL) {
            char *policy = prefs_get_string(PREF_OTR_POLICY);
            cons_show("OTR policy is now set to: %s", policy);
            prefs_free_string(policy);
            return TRUE;
        }

        char *choice = args[1];
        if ((g_strcmp0(choice, "manual") != 0) &&
                (g_strcmp0(choice, "opportunistic") != 0) &&
                (g_strcmp0(choice, "always") != 0)) {
            cons_show("OTR policy can be set to: manual, opportunistic or always.");
            return TRUE;
        }

        char *contact = args[2];
        if (contact == NULL) {
            prefs_set_string(PREF_OTR_POLICY, choice);
            cons_show("OTR policy is now set to: %s", choice);
            return TRUE;
        } else {
            if (jabber_get_connection_status() != JABBER_CONNECTED) {
                cons_show("You must be connected to set the OTR policy for a contact.");
                return TRUE;
            }
            char *contact_jid = roster_barejid_from_name(contact);
            if (contact_jid == NULL) {
                contact_jid = contact;
            }
            accounts_add_otr_policy(jabber_get_account_name(), contact_jid, choice);
            cons_show("OTR policy for %s set to: %s", contact_jid, choice);
            return TRUE;
        }
    }

    if (jabber_get_connection_status() != JABBER_CONNECTED) {
        cons_show("You must be connected with an account to load OTR information.");
        return TRUE;
    }

    if (strcmp(args[0], "gen") == 0) {
        ProfAccount *account = accounts_get_account(jabber_get_account_name());
        otr_keygen(account);
        account_free(account);
        return TRUE;

    } else if (strcmp(args[0], "myfp") == 0) {
        if (!otr_key_loaded()) {
            ui_current_print_formatted_line('!', 0, "You have not generated or loaded a private key, use '/otr gen'");
        } else {
            char *fingerprint = otr_get_my_fingerprint();
            ui_current_print_formatted_line('!', 0, "Your OTR fingerprint: %s", fingerprint);
            free(fingerprint);
        }
        return TRUE;

    } else if (strcmp(args[0], "theirfp") == 0) {
        win_type_t win_type = ui_current_win_type();

        if (win_type != WIN_CHAT) {
            ui_current_print_line("You must be in a regular chat window to view a recipient's fingerprint.");
        } else if (!ui_current_win_is_otr()) {
            ui_current_print_formatted_line('!', 0, "You are not currently in an OTR session.");
        } else {
            ProfChatWin *chatwin = ui_get_current_chat();
            char *fingerprint = otr_get_their_fingerprint(chatwin->barejid);
            ui_current_print_formatted_line('!', 0, "%s's OTR fingerprint: %s", chatwin->barejid, fingerprint);
            free(fingerprint);
        }
        return TRUE;

    } else if (strcmp(args[0], "start") == 0) {
        if (args[1] != NULL) {
            char *contact = args[1];
            char *barejid = roster_barejid_from_name(contact);
            if (barejid == NULL) {
                barejid = contact;
            }

            ui_new_chat_win(barejid);

            if (ui_current_win_is_otr()) {
                ui_current_print_formatted_line('!', 0, "You are already in an OTR session.");
            } else {
                if (!otr_key_loaded()) {
                    ui_current_print_formatted_line('!', 0, "You have not generated or loaded a private key, use '/otr gen'");
                } else if (!otr_is_secure(barejid)) {
                    char *otr_query_message = otr_start_query();
                    message_send_chat_encrypted(barejid, otr_query_message);
                } else {
                    ui_gone_secure(barejid, otr_is_trusted(barejid));
                }
            }
        } else {
            win_type_t win_type = ui_current_win_type();

            if (win_type != WIN_CHAT) {
                ui_current_print_line("You must be in a regular chat window to start an OTR session.");
            } else if (ui_current_win_is_otr()) {
                ui_current_print_formatted_line('!', 0, "You are already in an OTR session.");
            } else {
                if (!otr_key_loaded()) {
                    ui_current_print_formatted_line('!', 0, "You have not generated or loaded a private key, use '/otr gen'");
                } else {
                    ProfChatWin *chatwin = ui_get_current_chat();
                    char *otr_query_message = otr_start_query();
                    message_send_chat_encrypted(chatwin->barejid, otr_query_message);
                }
            }
        }
        return TRUE;

    } else if (strcmp(args[0], "end") == 0) {
        win_type_t win_type = ui_current_win_type();

        if (win_type != WIN_CHAT) {
            ui_current_print_line("You must be in a regular chat window to use OTR.");
        } else if (!ui_current_win_is_otr()) {
            ui_current_print_formatted_line('!', 0, "You are not currently in an OTR session.");
        } else {
            ProfChatWin *chatwin = wins_get_current_chat();
            ui_gone_insecure(chatwin->barejid);
            otr_end_session(chatwin->barejid);
        }
        return TRUE;

    } else if (strcmp(args[0], "trust") == 0) {
        win_type_t win_type = ui_current_win_type();

        if (win_type != WIN_CHAT) {
            ui_current_print_line("You must be in an OTR session to trust a recipient.");
        } else if (!ui_current_win_is_otr()) {
            ui_current_print_formatted_line('!', 0, "You are not currently in an OTR session.");
        } else {
            ProfChatWin *chatwin = wins_get_current_chat();
            ui_trust(chatwin->barejid);
            otr_trust(chatwin->barejid);
        }
        return TRUE;

    } else if (strcmp(args[0], "untrust") == 0) {
        win_type_t win_type = ui_current_win_type();

        if (win_type != WIN_CHAT) {
            ui_current_print_line("You must be in an OTR session to untrust a recipient.");
        } else if (!ui_current_win_is_otr()) {
            ui_current_print_formatted_line('!', 0, "You are not currently in an OTR session.");
        } else {
            ProfChatWin *chatwin = wins_get_current_chat();
            ui_untrust(chatwin->barejid);
            otr_untrust(chatwin->barejid);
        }
        return TRUE;

    } else if (strcmp(args[0], "secret") == 0) {
        win_type_t win_type = ui_current_win_type();
        if (win_type != WIN_CHAT) {
            ui_current_print_line("You must be in an OTR session to trust a recipient.");
        } else if (!ui_current_win_is_otr()) {
            ui_current_print_formatted_line('!', 0, "You are not currently in an OTR session.");
        } else {
            char *secret = args[1];
            if (secret == NULL) {
                cons_show("Usage: %s", help.usage);
            } else {
                ProfChatWin *chatwin = wins_get_current_chat();
                otr_smp_secret(chatwin->barejid, secret);
            }
        }
        return TRUE;

    } else if (strcmp(args[0], "question") == 0) {
        char *question = args[1];
        char *answer = args[2];

        if (question == NULL || answer == NULL) {
            cons_show("Usage: %s", help.usage);
            return TRUE;
        } else {
            win_type_t win_type = ui_current_win_type();
            if (win_type != WIN_CHAT) {
                ui_current_print_line("You must be in an OTR session to trust a recipient.");
            } else if (!ui_current_win_is_otr()) {
                ui_current_print_formatted_line('!', 0, "You are not currently in an OTR session.");
            } else {
                ProfChatWin *chatwin = wins_get_current_chat();
                otr_smp_question(chatwin->barejid, question, answer);
            }
            return TRUE;
        }

    } else if (strcmp(args[0], "answer") == 0) {
        win_type_t win_type = ui_current_win_type();
        if (win_type != WIN_CHAT) {
            ui_current_print_line("You must be in an OTR session to trust a recipient.");
        } else if (!ui_current_win_is_otr()) {
            ui_current_print_formatted_line('!', 0, "You are not currently in an OTR session.");
        } else {
            char *answer = args[1];
            if (answer == NULL) {
                cons_show("Usage: %s", help.usage);
            } else {
                ProfChatWin *chatwin = wins_get_current_chat();
                otr_smp_answer(chatwin->barejid, answer);
            }
        }
        return TRUE;

    } else {
        cons_show("Usage: %s", help.usage);
        return TRUE;
    }
#else
    cons_show("This version of Profanity has not been built with OTR support enabled");
    return TRUE;
#endif
}

// helper function for status change commands
static void
_update_presence(const resource_presence_t resource_presence,
    const char * const show, gchar **args)
{
    char *msg = NULL;
    int num_args = g_strv_length(args);
    if (num_args == 1) {
        msg = args[0];
    }

    jabber_conn_status_t conn_status = jabber_get_connection_status();

    if (conn_status != JABBER_CONNECTED) {
        cons_show("You are not currently connected.");
    } else {
        presence_update(resource_presence, msg, 0);
        ui_update_presence(resource_presence, msg, show);
    }
}

// helper function for boolean preference commands
static gboolean
_cmd_set_boolean_preference(gchar *arg, struct cmd_help_t help,
    const char * const display, preference_t pref)
{
    GString *enabled = g_string_new(display);
    g_string_append(enabled, " enabled.");

    GString *disabled = g_string_new(display);
    g_string_append(disabled, " disabled.");

    if (arg == NULL) {
        char usage[strlen(help.usage) + 8];
        sprintf(usage, "Usage: %s", help.usage);
        cons_show(usage);
    } else if (strcmp(arg, "on") == 0) {
        cons_show(enabled->str);
        prefs_set_boolean(pref, TRUE);
    } else if (strcmp(arg, "off") == 0) {
        cons_show(disabled->str);
        prefs_set_boolean(pref, FALSE);
    } else {
        char usage[strlen(help.usage) + 8];
        sprintf(usage, "Usage: %s", help.usage);
        cons_show(usage);
    }

    g_string_free(enabled, TRUE);
    g_string_free(disabled, TRUE);

    return TRUE;
}

static int
_strtoi(char *str, int *saveptr, int min, int max)
{
    char *ptr;
    int val;

    errno = 0;
    val = (int)strtol(str, &ptr, 0);
    if (errno != 0 || *str == '\0' || *ptr != '\0') {
        cons_show("Could not convert \"%s\" to a number.", str);
        return -1;
    } else if (val < min || val > max) {
        cons_show("Value %s out of range. Must be in %d..%d.", str, min, max);
        return -1;
    }

    *saveptr = val;

    return 0;
}

static void
_cmd_show_filtered_help(char *heading, gchar *cmd_filter[], int filter_size)
{
    cons_show("");
    cons_show("%s", heading);
    cons_show("");

    GList *ordered_commands = NULL;
    int i;
    for (i = 0; i < filter_size; i++) {
        Command *cmd = g_hash_table_lookup(commands, cmd_filter[i]);
        ordered_commands = g_list_insert_sorted(ordered_commands, cmd, (GCompareFunc)_compare_commands);
    }

    GList *curr = ordered_commands;
    while (curr != NULL) {
        Command *cmd = curr->data;
        cons_show("%-12s: %s", cmd->cmd, cmd->help.short_help);
        curr = g_list_next(curr);
    }
    g_list_free(ordered_commands);
    g_list_free(curr);

    cons_show("");
    cons_show("Use /help [command] without the leading slash, for help on a specific command");
    cons_show("");
}

static
gint _compare_commands(Command *a, Command *b)
{
    const char * utf8_str_a = a->cmd;
    const char * utf8_str_b = b->cmd;

    gchar *key_a = g_utf8_collate_key(utf8_str_a, -1);
    gchar *key_b = g_utf8_collate_key(utf8_str_b, -1);

    gint result = g_strcmp0(key_a, key_b);

    g_free(key_a);
    g_free(key_b);

    return result;
}
