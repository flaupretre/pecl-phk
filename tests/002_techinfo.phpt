--TEST--
_phk_techinfo
--SKIPIF--
<?php if (!extension_loaded("phk")) print "skip"; ?>
--FILE--
<?php
PHK::techinfo();
?>
===DONE===
--EXPECT--
Using PHK Accelerator: Yes
Accelerator Version: 1.1.0
===DONE===
