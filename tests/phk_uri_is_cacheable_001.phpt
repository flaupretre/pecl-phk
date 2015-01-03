--TEST--
PHK URI is cacheable
--SKIPIF--
<?php if (!function_exists("file_is_cacheable")) die("skip"); ?>
--FILE--
<?php
function check_cacheable($path)
{
$ret=file_is_cacheable($path);
echo "$path is ".($ret ? '' : 'not ')."cacheable\n";
}
//-----------
check_cacheable('phk://bad_id/Foo/Bar.php');

//TODO:
// Mount valid package
// Test valid path is cacheable
// Test command on path (containing '?') is not cacheable
// Test global command on mounted package is not cacheable
// mount package with 'no_opcode_cache' option
// Check that valid path in this package is not cacheable
?>
--EXPECTF--
%s is not cacheable
