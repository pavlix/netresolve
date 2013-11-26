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
#include <unistd.h>
#include <signal.h>
#include <stdio.h>

#include <netresolve-backend.h>
#include <netresolve-string.h>

struct buffer {
	char *buffer;
	char *start;
	char *end;
};

struct priv_exec {
	int pid;
	struct buffer inbuf;
	int infd;
	struct buffer outbuf;
	int outfd;
};

static bool
start_subprocess(char *const command[], int *pid, int *infd, int *outfd)
{
	int p1[2], p2[2];

	if (!pid || !infd || !outfd)
		return false;

	if (pipe(p1) == -1)
		goto err_pipe1;
	if (pipe(p2) == -1)
		goto err_pipe2;
	if ((*pid = fork()) == -1)
		goto err_fork;

	if (*pid) {
		*infd = p1[1];
		*outfd = p2[0];
		close(p1[0]);
		close(p2[1]);
		return true;
	} else {
		dup2(p1[0], 0);
		dup2(p2[1], 1);
		close(p1[1]);
		close(p2[0]);
		execvp(*command, command);
		/* Subprocess error occured. */
		fprintf(stderr, "error running %s: %s\n", *command, strerror(errno));
		abort();
	}

err_fork:
	close(p2[1]);
	close(p2[0]);
err_pipe2:
	close(p1[1]);
	close(p1[0]);
err_pipe1:
	return false;
}

static void
send_stdin(netresolve_backend_t resolver, struct priv_exec *priv)
{
	if (priv->inbuf.start != priv->inbuf.end) {
		ssize_t size = write(priv->infd, priv->inbuf.start, priv->inbuf.end - priv->inbuf.start);
		if (size > 0) {
			priv->inbuf.start += size;
			return;
		}
		debug("write failed: %s", strerror(errno));
	}

	netresolve_backend_watch_fd(resolver, priv->infd, 0);
	close(priv->infd);
	priv->infd = -1;
}

static bool
received_line(netresolve_backend_t resolver, struct priv_exec *priv, const char *line)
{
	char addrprefix[] = "address ";
	int addrprefixlen = strlen(addrprefix);
	char pathprefix[] = "path ";
	int pathprefixlen = strlen(pathprefix);
	Address address;
	int family;
	int ifindex;
	int socktype;
	int protocol;
	int port;

	debug("received: %s\n", priv->outbuf.buffer);

	if (!*line)
		return true;

	if (!strncmp(addrprefix, line, addrprefixlen)) {
		if (netresolve_backend_parse_address(line + addrprefixlen,
					&address, &family, &ifindex))
			netresolve_backend_add_address(resolver, family, &address, ifindex);
	} else if (!strncmp(pathprefix, line, pathprefixlen)) {
		if (netresolve_backend_parse_path(line + pathprefixlen,
					&address, &family, &ifindex, &socktype, &protocol, &port))
			netresolve_backend_add_path(resolver, family, &address, ifindex, socktype, protocol, port, 0, 0);
	}

	return false;
}

static void
pickup_stdout(netresolve_backend_t resolver, struct priv_exec *priv)
{
	char *nl;
	int size;

	if (!priv->outbuf.buffer) {
		priv->outbuf.buffer = priv->outbuf.start = malloc(1024);
		if (priv->outbuf.buffer)
			priv->outbuf.end = priv->outbuf.start + 1024;
	}
	if (!priv->outbuf.buffer) {
		netresolve_backend_failed(resolver);
		return;
	}

	size = read(priv->outfd, priv->outbuf.start, priv->outbuf.end - priv->outbuf.start);
	if (size <= 0) {
		abort();
		if (size < 0)
			error("read: %s", strerror(errno));
		netresolve_backend_failed(resolver);
		return;
	}
	debug("read: %*s\n", size, priv->outbuf.start);
	priv->outbuf.start += size;

	while ((nl = memchr(priv->outbuf.buffer, '\n', priv->outbuf.start - priv->outbuf.buffer))) {
		*nl++ = '\0';
		if (received_line(resolver, priv, priv->outbuf.buffer)) {
			netresolve_backend_finished(resolver);
			return;
		}
		memmove(priv->outbuf.buffer, nl, priv->outbuf.end - priv->outbuf.start);
		priv->outbuf.start = priv->outbuf.buffer + (priv->outbuf.start - nl);
	}
}

void
start(netresolve_backend_t resolver, char **settings)
{
	struct priv_exec *priv = netresolve_backend_new_priv(resolver, sizeof *priv);

	priv->infd = -1;
	priv->outfd = -1;

	if (!priv || !start_subprocess(settings, &priv->pid, &priv->infd, &priv->outfd)) {
		netresolve_backend_failed(resolver);
		return;
	}

	priv->inbuf.buffer = strdup(netresolve_get_request_string(resolver));
	priv->inbuf.start = priv->inbuf.buffer;
	priv->inbuf.end = priv->inbuf.buffer + strlen(priv->inbuf.buffer);

	netresolve_backend_watch_fd(resolver, priv->infd, POLLOUT);
	netresolve_backend_watch_fd(resolver, priv->outfd, POLLIN);
}

void
dispatch(netresolve_backend_t resolver, int fd, int events)
{
	struct priv_exec *priv = netresolve_backend_get_priv(resolver);

	debug("exec: events %d on fd %d\n", events, fd);

	if (fd == priv->infd && events & POLLOUT)
		send_stdin(resolver, priv);
	else if (fd == priv->outfd && events & POLLIN) {
		pickup_stdout(resolver, priv);
	} else if (fd == priv->outfd && events & POLLHUP) {
		error("exec: incomplete response\n");
		netresolve_backend_failed(resolver);
	} else {
		error("exec: unknown events %d on fd %d\n", events, fd);
		netresolve_backend_failed(resolver);
	}
}

void
cleanup(netresolve_backend_t resolver)
{
	struct priv_exec *priv = netresolve_backend_get_priv(resolver);

	if (priv->infd != -1) {
		netresolve_backend_watch_fd(resolver, priv->infd, 0);
		close(priv->infd);
	}
	if (priv->outfd != -1) {
		netresolve_backend_watch_fd(resolver, priv->outfd, 0);
		close(priv->outfd);
	}
	/* TODO: Implement proper child handling. */
	kill(priv->pid, SIGKILL);
	free(priv->inbuf.buffer);
	free(priv->outbuf.buffer);
}
