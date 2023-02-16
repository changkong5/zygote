/*
 * Copyright (C) 2006 The Android Open Source Project
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

#ifndef __CUTILS_SOCKETS_H
#define __CUTILS_SOCKETS_H

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define ANDROID_SOCKET_ENV_PREFIX	"ANDROID_SOCKET_"
#define ANDROID_SOCKET_DIR		"/dev/socket"

#ifdef __cplusplus
extern "C" {
#endif

extern int socket_loopback_client(int port, int type);
extern int socket_network_client(const char *host, int port, int type);
extern int socket_loopback_server(int port, int type);
extern int socket_local_server(const char *name, int namespaceId, int type);
extern int socket_local_server_bind(int s, const char *name, int namespaceId);
extern int socket_local_client_connect(int fd, 
        const char *name, int namespaceId, int type);
extern int socket_local_client(const char *name, int namespaceId, int type);
extern int socket_inaddr_any_server(int port, int type);

/*
 * socket_peer_is_trusted - Takes a socket which is presumed to be a
 * connected local socket (e.g. AF_LOCAL) and returns whether the peer
 * (the userid that owns the process on the other end of that socket)
 * is one of the two trusted userids, root or shell.
 *
 * Note: This only works as advertised on the Android OS and always
 * just returns true when called on other operating systems.
 */
extern bool socket_peer_is_trusted(int fd);

#ifdef __cplusplus
}
#endif

#endif /* __CUTILS_SOCKETS_H */ 
