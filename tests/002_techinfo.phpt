--TEST--
_phk_techinfo
--SKIPIF--
<?php if (!extension_loaded("phk")) print "skip"; ?>
--FILE--
<?php
PHK::accel_techinfo();
?>
===DONE===
--EXPECT--
Using PHK Accelerator: Yes
Accelerator Version: 2.0.0
===DONE===
