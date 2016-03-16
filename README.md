# pty
A small helper program that runs a command under a PTY

STDOUT and STDERR of the program under the PTY are redirected to STDOUT.

Cribbed from Rachid Koucha's page on PTY:
http://rachid.koucha.free.fr/tech_corner/pty_pdip.html

Licensed under GPL3

usage:

pty [program] [args]

useful for getting things like the passwd program to talk over STDIN and STDOUT
nicely.
