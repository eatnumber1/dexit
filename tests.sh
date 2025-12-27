#!/bin/bash

run_test() {
  eval "$*"
  if [[ $? -ne 0 ]]; then
    echo "Test '$*' failed!" >&2
  fi
}

run_test './dexit -h >/dev/null'
run_test '! ./dexit 2>/dev/null'
run_test './dexit 0 >/dev/null'
run_test './dexit 1 >/dev/null'
run_test './dexit -n 0 >/dev/null'
run_test '! ./dexit -se 2>/dev/null'
run_test './dexit -s 129'
run_test './dexit --if-signaled 129'
run_test '! ./dexit -s 255'
run_test '! ./dexit -s 127'
run_test '! ./dexit -s 1'
run_test '! ./dexit -s 0'
run_test './dexit -g 129 >/dev/null'
run_test '! ./dexit -g 0'
run_test './dexit -x 1 >/dev/null'
run_test '! ./dexit -x 129 >/dev/null'
run_test './dexit 160 >/dev/null'
run_test 'test "$(./dexit -g 129)" == 1'
run_test 'test "$(./dexit -x 1)" == 1'
run_test 'test "$(./dexit 0)" == "EXIT_SUCCESS"'
run_test 'test "$(./dexit 1)" == "EXIT_FAILURE"'
run_test 'test "$(SHELL=/bin/bash ./dexit 127)" == "bash: command not found"'
run_test 'test "$(SHELL=/bin/bash ./dexit 126)" == "bash: command not executable"'
run_test 'test "$(SHELL=/bin/zsh ./dexit 127)" == "exit code 127"'
run_test 'test "$(SHELL=/bin/zsh ./dexit 126)" == "exit code 126"'
run_test 'test "$(./dexit 71)" == "EX_OSERR: system error (e.g. can'"'"'t fork)"'
run_test 'test "$(./dexit 3)" == "exit code 3"'
run_test 'test "$(./dexit 100)" == "exit code 100"'
run_test 'test "$(./dexit 128)" == "exit code 128"'
run_test '[[ "$(./dexit 129)" =~ "SIGHUP: Hangup".* ]]'
