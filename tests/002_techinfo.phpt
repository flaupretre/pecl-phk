--TEST--
_phk_techinfo
--SKIPIF--
<?php if (!extension_loaded("phk")) print "skip"; ?>
--INI--
phk.enable_cli=1
--FILE--
<?php
PHK::accelTechInfo();
?>
===DONE===
--EXPECT--
Using PHK Accelerator: Yes
Accelerator Version: 3.0.1
===DONE===
