dnl
dnl $Id$
dnl

PHP_ARG_ENABLE(automap, whether to enable the Automap extension,
[  --enable-automap    Enable the Automap extension])

if test "$PHP_AUTOMAP" != "no"; then
  AC_DEFINE(HAVE_AUTOMAP, 1, [Whether you have the Automap extension])
  PHP_NEW_EXTENSION(automap, php_automap.c, $ext_shared)
fi
