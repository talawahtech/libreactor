#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>

#include <dynamic.h>

#include "reactor_user.h"
#include "reactor_desc.h"
#include "reactor_core.h"

static inline void reactor_desc_close_final(reactor_desc *desc)
{
  if (desc->state == REACTOR_DESC_CLOSE_WAIT &&
      desc->ref == 0)
    {
      reactor_user_dispatch(&desc->user, REACTOR_DESC_CLOSE, NULL);
      desc->state = REACTOR_DESC_CLOSED;
    }
}

static inline void reactor_desc_hold(reactor_desc *desc)
{
  desc->ref ++;
}

static inline void reactor_desc_release(reactor_desc *desc)
{
  desc->ref --;
  reactor_desc_close_final(desc);
}

void reactor_desc_init(reactor_desc *desc, reactor_user_callback *callback, void *state)
{
  *desc = (reactor_desc) {.state = REACTOR_DESC_CLOSED, .index = -1};
  reactor_user_init(&desc->user, callback, state);
}

void reactor_desc_open(reactor_desc *desc, int fd)
{
  int e;

  if (desc->state != REACTOR_DESC_CLOSED)
    {
      (void) close(fd);
      reactor_desc_error(desc);
      return;
    }

  e = reactor_core_desc_add(desc, fd, REACTOR_DESC_READ);
  if (e == -1)
    {
      (void) close(fd);
      desc->index = -1;
      reactor_desc_error(desc);
      return;
    }

  desc->state = REACTOR_DESC_OPEN;
}

void reactor_desc_close(reactor_desc *desc)
{
  if (desc->state == REACTOR_DESC_OPEN || desc->state == REACTOR_DESC_INVALID)
    {
      if (desc->index >= 0)
        {
          (void) close(reactor_core_desc_fd(desc));
          reactor_core_desc_remove(desc);
        }
      desc->state = REACTOR_DESC_CLOSE_WAIT;
      reactor_desc_close_final(desc);
    }
}

void reactor_desc_error(reactor_desc *desc)
{
  desc->state = REACTOR_DESC_INVALID;
  reactor_user_dispatch(&desc->user, REACTOR_DESC_ERROR, NULL);
}

void reactor_desc_events(reactor_desc *desc, int events)
{
  reactor_core_desc_events(desc, events);
}

int reactor_desc_fd(reactor_desc *desc)
{
  return reactor_core_desc_fd(desc);
}

void reactor_desc_event(void *state, int type, void *data)
{
  reactor_desc *desc = state;

  reactor_desc_hold(desc);
  if (type & POLLHUP)
    reactor_user_dispatch(&desc->user, REACTOR_DESC_SHUTDOWN, data);
  else if (type & (POLLERR | POLLNVAL))
    reactor_user_dispatch(&desc->user, REACTOR_DESC_ERROR, data);
  else
    {
      if (type & POLLOUT)
        reactor_user_dispatch(&desc->user, REACTOR_DESC_WRITE, data);
      if (desc->state == REACTOR_DESC_OPEN && type & POLLIN)
        reactor_user_dispatch(&desc->user, REACTOR_DESC_READ, data);
    }
  reactor_desc_release(desc);
}

ssize_t reactor_desc_read(reactor_desc *desc, void *data, size_t size)
{
  return recv(reactor_core_desc_fd(desc), data, size, MSG_DONTWAIT);
}

void reactor_desc_read_notify(reactor_desc *desc, int flag)
{
  if (flag)
    reactor_core_desc_set(desc, POLLIN);
  else
    reactor_core_desc_clear(desc, POLLIN);
}

ssize_t reactor_desc_write(reactor_desc *desc, void *data, size_t size)
{
  return send(reactor_core_desc_fd(desc), data, size, MSG_DONTWAIT);
}

void reactor_desc_write_notify(reactor_desc *desc, int flag)
{
  if (flag)
    reactor_core_desc_set(desc, POLLOUT);
  else
    reactor_core_desc_clear(desc, POLLOUT);
}
