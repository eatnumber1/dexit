//
// Author: Russell Harmon <russ@har.mn>
// License: MIT
//
#define _GNU_SOURCE
#include <assert.h>
#include <ctype.h>
#include <libgen.h>
#include <string.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sysexits.h>
#include <stdarg.h>

static const char *argv0 = NULL;

static void VFPrintfOrDie(FILE *out, const char *format, va_list args) {
  int rc = vfprintf(out, format, args);
  if (rc < 0) err(EX_IOERR, "vfprintf");
}

static void FPrintfOrDie(FILE *out, const char *format, ...) {
  va_list args;
  va_start(args, format);
  VFPrintfOrDie(out, format, args);
  va_end(args);
}

static void PrintfOrDie(const char *format, ...) {
  va_list args;
  va_start(args, format);
  VFPrintfOrDie(stdout, format, args);
  va_end(args);
}

__attribute__((noreturn))
static void usage_and_die(FILE *out, int exit_status) {
  // glibc's program_invocation_short_name, or BSD's getprogname() would be
  // nicer than argv0, but not worth the code complexity to use since they're
  // not portable. gnulib has a portable implementation, but isn't worth taking
  // a dependency on.
  FPrintfOrDie(
    out,
    "Usage: %s [options] <exit status>\n"
    "\n"
    "Tests:\n"
    "  -s, --if-signaled    Check whether the program died due to a signal.\n"
    "  -e, --if-exited      Check whether the program died due to a 'normal'\n"
    "                       exit (e.g. return from main or call to exit(3) or\n"
    "                       _exit(2)).\n"
    "\n"
    "Decoding:\n"
    "  -g, --signal-number  Retrieve the signal number from the exit status.\n"
    "  -x, --exit-number    Retrieve the exit number from the exit status.\n"
    "  -n, --name           Decode the exit status to a name (where\n"
    "                       possible). This is the default behavior.\n"
    "\n"
    "  The flags -g and -x will cause dexit to exit with an EX_USAGE error if\n"
    "  the program did not exit due to a signal or normal exit respectively.\n"
    "\n"
    "  The flag -n (default) will decode signal exit codes into their signal\n"
    "  name, 0 or 1 into EXIT_SUCCESS or EXIT_FAILURE respectively, and\n"
    "  sysexits.h error codes where defined. Otherwise it will produce the\n"
    "  exit number.\n"
    "\n"
    "Other flags:\n"
    "  -h, --help           This help text.\n"
    "\n"
    "Arguments:\n"
    "  exit status - The exit status returned from a call to wait(2) (which \n"
    "                is $? on many shells)\n"
    "\n"
    "Environment:\n"
    "  SHELL - if the shell is bash, the bash-specific error codes 126 and 127\n"
    "          are decoded as 'command not executable' and 'command not found'\n"
    "          respectively\n"
    "\n"
    "Author: Russell Harmon <russ@har.mn>\n"
    "License: MIT\n",
    argv0);
  exit(exit_status);
}

struct FlagsT {
  bool if_signaled;
  bool if_exited;
  bool signal_number;
  bool exit_number;
  bool name;
};
typedef struct FlagsT Flags;

static Flags DefaultFlags() {
  Flags flags = {
    .if_signaled = false,
    .if_exited = false,
    .signal_number = false,
    .exit_number = false,
    .name = false,
  };
  return flags;
}

// Mutates argc/argv to remove flags *and* argv[0].
static Flags ParseFlagsOrDie(int *argc, char **argv[]) {
  Flags flags = DefaultFlags();
  while (true) {
    int option_index = 0;
    static struct option long_options[] = {
      {"help", no_argument, /*flag=*/NULL, /*val=*/'h' },
      {"if-signaled", no_argument, /*flag=*/NULL, /*val=*/'s' },
      {"if-exited", no_argument, /*flag=*/NULL, /*val=*/'e' },
      {"signal-number", no_argument, /*flag=*/NULL, /*val=*/'g' },
      {"exit-number", no_argument, /*flag=*/NULL, /*val=*/'x' },
      {"name", no_argument, /*flag=*/NULL, /*val=*/'n' },
      { NULL, 0, NULL, 0 }
    };
    int c = getopt_long(*argc, *argv, "hsegxn", long_options, &option_index);
    if (c == -1) break;
    switch (c) {
    case 'h':
      usage_and_die(stdout, EXIT_SUCCESS);
    case 's':
      flags.if_signaled = true;
      break;
    case 'e':
      flags.if_exited = true;
      break;
    case 'g':
      flags.signal_number = true;
      break;
    case 'x':
      flags.exit_number = true;
      break;
    case 'n':
      flags.name = true;
      break;
    case '?':
      usage_and_die(stderr, EX_USAGE);
    default:
      FPrintfOrDie(stderr, "?? getopt return character code 0%o ??\n", c);
      exit(EX_SOFTWARE);
    }
  }
  *argc -= optind;
  *argv += optind;
  return flags;
}

static int AtoIOrDie(const char *str) {
  char *str_end = NULL;
  errno = 0;
  long val = strtol(str, &str_end, /*base=*/10);
  if (errno != 0) err(EX_USAGE, "'%s' is not convertable to an int", str);
  if (val > INT_MAX || val < INT_MIN) errx(EX_USAGE, "%ld is out of range for an int", val);
  while (*str_end != '\0') {
    if (!isspace(*str_end)) {
      errno = EINVAL;
      err(EX_USAGE, "'%s' is not convertable to an int", str);
    }
    ++str_end;
  }
  return (int)val;
}

static void ValidateFlagsOrDie(Flags flags) {
  int num_mutex_flags = 0;
  if (flags.if_signaled) num_mutex_flags++;
  if (flags.if_exited) num_mutex_flags++;
  if (flags.signal_number) num_mutex_flags++;
  if (flags.exit_number) num_mutex_flags++;
  if (flags.name) num_mutex_flags++;
  if (num_mutex_flags > 1) {
    FPrintfOrDie(stderr, "At most one of -s, -e, -c, -g, -x, or -n may be used\n");
    exit(EX_USAGE);
  }
}

// Returns -1 if the status is not a signal number.
static int ExitStatusToSignalNumber(int status) {
  // Per https://www.gnu.org/software/bash/manual/bash.html#Exit-Status
  //
  // > When a command terminates on a fatal signal whose number is N, Bash uses
  // > the value 128+N as the exit status.
  //
  // Same for zsh https://zsh.sourceforge.io/Doc/Release/Shell-Grammar.html
  //
  // > The value of a simple command is its exit status, or 128 plus the signal
  // > number if terminated by a signal.
  //
  // Similar shells have the same behavior (dash, etc.).
  //
  // Note: This does *not* mean that a command definitely exited with a signal.
  // Commands are allowed to exit with a return code in the range [128,255]
  // without a signal. E.g. `bash -c 'exit 129'`, which is not detectable.
  if (status <= 128) return -1;
  int signo = status - 128;
  if (signo >= NSIG) return -1;
  return signo;
}

static bool IsSignalExit(int status) {
  int signo = ExitStatusToSignalNumber(status);
  return signo != -1;
}

static bool IsNormalExit(int status) {
  return !IsSignalExit(status);
}

static const char *SysexitToString(int exit_code) {
  switch (exit_code) {
#define C(x) case x: return #x
  C(EX_OK) ": successful termination";
  C(EX_USAGE) ": command line usage error";
  C(EX_DATAERR) ": data format error";
  C(EX_NOINPUT) ": cannot open input";
  C(EX_NOUSER) ": addressee unknown";
  C(EX_NOHOST) ": host name unknown";
  C(EX_UNAVAILABLE) ": service unavailable";
  C(EX_SOFTWARE) ": internal software error";
  C(EX_OSERR) ": system error (e.g. can't fork)";
  C(EX_OSFILE) ": critical OS file missing";
  C(EX_CANTCREAT) ": can't create (user) output file";
  C(EX_IOERR) ": input/output error";
  C(EX_TEMPFAIL) ": temp failure; user is invited to retry";
  C(EX_PROTOCOL) ": remote error in protocol";
  C(EX_NOPERM) ": permission denied";
  C(EX_CONFIG) ": configuration error";
#undef C
  }
  return NULL;
}

// Returns a string describing the bash-specific exit codes documented at
// https://www.gnu.org/software/bash/manual/bash.html#Exit-Status, or a null
// pointer if the code is not one of the bash-specific exit codes.
static const char *BashExitToString(int exit_code) {
  switch (exit_code) {
  case 127: return "bash: command not found";
  case 126: return "bash: command not executable";
  }
  return NULL;
}

static bool IsUsingBash() {
  char *shell = getenv("SHELL");
  if (shell == NULL) return false;

  // basename modifies its argument, so we need to copy the string
  size_t shell_len = strlen(shell);
  char shellcpy[shell_len+1];
  memcpy(shellcpy, shell, shell_len+1);
  if (strcmp(basename(shellcpy), "bash") != 0) return false;

  return true;
}

static const char *CExitToString(int exit_code) {
  switch (exit_code) {
  case EXIT_SUCCESS: return "EXIT_SUCCESS";
  case EXIT_FAILURE: return "EXIT_FAILURE";
  }
  return NULL;
}

#ifndef SIG2STR_MAX
// Just guessing what the max signal name length is.
#define SIG2STR_MAX 64
#endif  // SIG2STR_MAX

// sig2str is defined by POSIX, but not implemented (yet) on MacOS or glibc.
// https://pubs.opengroup.org/onlinepubs/9799919799/functions/sig2str.html
#if defined(__APPLE__)
int sig2str(int signum, char *str) {
  if (signum >= NSIG) return -1;
  strcpy(str, sys_signame[signum]);
  return 0;
}
#elif defined(__GLIBC__)
int sig2str(int signum, char *str) {
  if (signum >= NSIG) return -1;
  // The realtime signals are not supported by sigabbrev_np, so we emulate them.
  // SIGRTMIN and SIGRTMAX are runtime-determined, however the support in
  // sigabbrev_np is compile-time decided. So instead we use glibc's internal
  // __SIGRTMIN and __SIGRTMAX which are the limits of SIGRTMIN/SIGRTMAX and
  // are what sigabbrev_np uses.
  if (signum >= __SIGRTMIN && signum <= __SIGRTMAX) {
    sprintf(str, "SIGRTMIN+%d", signum - __SIGRTMIN);
    return 0;
  }
  const char *abbrev = sigabbrev_np(signum);
  assert(abbrev != NULL);
  strcpy(str, abbrev);
  return 0;
}
#endif  // defined(__APPLE__) || defined(__GLIBC__)

// Converts the string *in-place* to uppercase.
void Uppercase(char *str) {
  while (*str != '\0') {
    int c = toupper(*str);
    *str = c;
    ++str;
  }
}

// The return value must be freed.
__attribute__((malloc))
static char *SignalExitToString(int exit_status) {
  int signum = ExitStatusToSignalNumber(exit_status);
  if (signum == -1) return NULL;

  char abbrev[SIG2STR_MAX];
  if (sig2str(signum, abbrev) == -1) return NULL;
  Uppercase(abbrev);
  const char *descr = strsignal(signum);
  char *ret = NULL;
  int rc = asprintf(&ret, "SIG%s: %s", abbrev, descr);
  if (rc == -1) err(EX_SOFTWARE, "asprintf");
  return ret;
}

static void PrintExitStatusNameOrDie(int status) {
  char *sigmsg = SignalExitToString(status);
  if (sigmsg != NULL) {
    PrintfOrDie("%s\n", sigmsg);
    free(sigmsg);
    return;
  }

  assert(IsNormalExit(status));

  const char *msg = CExitToString(status);
  if (msg != NULL) {
    PrintfOrDie("%s\n", msg);
    return;
  }

  msg = SysexitToString(status);
  if (msg != NULL) {
    PrintfOrDie("%s\n", msg);
    return;
  }

  msg = BashExitToString(status);
  if (msg != NULL && IsUsingBash()) {
    PrintfOrDie("%s\n", msg);
    return;
  }

  PrintfOrDie("%d\n", status);
}

int main(int argc, char *argv[]) {
  argv0 = argv[0];
  Flags flags = ParseFlagsOrDie(&argc, &argv);

  if (argc != 1) {
    FPrintfOrDie(stderr, "Must provide the exit status as the sole non-flag argument\n");
    usage_and_die(stderr, EX_USAGE);
  }
  int status = AtoIOrDie(argv[0]);

  ValidateFlagsOrDie(flags);

  if (flags.if_exited) {
    return IsNormalExit(status) ? EXIT_SUCCESS : EXIT_FAILURE;
  }
  if (flags.if_signaled) {
    return IsSignalExit(status) ? EXIT_SUCCESS : EXIT_FAILURE;
  }
  if (flags.signal_number) {
    if (!IsSignalExit(status)) return EXIT_FAILURE;
    int signum = ExitStatusToSignalNumber(status);
    PrintfOrDie("%d\n", signum);
    return EXIT_SUCCESS;
  }
  if (flags.exit_number) {
    if (!IsNormalExit(status)) return EXIT_FAILURE;
    PrintfOrDie("%d\n", status);
    return EXIT_SUCCESS;
  }
  // -n or no flags were passed
  PrintExitStatusNameOrDie(status);
  return EXIT_SUCCESS;
}
