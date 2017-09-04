#include <stdio.h>    // stderr, stdin, stdout - standard I/O streams
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define BUF_SIZE 2
#define STDIN_BUF_SIZE 2

// buffer to hold the characters read from in
char buffer[BUF_SIZE];

// indicate if we're on a new line. If true, the next character should
// be printed with the prefix provided.
bool first_line = true;
bool new_line = false;

// escape sequences can move the cursor around the lines. We need to adjust
// for prefix width when this happens
bool escape_seq = false;
// create a buffer to store the current escape sequence
char *escape_seq_buf = NULL;
int escape_seq_buf_size = 0;
// store an index for the escape sequence character count
int escape_seq_index = 0;

bool
is_sequence_end(char *c)
{
  char terminators[] = "ABCDEFGHJKRSTfminsulhp";

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
process_buffer(char *prefix)
{
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
      escape_seq_buf_size = 16;
      escape_seq_buf = calloc(escape_seq_buf_size, sizeof(char));
      escape_seq_buf[0] = c;
      escape_seq_index++;
    } else if (escape_seq == true) {
      // add character to escape seq
      escape_seq_buf[escape_seq_index] = c;
      escape_seq_index++;
      if (escape_seq_index == escape_seq_buf_size) {
        char *new_escape_seq_buf = realloc(escape_seq_buf, escape_seq_buf_size * 2 * sizeof(char));
        if (new_escape_seq_buf != NULL) {
          memset(new_escape_seq_buf + escape_seq_buf_size, 0, escape_seq_buf_size);
          escape_seq_buf_size = escape_seq_buf_size * 2;
          escape_seq_buf = new_escape_seq_buf;
        } else {
          exit(1);
        }
      }

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
        // memset(escape_seq_buf, 0, 16);
        free(escape_seq_buf);
        escape_seq_buf = NULL;
        escape_seq_buf_size = 0;
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

void
stream(char *prefix, FILE *in)
{
  //turn off buffering on in
  setvbuf(in, NULL, _IONBF, 0);

  while (fgets(buffer, BUF_SIZE, in)) {
    process_buffer(prefix);
  }
}

void
stream_interactive(char *prefix, FILE *subproc, FILE *in)
{
  //turn off buffering on subproc stdout
  setvbuf(subproc, NULL, _IONBF, 0);
  
  // turn off buffering on stdin
  setvbuf(in, NULL, _IONBF, 0);
  
  // create a file descriptor set
  fd_set fds;
  
  // get the fileno from the FILE descriptors
  int subproc_fileno = fileno(subproc);
  int in_fileno      = fileno(in);
  
  // set the max filedescriptor, which in this case MUST be 1 higher than
  // whichever filedescriptor is the highest. In this case, the subproc_fileno
  // will always be higher, since it's a spawned process and the stdin of this
  // process will always be 0.
  int max_fd = subproc_fileno + 1;
  
  // set some buffers for reading from stdin
  char stdin_buffer[STDIN_BUF_SIZE];
  
  while(1){

    FD_ZERO(&fds);
    FD_SET(subproc_fileno, &fds);
    FD_SET(in_fileno, &fds);

    // wait until data becomes available on either the subprocess stdout
    // or the current process's stdin, without a timeout
    select(max_fd, &fds, NULL, NULL, NULL); 
    
    // if we receive anything on stdout/stderr of the subprocess, we
    // process the buffer and print it out to the screen
    if (FD_ISSET(subproc_fileno, &fds)){
      if (fgets(buffer, BUF_SIZE, subproc))
        process_buffer(prefix);
      else
        return;
    }
    
    // if we receive anything from stdin, we just pipe it into the stdin
    // of the subprocess that we're running
    if (FD_ISSET(in_fileno, &fds)){
      if (fgets(stdin_buffer, STDIN_BUF_SIZE, in))
        fputs(stdin_buffer, subproc);
      else
        return;
    }
  }
}
