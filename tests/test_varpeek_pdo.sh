#!/bin/bash

# PDOStatement::execute
read -r -d '' php_src <<'EOD'
<?php
$pdo = new PDO('sqlite::memory:');
$pdo->sqliteCreateFunction('slow', function ($x) { sleep(1); return $x; }, 1);
$stmt = $pdo->prepare("SELECT slow(:name) AS r WHERE :age > 0");
$stmt->execute([':name' => 'hello', ':age' => 42]);
EOD
php_file=$(mktemp)
echo "$php_src" >$php_file
phpspy_opts=(--limit=1 --peek-pdo -- $PHP $php_file)
declare -A expected
expected[pdo_sql_execute_array ]="^# varpeek #pdo_sql@PDOStatement::execute = SELECT slow\(:name\) AS r WHERE :age > 0"
expected[pdo_args_execute_array]="^# varpeek #pdo_args@PDOStatement::execute = :name=hello,:age=42$"
source $TEST_SH
rm -f $php_file

# PDOStatement::execute with packed (positional) array
read -r -d '' php_src <<'EOD'
<?php
$pdo = new PDO('sqlite::memory:');
$pdo->sqliteCreateFunction('slow', function ($x) { sleep(1); return $x; }, 1);
$stmt = $pdo->prepare("SELECT slow(?) AS r WHERE ? > 0 AND ? > 0");
$stmt->execute(['hello', 42, 99]);
EOD
php_file=$(mktemp)
echo "$php_src" >$php_file
phpspy_opts=(--limit=1 --peek-pdo -- $PHP $php_file)
declare -A expected
expected[pdo_args_packed_array]="^# varpeek #pdo_args@PDOStatement::execute = 0=hello,1=42,2=99$"
source $TEST_SH
rm -f $php_file

# bindValue with positional placeholders (packed bound_params)
read -r -d '' php_src <<'EOD'
<?php
$pdo = new PDO('sqlite::memory:');
$pdo->setAttribute(PDO::ATTR_EMULATE_PREPARES, true);
$pdo->sqliteCreateFunction('slow', function ($x) { sleep(1); return $x; }, 1);
$stmt = $pdo->prepare("SELECT slow(?) AS r, ? AS x, ? AS y");
$stmt->bindValue(1, 'apple');
$stmt->bindValue(2, 11);
$stmt->bindValue(3, 22);
$stmt->execute();
EOD
php_file=$(mktemp)
echo "$php_src" >$php_file
phpspy_opts=(--limit=1 --peek-pdo -- $PHP $php_file)
declare -A expected
expected[pdo_args_packed_binds]="^# varpeek #pdo_args@PDOStatement::execute = 0=apple,1=11,2=22$"
source $TEST_SH
rm -f $php_file

# bindValue / bindParam + execute
read -r -d '' php_src <<'EOD'
<?php
$pdo = new PDO('sqlite::memory:');
$pdo->sqliteCreateFunction('slow', function ($x) { sleep(1); return $x; }, 1);
$stmt = $pdo->prepare("SELECT slow(:name) AS r, :age AS age");
$stmt->bindValue(':name', 'apple', PDO::PARAM_STR);
$age = 99;
$stmt->bindParam(':age', $age, PDO::PARAM_INT);
$stmt->execute();
EOD
php_file=$(mktemp)
echo "$php_src" >$php_file
phpspy_opts=(--limit=1 --peek-pdo -- $PHP $php_file)
declare -A expected
expected[pdo_sql_bound  ]="^# varpeek #pdo_sql@PDOStatement::execute = SELECT slow\(:name\) AS r, :age AS age"
expected[pdo_args_bound ]="^# varpeek #pdo_args@PDOStatement::execute = :name=apple,:age=99$"
source $TEST_SH
rm -f $php_file

# PDO::query
read -r -d '' php_src <<'EOD'
<?php
$pdo = new PDO('sqlite::memory:');
$pdo->sqliteCreateFunction('slow', function ($x) { sleep(1); return $x; }, 1);
foreach ($pdo->query("SELECT slow('banana')") as $row) {}
EOD
php_file=$(mktemp)
echo "$php_src" >$php_file
phpspy_opts=(--limit=1 --peek-pdo -- $PHP $php_file)
declare -A expected
expected[pdo_sql_pdo_query]="^# varpeek #pdo_sql@PDO::query = SELECT slow\('banana'\)"
source $TEST_SH
rm -f $php_file

# no peek
read -r -d '' php_src <<'EOD'
<?php
$pdo = new PDO('sqlite::memory:');
$pdo->sqliteCreateFunction('slow', function ($x) { sleep(1); return $x; }, 1);
$stmt = $pdo->prepare("SELECT slow('no')");
$stmt->execute();
EOD
php_file=$(mktemp)
echo "$php_src" >$php_file
phpspy_opts=(--limit=1 -- $PHP $php_file)
declare -A expected
declare -A not_expected
not_expected[no_pdo_lines_without_flag]="^# varpeek #pdo_"
source $TEST_SH
rm -f $php_file
