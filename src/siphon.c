#define _XOPEN_SOURCE 
#include <stdio.h>    // stderr, stdin, stdout - standard I/O streams
#include <string.h>   // string operations
#include <stdbool.h>  // boolean type and values
#include <unistd.h>   // standard symbolic constants and types
#include <stdlib.h>   // standard library definitions
#include <getopt.h>   // command option parsing
// #include <tcl8.6/expect.h> // exp_spawnv
// #include <tcl8.6/tcl.h> // exp_spawnv
#include <termios.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <pty.h>
#include <fcntl.h>
#include <stropts.h>

#define BUF_SIZE 2

#define TRUE 1
#define FALSE 0

#define sysreturn(x)  return(errno = x, -1)

#define EXP_MATCH_MAX 2000

#define SET_TTYTYPE 1

int exp_ttycopy = TRUE;     /* copy tty parms from /dev/tty */
int exp_ttyinit = TRUE;     /* set tty parms to sane state */
char *exp_stty_init = 0;    /* initial stty args */
int exp_console = FALSE;    /* redirect console */
void (*exp_child_exec_prelude)() = 0;
void (*exp_close_in_child)() = 0;
static int exp_autoallocpty = 1;
static int exp_pty[2];
static pid_t exp_pid;
static int knew_dev_tty;/* true if we had our hands on /dev/tty at any time */
struct termios exp_tty_original, exp_tty_current;
int exp_dev_tty;
char *master_name;
char *slave_name;
struct winsize winsize;
char *exp_pty_slave_name;

static int locked = FALSE;
static char lock[] = "/tmp/ptylock.XXXX"; /* XX is replaced by pty id */

static unsigned int bufsiz = 2*EXP_MATCH_MAX;

static int fd_alloc_max = -1; /* max fd allocated */

static struct f {
  int valid;

  char *buffer;   /* buffer of matchable chars */
  char *buffer_end; /* one beyond end of matchable chars */
  char *match_end;  /* one beyond end of matched string */
  int msize;    /* size of allocate space */
        /* actual size is one larger for null */
} *fs = 0;

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
stream(char *prefix, FILE *in)
{
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

  //turn off buffering on in
  setvbuf(in, NULL, _IONBF, 0);

  while (fgets(buffer, BUF_SIZE, in)) {

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

    // struct winsize ws;

    // exp_autoallocpty = 0;
    // get a pty
    // if (openpty(&exp_pty[0],&exp_pty[0],NULL,NULL,NULL) == -1) {
    //   fprintf(stderr, "openpty error\n");
    //   exit(1);
    // }

    // fcntl(exp_pty[0],F_SETFD,1);

    // if (ioctl(exp_pty[0], TIOCGWINSZ, (char *) &ws) < 0)
    //     fprintf(stderr, "TIOCGWINSZ error\n");
    // if (prefix_set == true)
    //   ws.ws_col -= strlen(prefix);
    // if (ioctl(exp_pty[0], TIOCSWINSZ, (char *) &ws) < 0)
    //   fprintf(stderr,"TIOCSWINSZ error\n");

    // if (tcgetattr(exp_pty[0], &ts) == -1)
    //   fprintf(stderr, "tcgetattr error\n");


    int fint = extra_exp_spawnv(prefix, prefix_set, argv[optind],&argv[optind]);

    // if (ioctl(fint, TIOCGWINSZ, (char *) &ws) < 0)
    //   fprintf(stderr, "TIOCGWINSZ error\n");
    // if (prefix_set == true)
    //   ws.ws_col -= strlen(prefix);
    // if (ioctl(fint, TIOCSWINSZ, (char *) &ws) < 0)
    //   fprintf(stderr,"TIOCSWINSZ error\n");

    FILE *fd = fdopen(fint, "r");
    setbuf(fd,(char *)0);

    if (prefix_set == false)
      stream("", fd);
    else {
      stream(prefix, fd);
    }
  }


}

void
exp_slave_control(master,control)
int master;
int control;  /* if 1, enable pty trapping of close/open/ioctl */
{
}

void
exp_pty_unlock(void)
{
  if (locked) {
    (void) unlink(lock);
    locked = FALSE;
  }
}

int exp_window_size_set(fd)
int fd;
{
  ioctl(fd,TIOCSWINSZ,&winsize);
}

/*static*/ struct termios exp_tty_current, exp_tty_cooked;
#define tty_current exp_tty_current
#define tty_cooked exp_tty_cooked

void
exp_init_tty()
{
  extern struct termios exp_tty_original;

  /* save original user tty-setting in 'cooked', just in case user */
  /* asks for it without earlier telling us what cooked means to them */
  tty_cooked = exp_tty_original;

  /* save our current idea of the terminal settings */
  tty_current = exp_tty_original;
}

static void
pty_stty(s,name)
char *s;    /* args to stty */
char *name;   /* name of pty */
{
#define MAX_ARGLIST 10240
  char buf[MAX_ARGLIST];  /* overkill is easier */
  void (*old)();  /* save old sigalarm handler */

  sprintf(buf,"%s %s > %s","/bin/stty",s,name);
  old = signal(SIGCHLD, SIG_DFL);
  int ret = system(buf);
  signal(SIGCHLD, old); /* restore signal handler */
}

#define GET_TTYTYPE 0
#define SET_TTYTYPE 1
static void
ttytype(request,fd,ttycopy,ttyinit,s)
int request;
int fd;
    /* following are used only if request == SET_TTYTYPE */
int ttycopy;  /* true/false, copy from /dev/tty */
int ttyinit;  /* if true, initialize to sane state */
char *s;  /* stty args */
{
  if (request == GET_TTYTYPE) {
    if (-1 == tcgetattr(fd, &exp_tty_original)) {
      knew_dev_tty = FALSE;
      exp_dev_tty = -1;
    }
    exp_window_size_get(fd);
  } else {  /* type == SET_TTYTYPE */
    if (ttycopy && knew_dev_tty) {
      (void) tcsetattr(fd, TCSADRAIN, &exp_tty_current);
      exp_window_size_set(fd);
    }

/* Apollo Domain doesn't need this */
    if (ttyinit) {
      /* overlay parms originally supplied by Makefile */
/* As long as BSD stty insists on stdout == stderr, we can no longer write */
/* diagnostics to parent stderr, since stderr has is now child's */
/* Maybe someday they will fix stty? */
/*      expDiagLogPtrStr("exp_getptyslave: (default) stty %s\n",DFLT_STTY);*/
      pty_stty("sane",slave_name);
    }

    /* lastly, give user chance to override any terminal parms */
    if (s) {
      /* give user a chance to override any terminal parms */
/*      expDiagLogPtrStr("exp_getptyslave: (user-requested) stty %s\n",s);*/
      pty_stty(s,slave_name);
    }
  }
}

int
exp_getptyslave(
    int ttycopy,
    int ttyinit,
    char *stty_args)
{
  int slave, slave2;
  char buf[10240];

  if (0 > (slave = open(slave_name, O_RDWR))) {
    // static char buf[500];
    // exp_pty_error = buf;
    // sprintf(exp_pty_error,"open(%s,rw) = %d (%s)",slave_name,slave,expErrnoMsg(errno));
    return(-1);
  }

  if (ioctl(slave, I_PUSH, "ptem")) {
    // expDiagLogPtrStrStr("ioctl(%d,I_PUSH,\"ptem\") = %s\n",slave,expErrnoMsg(errno));
  }
  if (ioctl(slave, I_PUSH, "ldterm")) {
    // expDiagLogPtrStrStr("ioctl(%d,I_PUSH,\"ldterm\") = %s\n",slave,expErrnoMsg(errno));
  }
  if (ioctl(slave, I_PUSH, "ttcompat")) {
    // expDiagLogPtrStrStr("ioctl(%d,I_PUSH,\"ttcompat\") = %s\n",slave,expErrnoMsg(errno));
  }


  if (0 == slave) {
    /* if opened in a new process, slave will be 0 (and */
    /* ultimately, 1 and 2 as well) */

    /* duplicate 0 onto 1 and 2 to prepare for stty */
    fcntl(0,F_DUPFD,1);
    fcntl(0,F_DUPFD,2);
  }

  ttytype(SET_TTYTYPE,slave,ttycopy,ttyinit,stty_args);

  (void) exp_pty_unlock();
  return(slave);
}

int exp_window_size_get(fd)
int fd;
{
  ioctl(fd,TIOCGWINSZ,&winsize);
}

int
exp_getptymaster()
{
  char *hex, *bank;
  struct stat stat_buf;
  int master = -1;
  int slave = -1;
  int num;

  if ((master = open("/dev/ptmx", O_RDWR)) == -1) return(-1);
  if ((slave_name = (char *)ptsname(master)) == NULL) {
    close(master);
    return(-1);
  }
  if (grantpt(master)) {
    close(master);
    return(-1);
  }
  if (-1 == (int)unlockpt(master)) {
    close(master);
    return(-1);
  }
  exp_pty_slave_name = slave_name;
  return(master);

}

static struct f *
fd_new(fd)
int fd;
{
  int i, low;
  struct f *fp;
  struct f *newfs;  /* temporary, so we don't lose old fs */

  if (fd > fd_alloc_max) {
    if (!fs) {  /* no fd's yet allocated */
      newfs = (struct f *)malloc(sizeof(struct f)*(fd+1));
      low = 0;
    } else {    /* enlarge fd table */
      newfs = (struct f *)realloc((char *)fs,sizeof(struct f)*(fd+1));
      low = fd_alloc_max+1;
    }
    fs = newfs;
    fd_alloc_max = fd;
    for (i = low; i <= fd_alloc_max; i++) { /* init new entries */
      fs[i].valid = FALSE;
    }
  }

  fp = fs+fd;

  if (!fp->valid) {
    /* initialize */
    fp->buffer = malloc((unsigned)(bufsiz+1));
    if (!fp->buffer) return 0;
    fp->msize = bufsiz;
    fp->valid = TRUE;
  }
  fp->buffer_end = fp->buffer;
  fp->match_end = fp->buffer;
  return fp;

}

static
void
exp_setpgrp()
{
  (void) setpgrp();
}

void
exp_init_pty()
{
  exp_dev_tty = open("/dev/tty",O_RDWR);
  knew_dev_tty = (exp_dev_tty != -1);
  if (knew_dev_tty) ttytype(GET_TTYTYPE,exp_dev_tty,0,0,(char *)0);
}

int
extra_exp_spawnv(prefix,prefix_set,file,argv)
char *prefix;
bool prefix_set;
char *file;
char *argv[]; /* some compiler complains about **argv? */
{
  int cc;
  int errorfd;  /* place to stash fileno(stderr) in child */
      /* while we're setting up new stderr */
  int ttyfd;
  struct winsize ws;
  int sync_fds[2];
  int sync2_fds[2];
  int status_pipe[2];
  int child_errno;
  char sync_byte;

  static int first_time = TRUE;

  if (first_time) {
    first_time = FALSE;
    exp_init_pty();
    exp_init_tty();

    /*
     * TIP 27; It is unclear why this code produces a
     * warning. The equivalent code in exp_main_sub.c
     * (line 512) does not generate a warning !
     */

  }

  if (!file || !argv) sysreturn(EINVAL);

  if (exp_autoallocpty) {
    if (0 > (exp_pty[0] = exp_getptymaster())) sysreturn(ENODEV);
  }
  fcntl(exp_pty[0],F_SETFD,1);  /* close on exec */

  if (!fd_new(exp_pty[0])) {
    errno = ENOMEM;
    return -1;
  }

  if (-1 == (pipe(sync_fds))) {
    return -1;
  }
  if (-1 == (pipe(sync2_fds))) {
    close(sync_fds[0]);
    close(sync_fds[1]);
    return -1;
  }

  if (-1 == pipe(status_pipe)) {
    close(sync_fds[0]);
    close(sync_fds[1]);
    close(sync2_fds[0]);
    close(sync2_fds[1]);
    return -1;
  }

  if ((exp_pid = fork()) == -1) return(-1);
  if (exp_pid) {
    /* parent */
    close(sync_fds[1]);
    close(sync2_fds[0]);
    close(status_pipe[1]);

    /*
     * wait for slave to initialize pty before allowing
     * user to send to it
     */ 

    cc = read(sync_fds[0],&sync_byte,1);
    if (cc == -1) {
      return -1;
    }

    /* turn on detection of eof */
    exp_slave_control(exp_pty[0],1);

    /*
     * tell slave to go on now now that we have initialized pty
     */

    cc = write(sync2_fds[1]," ",1);
    if (cc == -1) {
      return -1;
    }

    close(sync_fds[0]);
    close(sync2_fds[1]);

    /* see if child's exec worked */

  retry:
    switch (read(status_pipe[0],&child_errno,sizeof child_errno)) {
    case -1:
      if (errno == EINTR) goto retry;
      /* well it's not really the child's errno */
      /* but it can be treated that way */
      child_errno = errno;
      break;
    case 0:
      /* child's exec succeeded */
      child_errno = 0;
      break;
    default:
      /* child's exec failed; err contains exec's errno  */
      waitpid(exp_pid, NULL, 0);
      errno = child_errno;
      exp_pty[0] = -1;
    }
    close(status_pipe[0]);
    return(exp_pty[0]);
  }

  /*
   * child process - do not return from here!  all errors must exit()
   */

  close(sync_fds[0]);
  close(sync2_fds[1]);
  close(status_pipe[0]);
  fcntl(status_pipe[1],F_SETFD,1);  /* close on exec */


/* ultrix (at least 4.1-2) fails to obtain controlling tty if setsid */
/* is called.  setpgrp works though.  */
  setsid();
  exp_setpgrp();


  ttyfd = open("/dev/tty", O_RDWR);
  if (ttyfd >= 0) {
    (void) ioctl(ttyfd, TIOCNOTTY, (char *)0);
    (void) close(ttyfd);
  }

  /* save error fd while we're setting up new one */
  errorfd = fcntl(2,F_DUPFD,3);
  /* and here is the macro to restore it */
#define restore_error_fd {close(2);fcntl(errorfd,F_DUPFD,2);}

  if (exp_autoallocpty) {

      close(0);
      close(1);
      close(2);

      /* since we closed fd 0, open of pty slave must return fd 0 */

      if (0 > (exp_pty[1] = exp_getptyslave(exp_ttycopy,exp_ttyinit,
            exp_stty_init))) {
    restore_error_fd
    exit(-1);
      }
      /* sanity check */
      if (exp_pty[1] != 0) {
    restore_error_fd
    fprintf(stderr,"exp_getptyslave: slave = %d but expected 0\n",
                exp_pty[1]);
    exit(-1);
      }
  } else {
    if (exp_pty[1] != 0) {
      close(0); fcntl(exp_pty[1],F_DUPFD,0);
    }
    close(1);   fcntl(0,F_DUPFD,1);
    close(2);   fcntl(0,F_DUPFD,1);
    close(exp_pty[1]);
  }

/* The test for hpux may have to be more specific.  In particular, the */
/* code should be skipped on the hp9000s300 and hp9000s720 (but there */
/* is no documented define for the 720!) */

  (void) ioctl(0,TIOCSCTTY,(char *)0);
  /* ignore return value - on some systems, it is defined but it
   * fails and it doesn't seem to cause any problems.  Or maybe
   * it works but returns a bogus code.  Noone seems to be able
   * to explain this to me.  The systems are an assortment of
   * different linux systems (and FreeBSD 2.5), RedHat 5.2 and
   * Debian 2.0
   */

  if (exp_console) {
    int on = 1;
    if (ioctl(0,TIOCCONS,(char *)&on) == -1) {
      restore_error_fd
      fprintf(stderr, "spawn %s: cannot open console, check permissions of /dev/console\n",argv[0]);
      exit(-1);
    }
  }

  /* tell parent that we are done setting up pty */
  /* The actual char sent back is irrelevant. */

  cc = write(sync_fds[1]," ",1);
  if (cc == -1) {
    restore_error_fd
    exit(-1);
  }
  close(sync_fds[1]);

  /* wait for master to let us go on */
  cc = read(sync2_fds[0],&sync_byte,1);
  if (cc == -1) {
    restore_error_fd
    exit(-1);
  }
  close(sync2_fds[0]);

  /* (possibly multiple) masters are closed automatically due to */
  /* earlier fcntl(,,CLOSE_ON_EXEC); */

  /* just in case, allow user to explicitly close other files */
  if (exp_close_in_child) (*exp_close_in_child)();

  /* allow user to do anything else to child */
  if (exp_child_exec_prelude) (*exp_child_exec_prelude)();

  if (ioctl(exp_pty[0], TIOCGWINSZ, (char *) &ws) < 0)
    fprintf(stderr, "TIOCGWINSZ error\n");
  if (prefix_set == true && ws.ws_col > strlen(prefix))
    ws.ws_col -= strlen(prefix);
  if (ioctl(exp_pty[0], TIOCSWINSZ, (char *) &ws) < 0)
    fprintf(stderr,"TIOCSWINSZ error\n");

  (void) execvp(file,argv);

  /* Unfortunately, by now we've closed fd's to stderr, logfile
   * and debugfile.  The only reasonable thing to do is to send
   * *back the error as part of the program output.  This will
   * be *picked up in an expect or interact command.
   */

  int ret = write(status_pipe[1], &errno, sizeof errno);
  exit(-1);
  /*NOTREACHED*/
}