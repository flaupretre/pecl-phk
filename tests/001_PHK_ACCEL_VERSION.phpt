--TEST--
PHK_ACCEL_VERSION
--SKIPIF--
<?php if (!extension_loaded("phk")) print "skip"; ?>
--FILE--
<?php
var_dump(PHK_ACCEL_VERSION);
?>
===DONE===
--EXPECT--
string(5) "2.1.0"
===DONE===
