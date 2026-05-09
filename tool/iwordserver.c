/*
 * iword-server: Unix socket + TCP server wrapping iword C library
 *
 * Protocol: newline-delimited JSON (phase 1)
 *
 * Requests:
 *   {"op":"seek","word":"free"}\n
 *   {"op":"map","text":"get free prize","mode":3}\n
 *   {"op":"mask"}\n
 *   {"op":"status"}\n
 *   {"op":"ping"}\n
 *
 * Responses:
 *   {"key":2}\n
 *   {"matches":[{"pos":4,"len":4,"key":2}],"mask":4}\n
 *   {"mask":4}\n
 *   {"loaded":true}\n
 *   {"pong":true}\n
 *   {"error":"..."}\n
 *
 * iword_seek/iword_map are NOT thread-safe: all iword calls are
 * serialized through a single worker thread via a request queue.
 * Client connections are handled by a dedicated I/O thread each,
 * but they enqueue work and block until the worker responds.
 *
 * Usage:
 *   iwordserver [-u /tmp/iword.sock] [-p 7743] [-d dict_key]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "iword.h"

#define DEFAULT_UNIX_PATH   "/tmp/iword.sock"
#define DEFAULT_TCP_PORT    7743
#define MAX_LINE            65536
#define BACKLOG             64

/* ---------- simple JSON parser (no deps) --------------------------------- */

static const char *json_str(const char *json, const char *key, char *out, size_t outsz) {
	char pat[128];
	snprintf(pat, sizeof(pat), "\"%s\"", key);
	const char *p = strstr(json, pat);
	if (!p) return NULL;
	p += strlen(pat);
	while (*p == ' ' || *p == ':' || *p == '\t') p++;
	if (*p != '"') return NULL;
	p++;
	size_t i = 0;
	while (*p && *p != '"' && i + 1 < outsz) {
		if (*p == '\\' && *(p+1)) { p++; }
		out[i++] = *p++;
	}
	out[i] = '\0';
	return out;
}

static int json_int(const char *json, const char *key, int def) {
	char pat[128];
	snprintf(pat, sizeof(pat), "\"%s\"", key);
	const char *p = strstr(json, pat);
	if (!p) return def;
	p += strlen(pat);
	while (*p == ' ' || *p == ':' || *p == '\t') p++;
	if (*p == '-' || (*p >= '0' && *p <= '9')) return atoi(p);
	return def;
}

/* ---------- JSON response builders --------------------------------------- */

static void json_escape(const char *s, char *out, size_t outsz) {
	size_t j = 0;
	for (size_t i = 0; s[i] && j + 4 < outsz; i++) {
		unsigned char c = (unsigned char)s[i];
		if (c == '"')       { out[j++] = '\\'; out[j++] = '"'; }
		else if (c == '\\') { out[j++] = '\\'; out[j++] = '\\'; }
		else if (c == '\n') { out[j++] = '\\'; out[j++] = 'n'; }
		else if (c == '\r') { out[j++] = '\\'; out[j++] = 'r'; }
		else if (c < 0x20)  { j += snprintf(out+j, outsz-j, "\\u%04x", c); }
		else                { out[j++] = (char)c; }
	}
	out[j] = '\0';
}

/* ---------- worker thread (serializes all iword calls) ------------------- */

typedef struct {
	char   req[MAX_LINE];
	char   resp[MAX_LINE * 2];
	pthread_mutex_t mu;
	pthread_cond_t  ready;   /* worker signals client */
	pthread_cond_t  pending; /* client signals worker */
	int    has_req;
	int    has_resp;
} work_slot_t;

typedef struct slot_node {
	work_slot_t        *slot;
	struct slot_node   *next;
} slot_node_t;

static pthread_mutex_t  queue_mu  = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   queue_cnd = PTHREAD_COND_INITIALIZER;
static slot_node_t     *queue_head = NULL;
static slot_node_t     *queue_tail = NULL;
static volatile int     running   = 1;

static void enqueue(slot_node_t *node) {
	pthread_mutex_lock(&queue_mu);
	node->next = NULL;
	if (queue_tail) queue_tail->next = node;
	else            queue_head = node;
	queue_tail = node;
	pthread_cond_signal(&queue_cnd);
	pthread_mutex_unlock(&queue_mu);
}

static slot_node_t *dequeue_blocking(void) {
	pthread_mutex_lock(&queue_mu);
	while (!queue_head && running)
		pthread_cond_wait(&queue_cnd, &queue_mu);
	slot_node_t *n = queue_head;
	if (n) {
		queue_head = n->next;
		if (!queue_head) queue_tail = NULL;
	}
	pthread_mutex_unlock(&queue_mu);
	return n;
}

static void handle_seek(const char *req, char *resp, size_t rsz) {
	char word[1024] = "";
	if (!json_str(req, "word", word, sizeof(word)) || !word[0]) {
		snprintf(resp, rsz, "{\"error\":\"missing word\"}\n");
		return;
	}
	int key = iword_seek(word);
	if (key == -1) snprintf(resp, rsz, "{\"found\":false,\"key\":null}\n");
	else           snprintf(resp, rsz, "{\"found\":true,\"key\":%d}\n", key);
}

static void handle_map(const char *req, char *resp, size_t rsz) {
	char text[MAX_LINE] = "";
	if (!json_str(req, "text", text, sizeof(text))) {
		snprintf(resp, rsz, "{\"error\":\"missing text\"}\n");
		return;
	}
	int mode = json_int(req, "mode", IWORD_MODE_HTML | IWORD_MODE_FORBID);
	int tlen = (int)strlen(text);

	long long *q = iword_map(text, tlen, mode);
	if (!q) {
		snprintf(resp, rsz, "{\"matches\":[],\"mask\":0}\n");
		return;
	}

	/* count entries */
	int r = 0;
	while (q[r]) r++;
	long long mask = q[r + 1];

	/* build JSON */
	size_t off = 0;
	off += snprintf(resp + off, rsz - off, "{\"matches\":[");
	for (int i = 0; i < r && off < rsz - 64; i++) {
		int pos = (int)(q[i] >> 16);
		int key = (int)((q[i] >> 8) & 0xff);
		int len = (int)(q[i] & 0xff);
		if (i > 0) off += snprintf(resp + off, rsz - off, ",");
		off += snprintf(resp + off, rsz - off,
			"{\"pos\":%d,\"len\":%d,\"key\":%d}", pos, len, key);
	}
	off += snprintf(resp + off, rsz - off, "],\"mask\":%lld}\n", mask);
	free(q);
}

static void handle_mask(char *resp, size_t rsz) {
	int mask = iword_mask();
	snprintf(resp, rsz, "{\"mask\":%d}\n", mask);
}

static void dispatch(const char *req, char *resp, size_t rsz) {
	char op[64] = "";
	if (!json_str(req, "op", op, sizeof(op))) {
		snprintf(resp, rsz, "{\"error\":\"missing op\"}\n");
		return;
	}
	if      (strcmp(op, "seek")   == 0) handle_seek(req, resp, rsz);
	else if (strcmp(op, "map")    == 0) handle_map(req, resp, rsz);
	else if (strcmp(op, "mask")   == 0) handle_mask(resp, rsz);
	else if (strcmp(op, "status") == 0) snprintf(resp, rsz, "{\"loaded\":true,\"version\":\"%s\"}\n", IWORD_VERSION);
	else if (strcmp(op, "ping")   == 0) snprintf(resp, rsz, "{\"pong\":true}\n");
	else snprintf(resp, rsz, "{\"error\":\"unknown op: %s\"}\n", op);
}

static void *worker_thread(void *arg) {
	(void)arg;
	while (running) {
		slot_node_t *node = dequeue_blocking();
		if (!node) break;
		work_slot_t *slot = node->slot;

		pthread_mutex_lock(&slot->mu);
		dispatch(slot->req, slot->resp, sizeof(slot->resp));
		slot->has_resp = 1;
		pthread_cond_signal(&slot->ready);
		pthread_mutex_unlock(&slot->mu);
	}
	return NULL;
}

/* ---------- per-connection I/O thread ------------------------------------ */

static void *conn_thread(void *arg) {
	int fd = (int)(long)arg;
	char line[MAX_LINE];
	size_t fill = 0;

	while (1) {
		ssize_t n = read(fd, line + fill, sizeof(line) - fill - 1);
		if (n <= 0) break;
		fill += (size_t)n;
		line[fill] = '\0';

		/* process complete lines */
		char *start = line;
		char *nl;
		while ((nl = memchr(start, '\n', (size_t)(line + fill - start))) != NULL) {
			*nl = '\0';

			/* allocate a work slot on the stack of this thread */
			work_slot_t slot;
			memset(&slot, 0, sizeof(slot));
			pthread_mutex_init(&slot.mu, NULL);
			pthread_cond_init(&slot.ready, NULL);
			strncpy(slot.req, start, sizeof(slot.req) - 1);
			slot.has_req  = 1;
			slot.has_resp = 0;

			slot_node_t node = { &slot, NULL };
			enqueue(&node);

			pthread_mutex_lock(&slot.mu);
			while (!slot.has_resp)
				pthread_cond_wait(&slot.ready, &slot.mu);
			pthread_mutex_unlock(&slot.mu);

			if (write(fd, slot.resp, strlen(slot.resp)) < 0) {
				pthread_mutex_destroy(&slot.mu);
				pthread_cond_destroy(&slot.ready);
				goto done;
			}

			pthread_mutex_destroy(&slot.mu);
			pthread_cond_destroy(&slot.ready);

			start = nl + 1;
		}

		/* shift remaining partial line to front */
		size_t remaining = (size_t)(line + fill - start);
		if (remaining > 0 && start != line)
			memmove(line, start, remaining);
		fill = remaining;
		if (fill >= sizeof(line) - 1) fill = 0; /* overflow guard */
	}
done:
	close(fd);
	return NULL;
}

static void accept_loop(int lfd) {
	while (running) {
		int cfd = accept(lfd, NULL, NULL);
		if (cfd < 0) {
			if (errno == EINTR || errno == EWOULDBLOCK) continue;
			break;
		}
		pthread_t t;
		if (pthread_create(&t, NULL, conn_thread, (void *)(long)cfd) != 0) {
			close(cfd);
		} else {
			pthread_detach(t);
		}
	}
}

/* ---------- listener threads --------------------------------------------- */

typedef struct { int fd; } listener_arg_t;

static void *unix_listener(void *arg) {
	int lfd = ((listener_arg_t *)arg)->fd;
	free(arg);
	accept_loop(lfd);
	return NULL;
}

static void *tcp_listener(void *arg) {
	int lfd = ((listener_arg_t *)arg)->fd;
	free(arg);
	accept_loop(lfd);
	return NULL;
}

/* ---------- main --------------------------------------------------------- */

static void usage(void) {
	fprintf(stderr,
		"Usage: iwordserver [options]\n"
		"Options:\n"
		"  -u PATH   Unix socket path (default: " DEFAULT_UNIX_PATH ")\n"
		"  -p PORT   TCP port (default: %d, 0 = disable)\n"
		"  -d KEY    Dictionary key (default: empty)\n"
		"  -n        Disable Unix socket\n"
		"  -h        Show this help\n",
		DEFAULT_TCP_PORT);
}

int main(int argc, char **argv) {
	const char *unix_path = DEFAULT_UNIX_PATH;
	int tcp_port = DEFAULT_TCP_PORT;
	int no_unix  = 0;
	int opt;

	while ((opt = getopt(argc, argv, "u:p:d:nh")) != -1) {
		switch (opt) {
		case 'u': unix_path = optarg; break;
		case 'p': tcp_port  = atoi(optarg); break;
		case 'd': iword_set_strkey(optarg, strlen(optarg)); break;
		case 'n': no_unix   = 1; break;
		case 'h': usage(); return 0;
		default:  usage(); return 1;
		}
	}

	signal(SIGPIPE, SIG_IGN);

	/* start iword worker */
	pthread_t worker;
	if (pthread_create(&worker, NULL, worker_thread, NULL) != 0) {
		perror("pthread_create worker");
		return 1;
	}

	int started = 0;

	/* Unix socket listener */
	if (!no_unix) {
		int ufd = socket(AF_UNIX, SOCK_STREAM, 0);
		if (ufd < 0) { perror("socket(unix)"); }
		else {
			unlink(unix_path);
			struct sockaddr_un addr;
			memset(&addr, 0, sizeof(addr));
			addr.sun_family = AF_UNIX;
			strncpy(addr.sun_path, unix_path, sizeof(addr.sun_path) - 1);
			if (bind(ufd, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
			    listen(ufd, BACKLOG) < 0) {
				perror("bind/listen(unix)");
				close(ufd);
			} else {
				listener_arg_t *la = malloc(sizeof(*la));
				la->fd = ufd;
				pthread_t t;
				pthread_create(&t, NULL, unix_listener, la);
				pthread_detach(t);
				fprintf(stderr, "iword-server: listening on %s (unix)\n", unix_path);
				started++;
			}
		}
	}

	/* TCP listener */
	if (tcp_port > 0) {
		int tfd = socket(AF_INET, SOCK_STREAM, 0);
		if (tfd < 0) { perror("socket(tcp)"); }
		else {
			int on = 1;
			setsockopt(tfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
			struct sockaddr_in addr;
			memset(&addr, 0, sizeof(addr));
			addr.sin_family      = AF_INET;
			addr.sin_addr.s_addr = INADDR_ANY;
			addr.sin_port        = htons((uint16_t)tcp_port);
			if (bind(tfd, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
			    listen(tfd, BACKLOG) < 0) {
				perror("bind/listen(tcp)");
				close(tfd);
			} else {
				listener_arg_t *la = malloc(sizeof(*la));
				la->fd = tfd;
				pthread_t t;
				pthread_create(&t, NULL, tcp_listener, la);
				pthread_detach(t);
				fprintf(stderr, "iword-server: listening on TCP port %d\n", tcp_port);
				started++;
			}
		}
	}

	if (!started) {
		fprintf(stderr, "iword-server: no listeners started, exiting\n");
		return 1;
	}

	/* main thread parks here; Ctrl-C terminates */
	pause();
	running = 0;
	pthread_cond_broadcast(&queue_cnd);
	pthread_join(worker, NULL);
	return 0;
}
