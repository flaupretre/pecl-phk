--TEST--
_phk_techinfo
--SKIPIF--
<?php if (!extension_loaded("phk")) print "skip"; ?>
--FILE--
<?php
PHK::accelTechInfo();
?>
===DONE===
--EXPECT--
Using PHK Accelerator: Yes
Accelerator Version: 3.0.0
===DONE===
