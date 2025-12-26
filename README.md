# dexit: Decode Exit

dexit is a small program meant to be used to decode shell exit codes (`$?` in
most shells) into readable error messages.

It can also be used as a way to test for various kinds of errors.

## Generating Error Messages

There are several different kinds of error codes. `dexit` supports the
following:

 - C (`EXIT_SUCCESS` and `EXIT_FAILURE`)
   ```sh
   $ true; dexit $?
   EXIT_SUCCESS
   $ false; dexit $?
   EXIT_FAILURE
   ```
 - Signals
   ```sh
   $ bash -c 'kill -TERM $$'; dexit $?
   SIGTERM: Terminated: 15
   ```
 - [`sysexits.h`]
   ```sh
   $ python -c 'import os; import sys; sys.exit(os.EX_OSERR);'; dexit $?
   EX_OSERR: system error (e.g. can't fork)
   ```
 - Bash ([exit codes 126 and
   127](https://www.gnu.org/software/bash/manual/bash.html#Exit-Status))
   ```sh
   $ SHELL=/bin/bash /bin/bash -c 'asdf; dexit $?'
   /bin/bash: asdf: command not found
   bash: command not found
   ```
 - Unknown exit codes are just printed
   ```sh
   $ dexit 3
   3
   ```

## Testing for Error Kinds

```sh
$ bash -c 'kill -TERM $$'
$ dexit -s $? && echo 'Died due to signal'
Died due to signal
```

```sh
$ bash -c 'exit 1'
$ dexit -e $? && echo 'Died due to regular exit'
Died due to regular exit
```

## Usage

```
Usage: dexit [options] <exit status>

Tests:
  -s, --if-signaled    Check whether the program died due to a signal.
  -e, --if-exited      Check whether the program died due to a 'normal'
                       exit (e.g. return from main or call to exit(3) or
                       _exit(2)).

Decoding:
  -g, --signal-number  Retrieve the signal number from the exit status.
  -x, --exit-number    Retrieve the exit number from the exit status.
  -n, --name           Decode the exit status to a name (where
                       possible). This is the default behavior.

  The flags -g and -x will cause dexit to exit with an EX_USAGE error if
  the program did not exit due to a signal or normal exit respectively.

  The flag -n (default) will decode signal exit codes into their signal
  name, 0 or 1 into EXIT_SUCCESS or EXIT_FAILURE respectively, and
  sysexits.h error codes where defined. Otherwise it will produce the
  exit number.

Other flags:
  -h, --help           This help text.

Arguments:
  exit status - The exit status returned from a call to wait(2) (which
                is $? on many shells)

Environment:
  SHELL - if the shell is bash, the bash-specific error codes 126 and 127
          are decoded as 'command not executable' and 'command not found'
          respectively

Author: Russell Harmon <russ@har.mn>
License: MIT
```

# Tip

Use dexit in your shell scripts as a wrapper around `exit $?` to get better
errors. Here's how to do it in bash:

```bash
# Usage: dexit <exit status>
function dexit {
  local rc=$?
  [[ $# -eq 1 ]] && rc="$1"
  [[ $# -le 1 ]] || exit 1  # dexit flags not supported
  command dexit "$rc"
  exit "$rc"
}

command_that_may_fail || dexit
```

[sysexits.h]: https://www.man7.org/linux/man-pages/man3/sysexits.h.3head.html
