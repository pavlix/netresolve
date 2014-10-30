#include <netresolve-private.h>
#include <netresolve-compat.h>
//#include <libasyncns.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>

typedef struct netresolve_asyncns asyncns_t;
typedef struct netresolve_asyncns_query asyncns_query_t;

struct netresolve_asyncns_query {
	asyncns_t *asyncns;
	netresolve_query_t query;
	bool done;
	void *userdata;
	asyncns_query_t *previous;
	asyncns_query_t *next;
};

struct netresolve_asyncns {
	netresolve_t channel;
	int epoll_fd;
	int epoll_count;
	asyncns_query_t queries;
};

static void
enqueue(asyncns_query_t *list, asyncns_query_t *q)
{
	q->previous = list->previous;
	q->next = list;

	q->previous->next = q;
	q->next->previous = q;
}

static void
dequeue(asyncns_query_t *q)
{
	q->previous->next = q->next;
	q->next->previous = q->previous;
}

static void
watch_fd(netresolve_query_t query, int fd, int events, void *user_data)
{
	asyncns_t *asyncns = user_data;
	struct epoll_event event = { .events = events, .data = { .fd = fd } };

	if (epoll_ctl(asyncns->epoll_fd, EPOLL_CTL_DEL, fd, &event) != -1)
		asyncns->epoll_count--;
	else if (errno != ENOENT && errno != EBADF)
		error("epoll_ctl: %s", strerror(errno));

	if (!events)
		return;

	if (epoll_ctl(asyncns->epoll_fd, EPOLL_CTL_ADD, fd, &event) != -1)
		asyncns->epoll_count++;
	else
		error("epoll_ctl: %s", strerror(errno));
}

static void
on_success(netresolve_query_t query, void *user_data)
{
	asyncns_query_t *q = netresolve_query_get_user_data(query);

	debug("asyncns query successful");
	q->done = true;
}

asyncns_t *
asyncns_new (unsigned n_proc)
{
	asyncns_t *asyncns;

	if (!(asyncns = calloc(1, sizeof *asyncns)))
		goto fail_asyncns;

    if (!(asyncns->channel = netresolve_open()))
		goto fail_channel;

	if ((asyncns->epoll_fd = epoll_create1(0)) == -1)
		goto fail_epoll;

	netresolve_set_fd_callback(asyncns->channel, watch_fd, asyncns);
	netresolve_set_success_callback(asyncns->channel, on_success, asyncns);

	asyncns->queries.previous = asyncns->queries.next = &asyncns->queries;

	return asyncns;
fail_epoll:
	netresolve_close(asyncns->channel);
fail_channel:
	free(asyncns);
fail_asyncns:
	return NULL;
}

int
asyncns_fd(asyncns_t *asyncns)
{
	return asyncns->epoll_fd;
}

int
asyncns_wait(asyncns_t *asyncns, int block)
{
	struct epoll_event event;
	int count;

	count = epoll_wait(asyncns->epoll_fd, &event, 1, block ? -1 : 0);

	if (count)
		netresolve_dispatch_fd(asyncns->channel, event.data.fd, event.events);

	/* undocumented return value */
	return 0;
}

static asyncns_query_t *
add_query(asyncns_t *asyncns, netresolve_query_t query)
{
	asyncns_query_t *q;

	if (!query)
		goto fail;

	if (!(q = calloc(1, sizeof *q)))
		goto fail_alloc;

	netresolve_query_set_user_data(query, q);

	q->asyncns = asyncns;
	q->query = query;

	enqueue(&asyncns->queries, q);

	return q;
fail_alloc:
	netresolve_query_done(query);
fail:
	return NULL;
}

static netresolve_query_t
remove_query(asyncns_query_t *q)
{
	netresolve_query_t query = q->query;

	dequeue(q);
	free(q);

	return query;
}

asyncns_query_t *
asyncns_getaddrinfo(asyncns_t *asyncns, const char *node, const char *service, const struct addrinfo *hints) 
{
	return add_query(asyncns, netresolve_query_getaddrinfo(asyncns->channel, node, service, hints));
}

int
asyncns_getaddrinfo_done (asyncns_t *asyncns, asyncns_query_t *q, struct addrinfo **result)
{
	return netresolve_query_getaddrinfo_done(remove_query(q), result, NULL);
}

asyncns_query_t *
asyncns_getnameinfo (asyncns_t *asyncns, const struct sockaddr *sa, socklen_t salen, int flags, int gethost, int getserv)
{
	return add_query(asyncns, netresolve_query_getnameinfo(asyncns->channel, sa, salen, flags));
}

int
asyncns_getnameinfo_done (asyncns_t *asyncns, asyncns_query_t *q, char *host, size_t hostlen, char *serv, size_t servlen)
{
	char *myhost;
	char *myserv;
	int status;

	status = netresolve_query_getnameinfo_done(remove_query(q), &myhost, &myserv, NULL);

	if (!status) {
		size_t myhostlen = strlen(myhost) + 1;
		size_t myservlen = strlen(myserv) + 1;

		if (hostlen <= myhostlen && servlen <= myservlen) {
			memcpy(host, myhost, myhostlen);
			memcpy(serv, myserv, myservlen);
		} else
			status = ERANGE;

		free(myhost);
		free(myserv);
	}

	return status;
}

asyncns_query_t *
asyncns_res_query (asyncns_t *asyncns, const char *dname, int class, int type)
{
	return add_query(asyncns, netresolve_query_res_query(asyncns->channel, dname, class, type));
}

asyncns_query_t *
asyncns_res_search (asyncns_t *asyncns, const char *dname, int class, int type)
{
	/* FIXME: enable search */

	return add_query(asyncns, netresolve_query_res_query(asyncns->channel, dname, class, type));
}

int
asyncns_res_done (asyncns_t *asyncns, asyncns_query_t *q, unsigned char **answer)
{
	return netresolve_query_res_query_done(remove_query(q), answer);
}

asyncns_query_t *
asyncns_getnext (asyncns_t *asyncns)
{
	asyncns_query_t *list = &asyncns->queries;

	for (asyncns_query_t *q = list->next; q != list; q = q->next)
		if (q->done)
			return q;

	return NULL;
}

int
asyncns_getnqueries (asyncns_t *asyncns)
{
	asyncns_query_t *list = &asyncns->queries;
	int count = 0;

	for (asyncns_query_t *q = list->next; q != list; q = q->next)
		count++;

	return count;
}

void
asyncns_cancel (asyncns_t *asyncns, asyncns_query_t *q)
{
	netresolve_query_done(q->query);
	dequeue(q);
	free(q);
}

void
asyncns_free (asyncns_t *asyncns)
{
	asyncns_query_t *list = &asyncns->queries;

	close(asyncns->epoll_fd);

	while (list->next != list)
		asyncns_cancel(asyncns, list->next);

	free(asyncns);
}

void
asyncns_freeaddrinfo (struct addrinfo *result)
{
	netresolve_freeaddrinfo(result);
}

void
asyncns_freeanswer (unsigned char *answer)
{
	free(answer);
}

int
asyncns_isdone (asyncns_t *asyncns, asyncns_query_t *q)
{
	return q->done;
}

void
asyncns_setuserdata (asyncns_t *asyncns, asyncns_query_t *q, void *userdata)
{
	q->userdata = userdata;
}

void *
asyncns_getuserdata (asyncns_t *asyncns, asyncns_query_t *q)
{
	return q->userdata;
}