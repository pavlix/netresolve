/* Copyright (c) 2013 Pavel Šimerda, Red Hat, Inc. (psimerda at redhat.com) and others
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <netresolve.h>
#include <netresolve-utils.h>

static void
on_socket(netresolve_t resolver, int sock, void *user_data)
{
	int *psock = user_data;

	if (*psock == -1)
		*psock = sock;
	else
		close(sock);
}

static void
set_flags(int sock, int flags)
{
	fcntl(sock, F_SETFL, (fcntl(sock, F_GETFL, 0) & ~(SOCK_NONBLOCK | SOCK_CLOEXEC)) | flags);
}

int
netresolve_utils_bind(const char *node, const char *service, int family, int socktype, int protocol)
{
	netresolve_t resolver = netresolve_open();
	int sock = -1;
	int flags = socktype & (SOCK_NONBLOCK | SOCK_CLOEXEC);
	int status;

	if (!resolver)
		return -1;

	netresolve_callback_set_bind(resolver, on_socket, &sock);

	socktype &= ~flags;

	status = netresolve_resolve(resolver, node, service, family, socktype, protocol);

	netresolve_close(resolver);

	if (status)
		errno = status;
	return sock;
}

int
netresolve_utils_connect(const char *node, const char *service, int family, int socktype, int protocol)
{
	netresolve_t resolver;
	int sock = -1;
	int flags = socktype & (SOCK_NONBLOCK | SOCK_CLOEXEC);
	int status;

	socktype &= ~flags;

	resolver = netresolve_open();
	if (!resolver)
		return -1;
	netresolve_callback_set_connect(resolver, on_socket, &sock);
	status = netresolve_resolve(resolver, node, service, family, socktype, protocol);
	netresolve_close(resolver);

	if (sock != -1)
		set_flags(sock, flags);

	if (status)
		errno = status;
	return sock;
}
