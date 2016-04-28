#include <stdio.h>    // stderr, stdin, stdout - standard I/O streams
#include <string.h>   // string operations
#include <stdbool.h>  // boolean type and values
#include <unistd.h>   // standard symbolic constants and types
#include <stdlib.h>   // standard library definitions
#include <getopt.h>   // command option parsing

#define BUF_SIZE 2

// navigation: https://github.com/iarna/gauge/blob/master/console-strings.js
// http://ascii-table.com/ansi-escape-sequences.php
// http://bluesock.org/~willg/dev/ansi.html
// http://linux.die.net/man/3/libexpect

bool
is_sequence_end(char *c)
{
  char terminators[] = "GHfABCDsuJKmhlp";

  int i;
  for (i = 0; i < strlen(terminators); i++) {
    if (terminators[i] == *c)
      return true;
  }

  return false;
}

void
adjust_horizontal_seq(char *seq, int offset)
{
  int number;
  sscanf(seq, "\x1b[%iG", &number);
  sprintf(seq, "\x1b[%iG", number + offset);
}

void
adjust_goto_seq(char *seq, int offset)
{
  int x, y;
  sscanf(seq, "\x1b[%i;%iH", &y, &x);
  sprintf(seq, "\x1b[%i;%iH", y, x + offset);
}

void
stream(char *prefix)
{
  // buffer to hold the characters read from stdin
  char buffer[BUF_SIZE];

  // indicate if we're on a new line. If true, the next character should
  // be printed with the prefix provided.
  bool first_line = true;
  bool new_line = false;

  // escape sequences can move the cursor around the lines. We need to adjust
  // for prefix width when this happens
  bool escape_seq = false;
  // create a buffer to store the current escape sequence
  char *escape_seq_buf = calloc(16, sizeof(char));
  // store an index for the escape sequence character count
  int escape_seq_index = 0;

  //turn off buffering on stdin
  setvbuf(stdin, NULL, _IONBF, 0);

  while (fgets(buffer, BUF_SIZE, stdin)) {

    if (first_line == true) {
      first_line = false;
      printf("%s", prefix);
    }

    int i;
    for (i = 0; i < strlen(buffer); i++) {
      char c = buffer[i];

      // as soon as we see an escape character
      if (c == '\x1b') {
        escape_seq = true;
        escape_seq_buf[0] = c;
        escape_seq_index++;
      } else if (escape_seq == true) {
        // add character to escape seq
        escape_seq_buf[escape_seq_index] = c;
        escape_seq_index++;

        // check if escape sequence is complete
        if (is_sequence_end(&c) == true) {
          // check if goto or horizontal reset and modify
          if (c == 'G' && new_line == false)
            adjust_horizontal_seq(escape_seq_buf, strlen(prefix));
          else if (c == 'H' && new_line == false)
            adjust_goto_seq(escape_seq_buf, strlen(prefix));

          // print escape sequence
          printf("%s", escape_seq_buf);

          // reset initialized escape sequence
          escape_seq = false;
          // clear the escape sequence buffer
          memset(escape_seq_buf, 0, 16);
          // reset the escape sequence buffer
          escape_seq_index = 0;
        }

      } else {
        // print the character (prefixed if this is a new line)
        if (new_line == true)
          printf("%s%c", prefix, c);
        else
          printf("%c", c);

        // set new_line to true if the printed character was a newline
        // and the character previous wasn't a line reset
        if (c == '\n' || c == '\r')
          new_line = true;
        else
          new_line = false;
      }
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
