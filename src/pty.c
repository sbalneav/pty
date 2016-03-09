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

#define _XOPEN_SOURCE 600
#define _BSD_SOURCE
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <termios.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <errno.h>

static int child_exited = 0;

/*
 * handle_sigchild
 *
 * do a proper waitpid so child exits cleanly, set the child_exited status
 * variable for later in the program.
 */

void
handle_sigchld (int sig)
{
  int saved_errno = errno;
  while (waitpid ((pid_t) (-1), 0, WNOHANG) > 0);
  child_exited = 1;
  errno = saved_errno;
}

/*
 * die
 *
 * print an error message to STDERR and exit with status of 1.
 */

void
die (const char *format, ...)
{
  va_list ap;

  va_start (ap, format);
  vfprintf (stderr, format, ap);
  va_end (ap);
  fprintf (stderr, "\n");                      /* terminate with a \n */
  exit (1);
}

/*
 * mainline
 */

int
main (int argc, char **argv)
{
  int fdmaster, fdslave;
  int i, rc;
  char input[BUFSIZ];
  char **child_argv;
  struct sigaction sa;

  /* Check arguments */
  if (argc < 2)
    die ("Usage: %s program_name [parameters]", argv[0]);

  fdmaster = posix_openpt (O_RDWR);
  if (fdmaster < 0)
    die ("Error %d on posix_openpt()", errno);

  rc = grantpt (fdmaster);
  if (rc != 0)
    die ("Error %d on grantpt()", errno);

  rc = unlockpt (fdmaster);
  if (rc != 0)
    die ("Error %d on unlockpt()", errno);

  /* Open the slave side ot the PTY */
  fdslave = open (ptsname (fdmaster), O_RDWR);
  if (fdslave < 0)
    die ("Error %d on open()", errno);

  /* before we fork, set SIGCHLD signal handler */
  sa.sa_handler = &handle_sigchld;
  sigemptyset (&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
  if (sigaction (SIGCHLD, &sa, 0) == -1)
    die ("Error %d on sigaction()", errno);

  /* Create the child process */
  if (fork ())
    {
      /******************************************************************/
      /*                             Parent                             */
      /******************************************************************/

      fd_set fd_in;

      close (fdslave);                         /* Close slave side of PTY */

      for (;;)
        {
          if (child_exited)
            break;

          /* Wait for data from standard input and master side of PTY */
          FD_ZERO (&fd_in);
          FD_SET (STDIN_FILENO, &fd_in);
          FD_SET (fdmaster, &fd_in);

          rc = select (fdmaster + 1, &fd_in, NULL, NULL, NULL);
          if (rc < 0)
            die ("Error %d on select()", errno);

          /* If data on STDIN, write to master side of PTY */
          if (FD_ISSET (STDIN_FILENO, &fd_in))
            {
              rc = read (STDIN_FILENO, input, sizeof input);
              if (rc < 0)
                die ("Error %d on read STDIN", errno);
              else
                write (fdmaster, input, rc);
            }

          /* If data on master side of PTY, write to STDOUT */
          if (FD_ISSET (fdmaster, &fd_in))
            {
              rc = read (fdmaster, input, sizeof input);
              if (rc < 0)
                die ("Error %d on read master PTY", errno);
              else
                write (STDOUT_FILENO, input, rc);
            }
        }
    }
  else
    {
      /******************************************************************/
      /*                             Child                              */
      /******************************************************************/

      struct termios slave_orig_term_settings; /* Saved terminal settings */
      struct termios new_term_settings;        /* Current terminal settings */

      close (fdmaster);                        /* Close master side of PTY */

      /* Save the defaults parameters of the slave side of the PTY */
      rc = tcgetattr (fdslave, &slave_orig_term_settings);
      if (rc < 0)
        die ("Error %d on tcgetattr()", errno);

      /* Set RAW mode on slave side of PTY */
      new_term_settings = slave_orig_term_settings;
      cfmakeraw (&new_term_settings);
      rc = tcsetattr (fdslave, TCSANOW, &new_term_settings);
      if (rc < 0)
        die ("Error %d on tcsetattr()", errno);

      /*
       * The slave side of the PTY becomes the standard input and
       * outputs of the child process
       */

      close (STDIN_FILENO);                    /* Close STDIN (current) */
      close (STDOUT_FILENO);                   /* Close STDOUT (current) */
      close (STDERR_FILENO);                   /* Close STDERR (current) */

      dup (fdslave);                           /* PTY becomes STDIN */
      dup (fdslave);                           /* PTY becomes STDOUT */
      dup (fdslave);                           /* PTY becomes STDERR */

      close (fdslave);                         /* Close original fd */

      setsid ();                               /* Set as session leader */

      /*
       * As the child is a session leader, set the controlling terminal
       * to be the slave side of the PTY
       */

      ioctl (STDIN_FILENO, TIOCSCTTY, 1);

      /* Build the command line for program execution */
      /* argv[argc] = NULL, so we can just pass along the current argv + 1. */
      child_argv = argv;
      child_argv++;
      rc = execvp (child_argv[0], child_argv);

      /* Shouldn't ever reach here after an execvp... */
      return 1;
    }

  return 0;
}                                              /* main */
