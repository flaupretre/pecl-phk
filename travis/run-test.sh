#!/bin/sh
#
# This script is needed because 'make test' does not always report
# a non-zero code when it fails.

# -m : test using valgrind
# -q : No interaction

REPORT_EXIT_STATUS=1 TEST_PHP_ARGS='-m -q' USE_ZEND_ALLOC=0 make test

ret=0

for i in tests/*.diff tests/*.mem
	do
	[ -f "$i" ] || continue
	ret=1
	echo
	echo "----------------------------"
	echo "$i"
	echo "----------------------------"
	cat $i
done

exit $ret
