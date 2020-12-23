/* NBD client library in userspace
 * Copyright (C) 2013-2020 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>

#include "internal.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))

void
nbd_internal_hexdump (const void *data, size_t len, FILE *fp)
{
  size_t i, j;

  for (i = 0; i < len; i += 16) {
    fprintf (fp, "%04zx: ", i);
    for (j = i; j < MIN (i+16, len); ++j)
      fprintf (fp, "%02x ", ((const unsigned char *)data)[j]);
    for (; j < i+16; ++j)
      fprintf (fp, "   ");
    fprintf (fp, "|");
    for (j = i; j < MIN (i+16, len); ++j)
      if (isprint (((const char *)data)[j]))
        fprintf (fp, "%c", ((const char *)data)[j]);
      else
        fprintf (fp, ".");
    for (; j < i+16; ++j)
      fprintf (fp, " ");
    fprintf (fp, "|\n");
  }
}

/* Replace the string_vector with a deep copy of argv (including final NULL). */
int
nbd_internal_set_argv (string_vector *v, char **argv)
{
  size_t i;

  string_vector_reset (v);

  for (i = 0; argv[i] != NULL; ++i) {
    char *copy = strdup (argv[i]);
    if (copy == NULL)
      return -1;
    if (string_vector_append (v, copy) == -1) {
      free (copy);
      return -1;
    }
  }

  return string_vector_append (v, NULL);
}

/* Like sprintf ("%ld", v), but safe to use between fork and exec.  Do
 * not use this function in any other context.
 *
 * The caller must supply a scratch buffer which is at least 32 bytes
 * long (else the function will call abort()).  Note that the returned
 * string does not point to the start of this buffer.
 */
const char *
nbd_internal_fork_safe_itoa (long v, char *buf, size_t bufsize)
{
  unsigned long uv = (unsigned long) v;
  size_t i = bufsize - 1;
  bool neg = false;

  if (bufsize < 32) abort ();

  buf[i--] = '\0';
  if (v < 0) {
    neg = true;
    uv = -uv;
  }
  if (uv == 0)
    buf[i--] = '0';
  else {
    while (uv) {
      buf[i--] = '0' + (uv % 10);
      uv /= 10;
    }
  }
  if (neg)
    buf[i--] = '-';

  i++;
  return &buf[i];
}

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
#endif

/* Fork-safe version of perror.  ONLY use this after fork and before
 * exec, the rest of the time use set_error().
 */
void
nbd_internal_fork_safe_perror (const char *s)
{
  const int err = errno;
  const char *m = NULL;
  char buf[32];

  write (2, s, strlen (s));
  write (2, ": ", 2);
#ifdef HAVE_STRERRORDESC_NP
  m = strerrordesc_np (errno);
#else
#if HAVE_SYS_ERRLIST /* NB Don't use #ifdef */
  m = errno >= 0 && errno < sys_nerr ? sys_errlist[errno] : NULL;
#endif
#endif
  if (!m)
    m = nbd_internal_fork_safe_itoa ((long) errno, buf, sizeof buf);
  write (2, m, strlen (m));
  write (2, "\n", 1);

  /* Restore original errno in case it was disturbed by the system
   * calls above.
   */
  errno = err;
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

/* nbd_internal_printable_* functions are used by the API code to
 * print debug messages when we trace calls in and out of libnbd.  The
 * calls should attempt to convert the parameter into something
 * printable.
 *
 * They cannot fail, but it's OK if they return NULL.
 *
 * Caller frees the result.
 */

char *
nbd_internal_printable_buffer (const void *buf, size_t count)
{
  char *s = NULL;
  size_t len = 0, truncated;
  FILE *fp;

  fp = open_memstream (&s, &len);
  if (fp == NULL)
    return NULL;

  /* If the buffer is very long, truncate it to 1 sector. */
  if (count > 512) {
    truncated = count - 512;
    count = 512;
  }
  else
    truncated = 0;

  fprintf (fp, "\n");
  nbd_internal_hexdump (buf, count, fp);

  if (truncated)
    fprintf (fp, "[... %zu more bytes truncated ...]\n", truncated);
  fclose (fp);

  return s;
}

static void
printable_string (const char *str, FILE *fp)
{
  size_t i, n, truncated;

  if (str == NULL) {
    fprintf (fp, "NULL");
    return;
  }

  n = strlen (str);
  if (n > 512) {
    truncated = n - 512;
    n = 512;
  }
  else
    truncated = 0;

  fprintf (fp, "\"");
  for (i = 0; i < n; ++i) {
    if (isprint (str[i]))
      fputc (str[i], fp);
    else
      fprintf (fp, "\\x%02x", str[i]);
  }

  if (truncated)
    fprintf (fp, "[... %zu more bytes truncated ...]", truncated);
  fprintf (fp, "\"");
}

char *
nbd_internal_printable_string (const char *str)
{
  char *s = NULL;
  size_t len = 0;
  FILE *fp;

  fp = open_memstream (&s, &len);
  if (fp == NULL)
    return NULL;

  printable_string (str, fp);
  fclose (fp);

  return s;
}

char *
nbd_internal_printable_string_list (char **list)
{
  char *s = NULL;
  size_t len = 0, i;
  FILE *fp;

  fp = open_memstream (&s, &len);
  if (fp == NULL)
    return NULL;

  fprintf (fp, "[");
  for (i = 0; list[i] != NULL; ++i) {
    if (i > 0)
      fprintf (fp, ", ");
    printable_string (list[i], fp);
  }
  fprintf (fp, "]");
  fclose (fp);

  return s;

}

#ifndef WIN32

/* Set the FD_CLOEXEC flag on the given fd, if it is non-negative.
 * On failure, close fd and return -1; on success, return fd.
 *
 * Note that this function should ONLY be used on platforms that lack
 * atomic CLOEXEC support during fd creation (such as Haiku in 2019);
 * when using it as a fallback path, you must also consider how to
 * prevent fd leaks to plugins that want to fork().
 */
int
set_cloexec (int fd)
{
#if (defined SOCK_CLOEXEC && defined HAVE_MKOSTEMP && defined HAVE_PIPE2 && \
     defined HAVE_ACCEPT4)
  nbdkit_error ("prefer creating fds with CLOEXEC atomically set");
  close (fd);
  errno = EBADF;
  return -1;
#else
# if (defined SOCK_CLOEXEC || defined HAVE_MKOSTEMP || defined HAVE_PIPE2 || \
      defined HAVE_ACCEPT4) && !defined(__MACH__)
# error "Unexpected: your system has incomplete atomic CLOEXEC support"
# endif
  int f;
  int err;

  if (fd == -1)
    return -1;

  f = fcntl (fd, F_GETFD);
  if (f == -1 || fcntl (fd, F_SETFD, f | FD_CLOEXEC) == -1) {
    err = errno;
    //nbdkit_error ("fcntl: %m");
    close (fd);
    errno = err;
    return -1;
  }
  return fd;
#endif
}

#else /* WIN32 */

int
set_cloexec (int fd)
{
  return fd;
}

#endif /* WIN32 */

#ifndef WIN32

/* Set the O_NONBLOCK flag on the given fd, if it is non-negative.
 * On failure, close fd and return -1; on success, return fd.
 */
int
set_nonblock (int fd)
{
  int f;
  int err;

  if (fd == -1)
    return -1;

  f = fcntl (fd, F_GETFL);
  if (f == -1 || fcntl (fd, F_SETFL, f | O_NONBLOCK) == -1) {
    err = errno;
    //nbdkit_error ("fcntl: %m");
    close (fd);
    errno = err;
    return -1;
  }
  return fd;
}

#else /* WIN32 */

int
set_nonblock (int fd)
{
  return fd;
}

#endif /* WIN32 */
