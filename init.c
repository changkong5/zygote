/*
 * Copyright (C) 2008 The Android Open Source Project
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
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <errno.h>
#include <stdarg.h>
#include <mtd/mtd-user.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "list.h"
#include "util.h"
#include "log.h"
#include "init.h"
#include "parser.h"
#include "init_parser.h"
#include "property_service.h"


static struct action *cur_action = NULL;
static struct command *cur_command = NULL;
static struct listnode *command_queue = NULL;

void notify_service_state(const char *name, const char *state)
{
    char pname[PROP_NAME_MAX];
    int len = strlen(name);
    if ((len + 10) > PROP_NAME_MAX)
        return;
    snprintf(pname, sizeof(pname), "init.svc.%s", name);
    property_set(pname, state);
}

static int have_console;
static char console_name[PROP_VALUE_MAX] = "/dev/console";
static time_t process_needs_restart;

static const char *ENV[32];

/* add_environment - add "key=value" to the current environment */
int add_environment(const char *key, const char *val)
{
    int n;

    for (n = 0; n < 31; n++) {
        if (!ENV[n]) {
            size_t len = strlen(key) + strlen(val) + 2;
            char *entry = malloc(len);
            snprintf(entry, len, "%s=%s", key, val);
            ENV[n] = entry;
            return 0;
        }
    }

    return 1;
}

static void zap_stdio(void)
{
    int fd;
    fd = open("/dev/null", O_RDWR);
    dup2(fd, 0);
    dup2(fd, 1);
    dup2(fd, 2);
    close(fd);
}

static void open_console(void)
{
#if 0
    int fd;
    if ((fd = open(console_name, O_RDWR)) < 0) {
        fd = open("/dev/null", O_RDWR);
    }
    ioctl(fd, TIOCSCTTY, 0);
    dup2(fd, 0);
    dup2(fd, 1);
    dup2(fd, 2);
    close(fd);
#endif	
}

void service_start(struct service *svc, const char *dynamic_args)
{
    pid_t pid;
    struct stat s;
    int needs_console;

    /* starting a service removes it from the disabled or reset
     * state and immediately takes it out of the restarting
     * state if it was in there
     */
    svc->flags &= (~(SVC_DISABLED|SVC_RESTARTING|SVC_RESET|SVC_RESTART));
    svc->time_started = 0;

    /* running processes require no additional work -- if
     * they're in the process of exiting, we've ensured
     * that they will immediately restart on exit, unless
     * they are ONESHOT
     */
    if (svc->flags & SVC_RUNNING) {
        return;
    }

    needs_console = (svc->flags & SVC_CONSOLE) ? 1 : 0;
	
/*	
    if (needs_console && (!have_console)) {
        ERROR("service '%s' requires console\n", svc->name);
        svc->flags |= SVC_DISABLED;
        return;
    }
*/

    if (stat(svc->args[0], &s) != 0) {
        ERROR("cannot find '%s', disabling '%s'\n", svc->args[0], svc->name);
        svc->flags |= SVC_DISABLED;
        return;
    }

    if ((!(svc->flags & SVC_ONESHOT)) && dynamic_args) {
        ERROR("service '%s' must be one-shot to use dynamic args, disabling\n", svc->args[0]);
        svc->flags |= SVC_DISABLED;
        return;
    }

    NOTICE("starting '%s'\n", svc->name);

    pid = fork();

    if (pid == 0) {
        umask(077);

        if (needs_console) {
            open_console();
        } else {
            zap_stdio();
        }

#if 0
        for (int n = 0; svc->args[n]; n++) {
            INFO("args[%d] = '%s'\n", n, svc->args[n]);
        }
        for (int n = 0; ENV[n]; n++) {
            INFO("env[%d] = '%s'\n", n, ENV[n]);
        }
#endif

        setpgid(0, getpid());

        if (!dynamic_args) {
            if (execve(svc->args[0], (char**) svc->args, (char**) ENV) < 0) {
                ERROR("cannot execve('%s'): %s\n", svc->args[0], strerror(errno));
            }
        } else {
            char *arg_ptrs[INIT_PARSER_MAXARGS+1];
            int arg_idx = svc->nargs;
            char *tmp = strdup(dynamic_args);
            char *next = tmp;
            char *bword;

            /* Copy the static arguments */
            memcpy(arg_ptrs, svc->args, (svc->nargs * sizeof(char *)));

            while((bword = strsep(&next, " "))) {
                arg_ptrs[arg_idx++] = bword;
                if (arg_idx == INIT_PARSER_MAXARGS)
                    break;
            }
            arg_ptrs[arg_idx] = '\0';
            execve(svc->args[0], (char**) arg_ptrs, (char**) ENV);
        }
        _exit(127);
    }

    if (pid < 0) {
        ERROR("failed to start '%s'\n", svc->name);
        svc->pid = 0;
        return;
    }

    svc->time_started = gettime();
    svc->pid = pid;
    svc->flags |= SVC_RUNNING;

    if (properties_inited())
        notify_service_state(svc->name, "running");
}

/* The how field should be either SVC_DISABLED, SVC_RESET, or SVC_RESTART */
static void service_stop_or_reset(struct service *svc, int how)
{
    /* The service is still SVC_RUNNING until its process exits, but if it has
     * already exited it shoudn't attempt a restart yet. */
    svc->flags &= (~SVC_RESTARTING);

    if ((how != SVC_DISABLED) && (how != SVC_RESET) && (how != SVC_RESTART)) {
        /* Hrm, an illegal flag.  Default to SVC_DISABLED */
        how = SVC_DISABLED;
    }
	
    /* if the service has not yet started, prevent
	 * it from auto-starting with its class
     */
    if (how == SVC_RESET) {
        svc->flags |= (svc->flags & SVC_RC_DISABLED) ? SVC_DISABLED : SVC_RESET;
    } else {
        svc->flags |= how;
    }

    if (svc->pid) {
        NOTICE("service '%s' is being killed\n", svc->name);
        kill(-svc->pid, SIGKILL);
        notify_service_state(svc->name, "stopping");
    } else {
        notify_service_state(svc->name, "stopped");
    }
}

void service_reset(struct service *svc)
{
    service_stop_or_reset(svc, SVC_RESET);
}

void service_stop(struct service *svc)
{
    service_stop_or_reset(svc, SVC_DISABLED);
}

void service_restart(struct service *svc)
{
    if (svc->flags & SVC_RUNNING) {
        /* Stop, wait, then start the service. */
        service_stop_or_reset(svc, SVC_RESTART);
    } else if (!(svc->flags & SVC_RESTARTING)) {
        /* Just start the service since it's not running. */
        service_start(svc, NULL);
    } /* else: Service is restarting anyways. */
}

void property_changed(const char *name, const char *value)
{
    //if (property_triggers_enabled)
    //    queue_property_triggers(name, value);
}

static void restart_service_if_needed(struct service *svc)
{
    time_t next_start_time = svc->time_started + 5;

    if (next_start_time <= gettime()) {
        svc->flags &= (~SVC_RESTARTING);
        service_start(svc, NULL);
        return;
    }

    if ((next_start_time < process_needs_restart) ||
        (process_needs_restart == 0)) {
        process_needs_restart = next_start_time;
    }
}

void restart_processes(void)
{
    process_needs_restart = 0;
    service_for_each_flags(SVC_RESTARTING,
                           restart_service_if_needed);
}

static struct command *get_first_command(struct action *act)
{
    struct listnode *node;
    node = list_head(&act->commands);
    if (!node || list_empty(&act->commands))
        return NULL;

    return node_to_item(node, struct command, clist);
}

static struct command *get_next_command(struct action *act, struct command *cmd)
{
    struct listnode *node;
    node = cmd->clist.next;
    if (!node)
        return NULL;
    if (node == &act->commands)
        return NULL;

    return node_to_item(node, struct command, clist);
}

static int is_last_command(struct action *act, struct command *cmd)
{
    return (list_tail(&act->commands) == &cmd->clist);
}

int execute_one_command(void)
{
    int ret;

    if (!cur_action || !cur_command || is_last_command(cur_action, cur_command)) {
        cur_action = action_remove_queue_head();
        cur_command = NULL;
        if (!cur_action)
            return 0;
        INFO("processing action %p (%s)\n", cur_action, cur_action->name);
        cur_command = get_first_command(cur_action);
    } else {
        cur_command = get_next_command(cur_action, cur_command);
    }

    if (!cur_command)
        return 0;

    ret = cur_command->func(cur_command->nargs, cur_command->args);
    INFO("command '%s' r=%d\n", cur_command->args[0], ret);
	
	return 1;
}

static int console_init_action(int nargs, char **args)
{
    int fd;

    // snprintf(console_name, sizeof(console_name), "/dev/%s", console);

    fd = open(console_name, O_RDWR);
    if (fd >= 0)
        have_console = 1;
    close(fd);
    
    return 0;
}

