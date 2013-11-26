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
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>

#include <netresolve-backend.h>

#if BYTE_ORDER == BIG_ENDIAN
static const struct in_addr inaddr_loopback = { 0x7f000001 };
#elif BYTE_ORDER == LITTLE_ENDIAN
static const struct in_addr inaddr_loopback = { 0x0100007f };
#else
	#error Neither big endian nor little endian
#endif

void
start(netresolve_backend_t resolver, char **settings)
{
	const char *node = netresolve_backend_get_node(resolver);
	bool ipv4 = !node || !*node || !strcmp(node, "localhost") || !strcmp(node, "localhost4");
	bool ipv6 = !node || !*node || !strcmp(node, "localhost") || !strcmp(node, "localhost6");

	if (!ipv4 && !ipv6) {
		netresolve_backend_failed(resolver);
		return;
	}

	if (ipv4)
		netresolve_backend_add_address(resolver, AF_INET, &inaddr_loopback, 0);
	if (ipv6)
		netresolve_backend_add_address(resolver, AF_INET6, &in6addr_loopback, 0);
	netresolve_backend_finished(resolver);
}
