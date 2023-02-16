/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>

#include "init.h"
#include "parser.h"
#include "init_parser.h"
#include "list.h"
#include "util.h"
#include "log.h"
#include "property_service.h"


static list_declare(service_list);
static list_declare(action_list);
static list_declare(action_queue);

struct import {
    struct listnode list;
    const char *filename;
};

// -----------------------------------------------

static void DUMP(void)
{
#if 0
    struct service *svc;
    struct action *act;
    struct command *cmd;
    struct listnode *node;
    struct listnode *node2;
    struct socketinfo *si;
    int n;
    
    list_for_each(node, &service_list) {
        svc = node_to_item(node, struct service, slist);
        RAW("service %s\n", svc->name);
        RAW("  class '%s'\n", svc->classname);
        RAW("  exec");
        for (n = 0; n < svc->nargs; n++) {
            RAW(" '%s'", svc->args[n]);
        }
        RAW("\n");
        for (si = svc->sockets; si; si = si->next) {
            RAW("  socket %s %s 0%o\n", si->name, si->type, si->perm);
        }
    }

    list_for_each(node, &action_list) {
        act = node_to_item(node, struct action, alist);
        RAW("on %s\n", act->name);
        list_for_each(node2, &act->commands) {
            cmd = node_to_item(node2, struct command, clist);
            RAW("  %p", cmd->func);
            for (n = 0; n < cmd->nargs; n++) {
                RAW(" %s", cmd->args[n]);
            }
            RAW("\n");
        }
        RAW("\n");
    }
#endif       
}
// -----------------------------------------------

static void *parse_service(struct parse_state *state, int nargs, char **args);
static void parse_line_service(struct parse_state *state, int nargs, char **args);

static void *parse_action(struct parse_state *state, int nargs, char **args);
static void parse_line_action(struct parse_state *state, int nargs, char **args);

#define SECTION 0x01
#define COMMAND 0x02
#define OPTION  0x04

#include "keywords.h"

#define KEYWORD(symbol, flags, nargs, func) \
    [ K_##symbol ] = { #symbol, func, nargs + 1, flags, },

struct {
    const char *name;
    int (*func)(int nargs, char **args);
    unsigned char nargs;
    unsigned char flags;
} keyword_info[KEYWORD_COUNT] = {
    [ K_UNKNOWN ] = { "unknown", 0, 0, 0 },
#include "keywords.h"
};
#undef KEYWORD

#define kw_is(kw, type) (keyword_info[kw].flags & (type))
#define kw_name(kw) (keyword_info[kw].name)
#define kw_func(kw) (keyword_info[kw].func)
#define kw_nargs(kw) (keyword_info[kw].nargs)

int lookup_keyword(const char *s)
{
    switch (*s++) {
    case 'c':
    if (!strcmp(s, "opy")) return K_copy;
        if (!strcmp(s, "apability")) return K_capability;
        if (!strcmp(s, "hdir")) return K_chdir;
        if (!strcmp(s, "hroot")) return K_chroot;
        if (!strcmp(s, "lass")) return K_class;
        if (!strcmp(s, "lass_start")) return K_class_start;
        if (!strcmp(s, "lass_stop")) return K_class_stop;
        if (!strcmp(s, "lass_reset")) return K_class_reset;
        if (!strcmp(s, "onsole")) return K_console;
        if (!strcmp(s, "hown")) return K_chown;
        if (!strcmp(s, "hmod")) return K_chmod;
        if (!strcmp(s, "ritical")) return K_critical;
        break;
    
    case 'd':
        if (!strcmp(s, "isabled")) return K_disabled;
        break;
    
    case 'e':
        if (!strcmp(s, "xec")) return K_exec;
        if (!strcmp(s, "xport")) return K_export;
        break;
        
    case 'g':
        if (!strcmp(s, "roup")) return K_group;
        break;
        
    case 'h':
        if (!strcmp(s, "ostname")) return K_hostname;
        break;
        
    case 'i':
        if (!strcmp(s, "oprio")) return K_ioprio;
        if (!strcmp(s, "fup")) return K_ifup;
        if (!strcmp(s, "mport")) return K_import;
        break;
        
    case 'k':
        break;
    case 'l':
        break;
    case 'm':
        if (!strcmp(s, "kdir")) return K_mkdir;
        break;

    case 'o':
        if (!strcmp(s, "n")) return K_on;
        if (!strcmp(s, "neshot")) return K_oneshot;
        if (!strcmp(s, "nrestart")) return K_onrestart;
        break;
    case 'p':
        break;
    case 'r':
        if (!strcmp(s, "estart")) return K_restart;
        if (!strcmp(s, "mdir")) return K_rmdir;
        if (!strcmp(s, "m")) return K_rm;
        break;
        
    case 's':
        if (!strcmp(s, "ervice")) return K_service;
        if (!strcmp(s, "etenv")) return K_setenv;
        if (!strcmp(s, "ocket")) return K_socket;
        if (!strcmp(s, "tart")) return K_start;
        if (!strcmp(s, "top")) return K_stop;
        if (!strcmp(s, "ymlink")) return K_symlink;
        break;
        
    case 't':
        if (!strcmp(s, "rigger")) return K_trigger;
        break;
        
    case 'u':
        break;
        
    case 'w':
        if (!strcmp(s, "rite")) return K_write;
        if (!strcmp(s, "ait")) return K_wait;
        break; 
    }
    
    return K_UNKNOWN;
}

void parse_line_no_op(struct parse_state *state, int nargs, char **args)
{
}

static int push_chars(char **dst, int *len, const char *chars, int cnt)
{
    if (cnt > *len)
        return -1;

    memcpy(*dst, chars, cnt);
    *dst += cnt;
    *len -= cnt;

    return 0;
}

int expand_props(char *dst, const char *src, int dst_size)
{
    int cnt = 0;
    char *dst_ptr = dst;
    const char *src_ptr = src;
    int src_len;
    int idx = 0;
    int ret = 0;
    int left = dst_size - 1;

    if (!src || !dst || dst_size == 0)
        return -1;

    src_len = strlen(src);

    /* - variables can either be $x.y or ${x.y}, in case they are only part
     *   of the string.
     * - will accept $$ as a literal $.
     * - no nested property expansion, i.e. ${foo.${bar}} is not supported,
     *   bad things will happen
     */
    while (*src_ptr && left > 0) {
        char *c;
        char prop[PROP_NAME_MAX + 1];
        char prop_val[PROP_VALUE_MAX];
        int prop_len = 0;
        int prop_val_len;

        c = strchr(src_ptr, '$');
        if (!c) {
            while (left-- > 0 && *src_ptr)
                *(dst_ptr++) = *(src_ptr++);
            break;
        }

        memset(prop, 0, sizeof(prop));

        ret = push_chars(&dst_ptr, &left, src_ptr, c - src_ptr);
        if (ret < 0)
            goto err_nospace;
        c++;

        if (*c == '$') {
            *(dst_ptr++) = *(c++);
            src_ptr = c;
            left--;
            continue;
        } else if (*c == '\0') {
            break;
        }

        if (*c == '{') {
            c++;
            while (*c && *c != '}' && prop_len < PROP_NAME_MAX)
                prop[prop_len++] = *(c++);
            if (*c != '}') {
                /* failed to find closing brace, abort. */
                if (prop_len == PROP_NAME_MAX)
                    ERROR("prop name too long during expansion of '%s'\n",
                          src);
                else if (*c == '\0')
                    ERROR("unexpected end of string in '%s', looking for }\n",
                          src);
                goto err;
            }
            prop[prop_len] = '\0';
            c++;
        } else if (*c) {
            while (*c && prop_len < PROP_NAME_MAX)
                prop[prop_len++] = *(c++);
            if (prop_len == PROP_NAME_MAX && *c != '\0') {
                ERROR("prop name too long in '%s'\n", src);
                goto err;
            }
            prop[prop_len] = '\0';
            ERROR("using deprecated syntax for specifying property '%s', use ${name} instead\n",
                  prop);
        }

        if (prop_len == 0) {
            ERROR("invalid zero-length prop name in '%s'\n", src);
            goto err;
        }

        prop_val_len = property_get(prop, prop_val);
        if (!prop_val_len) {
            ERROR("property '%s' doesn't exist while expanding '%s'\n",
                  prop, src);
            goto err;
        }

        ret = push_chars(&dst_ptr, &left, prop_val, prop_val_len);
        if (ret < 0)
            goto err_nospace;
        src_ptr = c;
        continue;
    }

    *dst_ptr = '\0';
    return 0;

err_nospace:
    ERROR("destination buffer overflow while expanding '%s'\n", src);
err:
    return -1;
}

void parse_import(struct parse_state *state, int nargs, char **args)
{
    struct listnode *import_list = state->priv;
    struct import *import;
    char conf_file[PATH_MAX];
    int ret;

    if (nargs != 2) {
        ERROR("single argument needed for import\n");
        return;
    }

    ret = expand_props(conf_file, args[1], sizeof(conf_file));
    if (ret) {
        ERROR("error while handling import on line '%d' in '%s'\n",
              state->line, state->filename);
        return;
    }

    import = calloc(1, sizeof(struct import));
    import->filename = strdup(conf_file);
    list_add_tail(import_list, &import->list);
    INFO("found import '%s', adding to import list\n", import->filename);
}

void parse_new_section(struct parse_state *state, int kw,
                       int nargs, char **args)
{
    printf("[ %s %s ]\n", args[0],
           nargs > 1 ? args[1] : "");
    switch(kw) {
    case K_service:
        state->context = parse_service(state, nargs, args);
        if (state->context) {
            state->parse_line = parse_line_service;
            return;
        }
        break;
    case K_on:
        state->context = parse_action(state, nargs, args);
        if (state->context) {
            state->parse_line = parse_line_action;
            return;
        }
        break;
    case K_import:
        parse_import(state, nargs, args);
        break;
    }
    state->parse_line = parse_line_no_op;
}

static void parse_config(const char *fn, char *s)
{
    struct parse_state state;
    struct listnode import_list;
    struct listnode *node;
    char *args[INIT_PARSER_MAXARGS];
    int nargs;

    nargs = 0;
    state.filename = fn;
    state.line = 0;
    state.ptr = s;
    state.nexttoken = 0;
    state.parse_line = parse_line_no_op;

    list_init(&import_list);
    state.priv = &import_list;

    for (;;) {
        switch (next_token(&state)) {
        case T_EOF:
            state.parse_line(&state, 0, 0);
            goto parser_done;
        case T_NEWLINE:
            state.line++;
            if (nargs) {
                int kw = lookup_keyword(args[0]);
                if (kw_is(kw, SECTION)) {
                    state.parse_line(&state, 0, 0);
                    parse_new_section(&state, kw, nargs, args);
                } else {
                    state.parse_line(&state, nargs, args);
                }
                nargs = 0;
            }
            break;
        case T_TEXT:
            if (nargs < INIT_PARSER_MAXARGS) {
                args[nargs++] = state.text;
            }
            break;
        }
    }

parser_done:
    list_for_each(node, &import_list) {
         struct import *import = node_to_item(node, struct import, list);
         int ret;

         INFO("importing '%s'\n", import->filename);
         ret = init_parse_config_file(import->filename);
         if (ret)
             ERROR("could not import file '%s' from '%s'\n",
                   import->filename, fn);
    }
}

int init_parse_config_file(const char *fn)
{
    char *data;
    data = read_file(fn, 0);
    if (!data) return -1;

    parse_config(fn, data);
    DUMP();
    return 0;
}

static int valid_name(const char *name)
{
    if (strlen(name) > 16) {
        return 0;
    }
    while (*name) {
        if (!isalnum(*name) && (*name != '_') && (*name != '-')) {
            return 0;
        }
        name++;
    }
    return 1;
}

struct service *service_find_by_name(const char *name)
{
    struct listnode *node;
    struct service *svc;
    list_for_each(node, &service_list) {
        svc = node_to_item(node, struct service, slist);
        if (!strcmp(svc->name, name)) {
            return svc;
        }
    }
    return 0;
}

struct service *service_find_by_pid(pid_t pid)
{
    struct listnode *node;
    struct service *svc;
    list_for_each(node, &service_list) {
        svc = node_to_item(node, struct service, slist);
        if (svc->pid == pid) {
            return svc;
        }
    }
    return 0;
}

void service_for_each(void (*func)(struct service *svc))
{
    struct listnode *node;
    struct service *svc;
    list_for_each(node, &service_list) {
        svc = node_to_item(node, struct service, slist);
        func(svc);
    }
}

void service_for_each_class(const char *classname,
                            void (*func)(struct service *svc))
{
    struct listnode *node;
    struct service *svc;
    list_for_each(node, &service_list) {
        svc = node_to_item(node, struct service, slist);
        if (!strcmp(svc->classname, classname)) {
            func(svc);
        }
    }
}

void service_for_each_flags(unsigned matchflags,
                            void (*func)(struct service *svc))
{
    struct listnode *node;
    struct service *svc;
    list_for_each(node, &service_list) {
        svc = node_to_item(node, struct service, slist);
        if (svc->flags & matchflags) {
            func(svc);
        }
    }
}

void action_for_each_trigger(const char *trigger,
                             void (*func)(struct action *act))
{
    struct listnode *node;
    struct action *act;
    list_for_each(node, &action_list) {
        act = node_to_item(node, struct action, alist);
        if (!strcmp(act->name, trigger)) {
            func(act);
        }
    }
}

void queue_property_triggers(const char *name, const char *value)
{
    struct listnode *node;
    struct action *act;
    list_for_each(node, &action_list) {
        act = node_to_item(node, struct action, alist);
        if (!strncmp(act->name, "property:", strlen("property:"))) {
            const char *test = act->name + strlen("property:");
            int name_length = strlen(name);

            if (!strncmp(name, test, name_length) &&
                    test[name_length] == '=' &&
                    (!strcmp(test + name_length + 1, value) ||
                     !strcmp(test + name_length + 1, "*"))) {
                action_add_queue_tail(act);
            }
        }
    }
}

void queue_all_property_triggers(void)
{
    struct listnode *node;
    struct action *act;
    list_for_each(node, &action_list) {
        act = node_to_item(node, struct action, alist);
        if (!strncmp(act->name, "property:", strlen("property:"))) {
            /* parse property name and value
               syntax is property:<name>=<value> */
            const char* name = act->name + strlen("property:");
            const char* equals = strchr(name, '=');
            if (equals) {
                char prop_name[PROP_NAME_MAX + 1];
                char value[PROP_VALUE_MAX];
                int length = equals - name;
                if (length > PROP_NAME_MAX) {
                    ERROR("property name too long in trigger %s", act->name);
                } else {
                    memcpy(prop_name, name, length);
                    prop_name[length] = 0;

                    /* does the property exist, and match the trigger value? */
                    property_get(prop_name, value);
                    if (!strcmp(equals + 1, value) ||!strcmp(equals + 1, "*")) {
                        action_add_queue_tail(act);
                    }
                }
            }
        }
    }
}

void queue_builtin_action(int (*func)(int nargs, char **args), char *name)
{
    struct action *act;
    struct command *cmd;

    act = calloc(1, sizeof(*act));
    act->name = name;
    list_init(&act->commands);
    list_init(&act->qlist);

    cmd = calloc(1, sizeof(*cmd));
    cmd->func = func;
    cmd->args[0] = name;
    list_add_tail(&act->commands, &cmd->clist);

    list_add_tail(&action_list, &act->alist);
    action_add_queue_tail(act);
}

void action_add_queue_tail(struct action *act)
{
    if (list_empty(&act->qlist)) {
        list_add_tail(&action_queue, &act->qlist);
    }
}

struct action *action_remove_queue_head(void)
{
    if (list_empty(&action_queue)) {
        return 0;
    } else {
        struct listnode *node = list_head(&action_queue);
        struct action *act = node_to_item(node, struct action, qlist);
        list_remove(node);
        list_init(node);
        return act;
    }
}

int action_queue_empty(void)
{
    return list_empty(&action_queue);
}

static void *parse_service(struct parse_state *state, int nargs, char **args)
{
    struct service *svc;
    if (nargs < 3) {
        parse_error(state, "services must have a name and a program\n");
        return 0;
    }
    if (!valid_name(args[1])) {
        parse_error(state, "invalid service name '%s'\n", args[1]);
        return 0;
    }

    svc = service_find_by_name(args[1]);
    if (svc) {
        parse_error(state, "ignored duplicate definition of service '%s'\n", args[1]);
        return 0;
    }

    nargs -= 2;
    svc = calloc(1, sizeof(*svc) + sizeof(char*) * nargs);
    if (!svc) {
        parse_error(state, "out of memory\n");
        return 0;
    }
    svc->name = args[1];
    svc->classname = "default";
    memcpy(svc->args, args + 2, sizeof(char*) * nargs);
    svc->args[nargs] = 0;
    svc->nargs = nargs;
    svc->onrestart.name = "onrestart";
    list_init(&svc->onrestart.commands);
    list_add_tail(&service_list, &svc->slist);
    return svc;
}

static void parse_line_service(struct parse_state *state, int nargs, char **args)
{
    struct service *svc = state->context;
    struct command *cmd;
    int i, kw, kw_nargs;

    if (nargs == 0) {
        return;
    }

    kw = lookup_keyword(args[0]);
    switch (kw) {
    case K_capability:
        break;
    case K_class:
        if (nargs != 2) {
            parse_error(state, "class option requires a classname\n");
        } else {
            svc->classname = args[1];
        }
        break;
    case K_console:
        svc->flags |= SVC_CONSOLE;
        break;
    case K_disabled:
        svc->flags |= SVC_DISABLED;
        svc->flags |= SVC_RC_DISABLED;
        break;
    case K_ioprio:
        break;
    case K_group:
        break;
    case K_keycodes:
        break;
    case K_oneshot:
        svc->flags |= SVC_ONESHOT;
        break;
    case K_onrestart:
        nargs--;
        args++;
        kw = lookup_keyword(args[0]);
        if (!kw_is(kw, COMMAND)) {
            parse_error(state, "invalid command '%s'\n", args[0]);
            break;
        }
        kw_nargs = kw_nargs(kw);
        if (nargs < kw_nargs) {
            parse_error(state, "%s requires %d %s\n", args[0], kw_nargs - 1,
                kw_nargs > 2 ? "arguments" : "argument");
            break;
        }

        cmd = malloc(sizeof(*cmd) + sizeof(char*) * nargs);
        cmd->func = kw_func(kw);
        cmd->nargs = nargs;
        memcpy(cmd->args, args, sizeof(char*) * nargs);
        list_add_tail(&svc->onrestart.commands, &cmd->clist);
        break;
    case K_critical:
        svc->flags |= SVC_CRITICAL;
        break;
    case K_setenv: 
		break;
    case K_socket: 
		break;
    case K_user:
        break;
    case K_seclabel:
        break;

    default:
        parse_error(state, "invalid option '%s'\n", args[0]);
    }
}


static void *parse_action(struct parse_state *state, int nargs, char **args)
{
    struct action *act;
    if (nargs < 2) {
        parse_error(state, "actions must have a trigger\n");
        return 0;
    }
    if (nargs > 2) {
        parse_error(state, "actions may not have extra parameters\n");
        return 0;
    }
    act = calloc(1, sizeof(*act));
    act->name = args[1];
    list_init(&act->commands);
    list_init(&act->qlist);
    list_add_tail(&action_list, &act->alist);
        /* XXX add to hash */
    return act;
}

static void parse_line_action(struct parse_state* state, int nargs, char **args)
{
    struct command *cmd;
    struct action *act = state->context;
    int (*func)(int nargs, char **args);
    int kw, n;

    if (nargs == 0) {
        return;
    }

    kw = lookup_keyword(args[0]);
    if (!kw_is(kw, COMMAND)) {
        parse_error(state, "invalid command '%s'\n", args[0]);
        return;
    }

    n = kw_nargs(kw);
    if (nargs < n) {
        parse_error(state, "%s requires %d %s\n", args[0], n - 1,
            n > 2 ? "arguments" : "argument");
        return;
    }
    cmd = malloc(sizeof(*cmd) + sizeof(char*) * nargs);
    cmd->func = kw_func(kw);
    cmd->nargs = nargs;
    memcpy(cmd->args, args, sizeof(char*) * nargs);
    list_add_tail(&act->commands, &cmd->clist);
}

