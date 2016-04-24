/*
 * pty.c
 *
 * Run a program in a pty.  forcing it to communicate via STDIN and STDOUT
 *
 * Cribbed from Rachid Koucha's page on PTY:
 * http://rachid.koucha.free.fr/tech_corner/pty_pdip.html
 *
 *  Copyright (C) 2007-2015 Rachid Koucha <rachid dot koucha at free dot fr>
 *  Copyright (C) 2016 Scott Balneaves <sbalneav@ltsp.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <pty.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>

#define TIMEOUTUSEC 250000

/*
 * copyfd
 *
 * Read from one fd, write to the other. Handles short writes.
 */

void
copyfd (int in, int out)
{
  ssize_t len, wlen;
  char    buf[BUFSIZ], *ptr = buf;

  if ((len = read (in, buf, BUFSIZ)) > 0)
    do
      {
        if ((wlen = write (out, ptr, len)) < 0)
          break;
        len -= wlen;
        ptr += wlen;
      }
    while (len > 0);
}

/*
 * mainline
 */

int
main (int argc, char **argv)
{
  int    master;
  pid_t  pid;

  if (argc < 2)
    {
      fprintf (stderr, "Usage: %s program_name [program_arguments]\n", argv[0]);
      return 1;
    }

  /* Create the child process */
  pid = forkpty (&master, NULL, NULL, NULL);
  if (!pid)                     /* child */
    {
      /* Build argv for execvp. argv[argc] = NULL, so just pass argv + 1. */
      execvp (*(argv + 1), (argv + 1));
      return 1;
    }
  else if (pid > 0)             /* parent */
    {
      struct timeval tv;
      int status = 0;
      fd_set fd_in;

      for (;;)
        {
          /* Wait for data from STDIN and master side of PTY */
          FD_ZERO (&fd_in);
          FD_SET (STDIN_FILENO, &fd_in);
          FD_SET (master, &fd_in);

          /* Pause for TIMEOUTUSEC microseconds on select */
          tv.tv_sec = 0;
          tv.tv_usec = TIMEOUTUSEC;
          if (select (master + 1, &fd_in, NULL, NULL, &tv) < 0)
            break;
          if (FD_ISSET (STDIN_FILENO, &fd_in))
            copyfd (STDIN_FILENO, master);
          if (FD_ISSET (master, &fd_in))
            copyfd (master, STDOUT_FILENO);
          if (waitpid(pid, &status, WNOHANG))
            break;
        }

      if (WIFEXITED (status))
        return WEXITSTATUS (status);
      else
        return 1;
    }

  return 1;
}
