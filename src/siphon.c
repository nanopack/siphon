#include <stdio.h>    // stderr, stdin, stdout - standard I/O streams
#include <string.h>   // string operations
#include <stdbool.h>  // boolean type and values
#include <unistd.h>   // standard symbolic constants and types
#include <stdlib.h>   // standard library definitions
#include <getopt.h>   // command option parsing
#include "siphon_stream.h"
#include "siphon_pty.h"

// navigation: https://github.com/iarna/gauge/blob/master/console-strings.js
// http://ascii-table.com/ansi-escape-sequences.php
// http://bluesock.org/~willg/dev/ansi.html
// http://linux.die.net/man/3/libexpect

void
usage(void)
{
  fprintf(stderr,"\nsiphon - output stream formatter\n\n");
  fprintf(stderr,"Usage: siphon [options] [-- <command to exec>]\n");
	fprintf(stderr,"       siphon -p '-> ' or --prefix '-> '\n");
	fprintf(stderr,"       siphon -h or --help\n\n");
	fprintf(stderr,"Examples:\n");
	fprintf(stderr,"       ls -lah / | siphon --prefix '-> '\n");
  fprintf(stderr,"       siphon --prefix '-> ' -- ls -lah /\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
  // user-specified prefix string
  char *prefix = NULL;
  bool prefix_set = false;

  // long options
  static struct option long_opts[] = {
    {"prefix", required_argument, 0, 'p'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
  };

  /* getopt_long stores the option index here. */
  int opt_index = 0;

  int c;
  while ((c = getopt_long(argc, argv, "p:h", long_opts, &opt_index)) != -1) {
    switch (c) {
      case 'p':
        prefix = malloc(strlen(optarg) * sizeof(char) + 1);
        strcpy(prefix, optarg);
        prefix_set = true;
        break;
      case 'h':
        usage();
        break;
    }
  }

  if (optind == argc) {
    // If optind == argc, then stream from stdin
    if (prefix_set == false)
      stream("", stdin);
    else
      stream(prefix, stdin);
  } else {
    // If optind > argc, then try running the command and streaming from that

    int fint = exp_spawnv(prefix, prefix_set, argv[optind],&argv[optind]);

    FILE *fd = fdopen(fint, "r");
    setbuf(fd,(char *)0);

    if (prefix_set == false)
      stream("", fd);
    else {
      stream(prefix, fd);
    }
    int child_status;
    waitpid(exp_pid, &child_status, 0);
    exit(WEXITSTATUS(child_status));
  }


}

