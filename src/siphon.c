#include <stdio.h>    // stderr, stdin, stdout - standard I/O streams
#include <string.h>   // string operations
#include <stdbool.h>  // boolean type and values
#include <unistd.h>   // standard symbolic constants and types
#include <stdlib.h>   // standard library definitions
#include <getopt.h>   // command option parsing

#define BUF_SIZE 2

void
stream(char *prefix)
{
  // buffer to hold the characters read from stdin
  char buffer[BUF_SIZE];

  // indicate if we're on a new line. If true, the next character should
  // be printed with the prefix provided.
  bool new_line = true;

  //turn off buffering on stdin
  setvbuf(stdin, NULL, _IONBF, 0);

  while (fgets(buffer, BUF_SIZE, stdin)) {

    int i;
    for (i = 0; i < strlen(buffer); i++) {
      char c = buffer[i];

      // print the character (prefixed if this is a new line)
      if (new_line == true)
        printf("%s%c", prefix, c);
      else
        printf("%c", c);

      // set new_line to true if the printed character was a newline
      // or a line reset
      if (c == '\n' || c == '\r')
        new_line = true;
      else
        new_line = false;
    }
  }
}

void
usage(void)
{
  fprintf(stderr,"\nsiphon - output stream formatter\n\n");
  fprintf(stderr,"Usage: siphon [options]\n");
	fprintf(stderr,"       siphon -p '-> ' or --prefix '-> '\n");
	fprintf(stderr,"       siphon -h or --help\n\n");
	fprintf(stderr,"Examples:\n");
	fprintf(stderr,"       ls -lah / | siphon --prefix '-> '\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
  // user-specified prefix string
  char *prefix;
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
  while ((c = getopt_long(argc, argv, ":p:h", long_opts, &opt_index)) != -1) {
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

  if (prefix_set == false)
    stream("");
  else
    stream(prefix);
}
