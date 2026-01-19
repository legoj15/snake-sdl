#include "bot.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
typedef SOCKET sock_t;
#define SOCK_INVALID INVALID_SOCKET
static void sock_close(sock_t s) {
  if (s != INVALID_SOCKET)
    closesocket(s);
}
static int sock_set_nonblocking(sock_t s) {
  u_long mode = 1;
  return ioctlsocket(s, FIONBIO, &mode);
}
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int sock_t;
#define SOCK_INVALID (-1)
static void sock_close(sock_t s) {
  if (s >= 0)
    close(s);
}
static int sock_set_nonblocking(sock_t s) {
  int flags = fcntl(s, F_GETFL, 0);
  if (flags < 0)
    return -1;
  return fcntl(s, F_SETFL, flags | O_NONBLOCK);
}
#endif

static sock_t as_sock(uintptr_t v) { return (sock_t)v; }
static uintptr_t to_up(sock_t s) { return (uintptr_t)s; }

static void bot_drop_client(Bot *b) {
  if (!b)
    return;
  sock_close(as_sock(b->_client_sock));
  b->_client_sock = (uintptr_t)SOCK_INVALID;
  b->connected = false;
}

bool Bot_Init(Bot *b, bool enable, uint16_t port) {
  if (!b)
    return false;
  memset(b, 0, sizeof(*b));

  b->enabled = enable;
  b->connected = false;
  b->port = (port == 0) ? 5555 : port;
  b->_listen_sock = (uintptr_t)SOCK_INVALID;
  b->_client_sock = (uintptr_t)SOCK_INVALID;

  if (!enable)
    return true;

#if defined(_WIN32)
  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
    fprintf(stderr, "[bot] WSAStartup failed\n");
    return false;
  }
#endif

  sock_t ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (ls == SOCK_INVALID) {
    fprintf(stderr, "[bot] socket() failed\n");
    return false;
  }

  int yes = 1;
  (void)setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes,
                   (socklen_t)sizeof(yes));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(b->port);
  addr.sin_addr.s_addr = htonl(0x7F000001u); // 127.0.0.1

  if (bind(ls, (struct sockaddr *)&addr, (socklen_t)sizeof(addr)) != 0) {
    fprintf(stderr, "[bot] bind(127.0.0.1:%u) failed\n", (unsigned)b->port);
    sock_close(ls);
    return false;
  }

  if (listen(ls, 1) != 0) {
    fprintf(stderr, "[bot] listen() failed\n");
    sock_close(ls);
    return false;
  }

  if (sock_set_nonblocking(ls) != 0) {
    fprintf(stderr, "[bot] failed to set listen socket non-blocking\n");
    sock_close(ls);
    return false;
  }

  b->_listen_sock = to_up(ls);
  fprintf(stderr, "[bot] listening on 127.0.0.1:%u\n", (unsigned)b->port);
  return true;
}

void Bot_Shutdown(Bot *b) {
  if (!b)
    return;

  bot_drop_client(b);
  sock_close(as_sock(b->_listen_sock));
  b->_listen_sock = (uintptr_t)SOCK_INVALID;

#if defined(_WIN32)
  if (b->enabled) {
    WSACleanup();
  }
#endif

  b->enabled = false;
}

void Bot_PollAccept(Bot *b) {
  if (!b || !b->enabled)
    return;
  if (b->connected)
    return;

  sock_t ls = as_sock(b->_listen_sock);
  if (ls == SOCK_INVALID)
    return;

  struct sockaddr_in caddr;
  socklen_t clen = (socklen_t)sizeof(caddr);
  sock_t cs = accept(ls, (struct sockaddr *)&caddr, &clen);

  if (cs == SOCK_INVALID) {
#if defined(_WIN32)
    int err = WSAGetLastError();
    if (err == WSAEWOULDBLOCK)
      return;
#else
    if (errno == EWOULDBLOCK || errno == EAGAIN)
      return;
#endif
    return;
  }

  if (sock_set_nonblocking(cs) != 0) {
    sock_close(cs);
    return;
  }

  b->_client_sock = to_up(cs);
  b->connected = true;
  fprintf(stderr, "[bot] client connected\n");
}

// Returns: 1 = got byte, 0 = would-block/no data, -1 = disconnect/error
static int recv_one_byte(sock_t s, uint8_t *out) {
  if (!out)
    return -1;
  uint8_t b;
  int n = (int)recv(s, (char *)&b, 1, 0);
  if (n == 1) {
    *out = b;
    return 1;
  }
  if (n == 0) {
    return -1; // clean disconnect
  }

#if defined(_WIN32)
  int err = WSAGetLastError();
  if (err == WSAEWOULDBLOCK)
    return 0;
#else
  if (errno == EWOULDBLOCK || errno == EAGAIN)
    return 0;
#endif
  return -1;
}

bool Bot_TryRecvDir(Bot *b, Dir *out_dir) {
  if (!b || !b->enabled || !b->connected)
    return false;
  if (!out_dir)
    return false;

  sock_t cs = as_sock(b->_client_sock);
  if (cs == SOCK_INVALID)
    return false;

  uint8_t v;
  int r = recv_one_byte(cs, &v);
  if (r == 0)
    return false; // no data
  if (r < 0) {
    bot_drop_client(b);
    return false;
  }

  if (v > 3)
    return false;
  *out_dir = (Dir)v;
  return true;
}

// Non-blocking-ish send helper: if it would block or short-writes, we treat it
// as failure. For localhost small payloads, this is typically fine.
static bool send_all_or_drop(sock_t s, const void *data, int len) {
  const char *p = (const char *)data;
  int sent = 0;

  while (sent < len) {
    int n = (int)send(s, p + sent, len - sent, 0);
    if (n > 0) {
      sent += n;
      continue;
    }

#if defined(_WIN32)
    int err = WSAGetLastError();
    if (err == WSAEWOULDBLOCK)
      return false;
#else
    if (errno == EWOULDBLOCK || errno == EAGAIN)
      return false;
#endif
    return false;
  }

  return true;
}

void Bot_SendState(Bot *b, const Snake *s, IVec2 apple, int score,
                   bool game_over, bool you_win) {
  if (!b || !b->enabled || !b->connected || !s)
    return;

  sock_t cs = as_sock(b->_client_sock);
  if (cs == SOCK_INVALID)
    return;

  // Header (version 2)
  BotStateMsg m;
  memset(&m, 0, sizeof(m));
  m.magic = htonl(0x534E4B42u); // 'SNKB'
  m.version = htons(2);

  uint16_t flags = 0;
  if (game_over)
    flags |= 1u;
  if (you_win)
    flags |= 2u;
  m.flags = htons(flags);

  m.grid_w = htonl(s->grid_w);
  m.grid_h = htonl(s->grid_h);

  IVec2 head = s->seg[0];
  m.head_x = htonl(head.x);
  m.head_y = htonl(head.y);

  m.apple_x = htonl(apple.x);
  m.apple_y = htonl(apple.y);

  m.snake_len = htonl(s->len);
  m.score = htonl(score);

  // Send header
  if (!send_all_or_drop(cs, &m, (int)sizeof(m))) {
    bot_drop_client(b);
    return;
  }

  // Send payload: seg[0..len-1] as big-endian int32 pairs.
  // Layout: x0 y0 x1 y1 ... (int32 each)
  for (int i = 0; i < s->len; i++) {
    int32_t x = htonl(s->seg[i].x);
    int32_t y = htonl(s->seg[i].y);
    if (!send_all_or_drop(cs, &x, (int)sizeof(x)) ||
        !send_all_or_drop(cs, &y, (int)sizeof(y))) {
      bot_drop_client(b);
      return;
    }
  }
}
