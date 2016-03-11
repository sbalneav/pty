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

/* For posix_openpt */
#define _XOPEN_SOURCE 600
/* For cfmakeraw */
#define _BSD_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

int   child_exited = 0;
pid_t child_exit_status = 0;

/*
 * handle_sigchild
 *
 * Perform waitpid() so child exits cleanly, set the child_exited status
 * variable for later in the program.
 */

void
handle_sigchld (int sig __attribute__ ((unused)))
{
  while (waitpid ((pid_t) (-1), &child_exit_status, WNOHANG) > 0);
  child_exited = 1;
}

/*
 * die
 *
 * Print an error message to STDERR and exit with status of 1.
 */

void
die (const char *msg)
{
  perror (msg);
  exit (1);
}

/*
 * mainline
 */

int
main (int argc, char **argv)
{
  int    fdmaster, fdslave;
  struct sigaction sa;

  if (argc < 2)
    return 1;

  /* Create master side of the PTY */
  if ((fdmaster = posix_openpt (O_RDWR|O_NOCTTY)) < 0)
    die ("posix_openpt()");
  if (grantpt (fdmaster) < 0)
    die ("grantpt()");
  if (unlockpt (fdmaster) < 0)
    die ("unlockpt()");

  /* Create the slave side of the PTY */
  if ((fdslave = open (ptsname (fdmaster), O_RDWR)) < 0)
    die ("open()");

  /* before we fork, set SIGCHLD signal handler */
  sa.sa_handler = &handle_sigchld;
  sigemptyset (&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
  if (sigaction (SIGCHLD, &sa, NULL) < 0)
    die ("sigaction()");

  /* Create the child process */
  if (!fork ())
    {
      /******************************************************************/
      /*                             Child                              */
      /******************************************************************/

      struct termios term_settings;

      close (fdmaster);

      /* Set RAW mode on slave side of the PTY */
      if (tcgetattr (fdslave, &term_settings) < 0)
        die ("tcgetattr()");
      cfmakeraw (&term_settings);
      if (tcsetattr (fdslave, TCSANOW, &term_settings) < 0)
        die ("tcsetattr()");

      /* Slave side of the PTY becomes STDIN, STDOUT & STDERR for child */
      dup2 (fdslave, STDIN_FILENO);
      dup2 (fdslave, STDOUT_FILENO);
      dup2 (fdslave, STDERR_FILENO);
      close (fdslave);

      /* Child becomes session leader, slave PTY becomes controlling term */
      if (setsid () < 0)
        die ("setsid()");
      if (ioctl (STDIN_FILENO, TIOCSCTTY, 1) < 0)
        die ("ioctl()");
      if (tcsetpgrp(STDIN_FILENO, getpid()) < 0)
        die ("tcsetpgrp()");

      /* Build argv for execvp. argv[argc] = NULL, so just pass argv + 1. */
      argv++;
      execvp (argv[0], argv);

      /* Shouldn't ever reach here after an execvp... */
      return 1;
    }
  else
    {
      /******************************************************************/
      /*                             Parent                             */
      /******************************************************************/

      int    len;
      char   buf[BUFSIZ];
      fd_set fd_in;

      close (fdslave);

      for (;;)
        {
          if (child_exited)
            break;

          /* Wait for data from standard input and master side of PTY */
          FD_ZERO (&fd_in);
          FD_SET (STDIN_FILENO, &fd_in);
          FD_SET (fdmaster, &fd_in);

          if (select (fdmaster + 1, &fd_in, NULL, NULL, NULL) < 0)
            die ("select()");

          /* If data on STDIN, write to master side of PTY */
          if (FD_ISSET (STDIN_FILENO, &fd_in))
            if ((len = read (STDIN_FILENO, buf, BUFSIZ)) > 0)
              write (fdmaster, buf, len);

          /* If data on master side of PTY, write to STDOUT */
          if (FD_ISSET (fdmaster, &fd_in))
            if ((len = read (fdmaster, buf, BUFSIZ)) > 0)
              write (STDOUT_FILENO, buf, len);
        }
      return WEXITSTATUS (child_exit_status);
    }
}
