#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <assert.h>
#include <err.h>

#include <dynamic.h>

#include "reactor_core.h"

static char reply[] =
  "HTTP/1.0 200 OK\r\n"
  "Content-Length: 4\r\n"
  "Content-Type: plain/html\r\n"
  "Connection: keep-alive\r\n"
  "\r\n"
  "test";

void http_event(void *state, int type, void *data)
{
  reactor_http *http = state;
  reactor_http_session *session;
  size_t i;

  (void) http;
  switch (type)
    {
    case REACTOR_HTTP_REQUEST:
      session = data;
      printf("method: %s\n", session->message.header.method);
      printf("body %lu\n%.*s\n", session->message.body_size, (int) session->message.body_size, (char *) session->message.body);
      for (i = 0; i < session->message.header.fields_size; i ++)
        printf("header [%lu] %s = %s\n", i, session->message.header.fields[i].name, session->message.header.fields[i].value);
      reactor_stream_write_direct(&session->stream, reply, sizeof reply - 1);
      break;
    case REACTOR_HTTP_ERROR:
      reactor_http_close(http);
      break;
    }
}

int main()
{
  reactor_http http;

  reactor_core_open();
  reactor_http_init(&http, http_event, &http);
  reactor_http_server(&http, "localhost", "80");
  assert(reactor_core_run() == 0);

  reactor_core_close();
}