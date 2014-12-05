
PHP_ARG_ENABLE(phk, whether to enable the PHK accelerator,
[  --enable-phk    Enable the PHK accelerator])

if test "$PHP_PHK" != "no"; then
  AC_DEFINE(HAVE_PHK, 1, [Whether you have the PHK accelerator])
  PHP_NEW_EXTENSION(phk, php_phk.c, $ext_shared)
fi
