#!./perl -w


use strict ;


use lib 't' ;
use BerkeleyDB; 
use util ;

BEGIN
{
    if ($BerkeleyDB::db_version < 3.3) {
        print "1..0 # Skip: this needs Berkeley DB 3.3.x or better\n" ;
        exit 0 ;
    }
}     

umask(0);

print "1..130\n";        

{
    # db->truncate

    my $Dfile;
    my $lex = new LexFile $Dfile ;
    my %hash ;
    my ($k, $v) ;
    ok 1, my $db = new BerkeleyDB::Hash -Filename => $Dfile, 
				     -Flags    => DB_CREATE ;

    # create some data
    my %data =  (
		"red"	=> 2,
		"green"	=> "house",
		"blue"	=> "sea",
		) ;

    my $ret = 0 ;
    while (($k, $v) = each %data) {
        $ret += $db->db_put($k, $v) ;
    }
    ok 2, $ret == 0 ;

    # check there are three records
    ok 3, countRecords($db) == 3 ;

    # now truncate the database
    my $count = 0;
    ok 4, $db->truncate($count) == 0 ;

    ok 5, $count == 3 ;
    ok 6, countRecords($db) == 0 ;

}

{
    # db->associate -- secondary keys

    sub sec_key
    {
        #print "in sec_key\n";
        my $pkey = shift ;
        my $pdata = shift ;

       $_[0] = $pdata ;
        return 0;
    }

    my ($Dfile1, $Dfile2);
    my $lex = new LexFile $Dfile1, $Dfile2 ;
    my %hash ;
    my $status;
    my ($k, $v, $pk) = ('','','');

    # create primary database
    ok 7, my $primary = new BerkeleyDB::Hash -Filename => $Dfile1, 
				     -Flags    => DB_CREATE ;

    # create secondary database
    ok 8, my $secondary = new BerkeleyDB::Hash -Filename => $Dfile2, 
				     -Flags    => DB_CREATE ;

    # associate primary with secondary
    ok 9, $primary->associate($secondary, \&sec_key) == 0;

    # add data to the primary
    my %data =  (
		"red"	=> "flag",
		"green"	=> "house",
		"blue"	=> "sea",
		) ;

    my $ret = 0 ;
    while (($k, $v) = each %data) {
        my $r = $primary->db_put($k, $v) ;
	#print "put $r $BerkeleyDB::Error\n";
        $ret += $r;
    }
    ok 10, $ret == 0 ;

    # check the records in the secondary
    ok 11, countRecords($secondary) == 3 ;

    ok 12, $secondary->db_get("house", $v) == 0;
    ok 13, $v eq "house";

    ok 14, $secondary->db_get("sea", $v) == 0;
    ok 15, $v eq "sea";

    ok 16, $secondary->db_get("flag", $v) == 0;
    ok 17, $v eq "flag";

    # pget to primary database is illegal
    ok 18, $primary->db_pget('red', $pk, $v) != 0 ;

    # pget to secondary database is ok
    ok 19, $secondary->db_pget('house', $pk, $v) == 0 ;
    ok 20, $pk eq 'green';
    ok 21, $v  eq 'house';

    ok 22, my $p_cursor = $primary->db_cursor();
    ok 23, my $s_cursor = $secondary->db_cursor();

    # c_get from primary 
    $k = 'green';
    ok 24, $p_cursor->c_get($k, $v, DB_SET) == 0;
    ok 25, $k eq 'green';
    ok 26, $v eq 'house';

    # c_get from secondary
    $k = 'sea';
    ok 27, $s_cursor->c_get($k, $v, DB_SET) == 0;
    ok 28, $k eq 'sea';
    ok 29, $v eq 'sea';

    # c_pget from primary database should fail
    $k = 1;
    ok 30, $p_cursor->c_pget($k, $pk, $v, DB_FIRST) != 0;

    # c_pget from secondary database 
    $k = 'flag';
    ok 31, $s_cursor->c_pget($k, $pk, $v, DB_SET) == 0;
    ok 32, $k eq 'flag';
    ok 33, $pk eq 'red';
    ok 34, $v eq 'flag';

    # check put to secondary is illegal
    ok 35, $secondary->db_put("tom", "dick") != 0;
    ok 36, countRecords($secondary) == 3 ;

    # delete from primary
    ok 37, $primary->db_del("green") == 0 ;
    ok 38, countRecords($primary) == 2 ;

    # check has been deleted in secondary
    ok 39, $secondary->db_get("house", $v) != 0;
    ok 40, countRecords($secondary) == 2 ;

    # delete from secondary
    ok 41, $secondary->db_del('flag') == 0 ;
    ok 42, countRecords($secondary) == 1 ;


    # check deleted from primary
    ok 43, $primary->db_get("red", $v) != 0;
    ok 44, countRecords($primary) == 1 ;

}


    # db->associate -- multiple secondary keys


    # db->associate -- same again but when DB_DUP is specified.


{
    # db->associate -- secondary keys, each with a user defined sort

    sub sec_key2
    {
        my $pkey = shift ;
        my $pdata = shift ;
        #print "in sec_key2 [$pkey][$pdata]\n";

        $_[0] = length $pdata ;
        return 0;
    }

    my ($Dfile1, $Dfile2);
    my $lex = new LexFile $Dfile1, $Dfile2 ;
    my %hash ;
    my $status;
    my ($k, $v, $pk) = ('','','');

    # create primary database
    ok 45, my $primary = new BerkeleyDB::Btree -Filename => $Dfile1, 
				     -Compare  => sub { return $_[0] cmp $_[1]},
				     -Flags    => DB_CREATE ;

    # create secondary database
    ok 46, my $secondary = new BerkeleyDB::Btree -Filename => $Dfile2, 
				     -Compare  => sub { return $_[0] <=> $_[1]},
				     -Property => DB_DUP,
				     -Flags    => DB_CREATE ;

    # associate primary with secondary
    ok 47, $primary->associate($secondary, \&sec_key2) == 0;

    # add data to the primary
    my %data =  (
		"red"	=> "flag",
		"orange"=> "custard",
		"green"	=> "house",
		"blue"	=> "sea",
		) ;

    my $ret = 0 ;
    while (($k, $v) = each %data) {
        my $r = $primary->db_put($k, $v) ;
	#print "put [$r] $BerkeleyDB::Error\n";
        $ret += $r;
    }
    ok 48, $ret == 0 ;
    #print "ret $ret\n";

    #print "Primary\n" ; dumpdb($primary) ;
    #print "Secondary\n" ; dumpdb($secondary) ;

    # check the records in the secondary
    ok 49, countRecords($secondary) == 4 ;

    my $p_data = joinkeys($primary, " ");
    #print "primary [$p_data]\n" ;
    ok 50, $p_data eq join " ", sort { $a cmp $b } keys %data ;
    my $s_data = joinkeys($secondary, " ");
    #print "secondary [$s_data]\n" ;
    ok 51, $s_data eq join " ", sort { $a <=> $b } map { length } values %data ;

}

{
    # db->associate -- primary recno, secondary hash

    sub sec_key3
    {
        #print "in sec_key\n";
        my $pkey = shift ;
        my $pdata = shift ;

       $_[0] = $pdata ;
        return 0;
    }

    my ($Dfile1, $Dfile2);
    my $lex = new LexFile $Dfile1, $Dfile2 ;
    my %hash ;
    my $status;
    my ($k, $v, $pk) = ('','','');

    # create primary database
    ok 52, my $primary = new BerkeleyDB::Recno -Filename => $Dfile1, 
				     -Flags    => DB_CREATE ;

    # create secondary database
    ok 53, my $secondary = new BerkeleyDB::Hash -Filename => $Dfile2, 
				     -Flags    => DB_CREATE ;

    # associate primary with secondary
    ok 54, $primary->associate($secondary, \&sec_key3) == 0;

    # add data to the primary
    my %data =  (
		0 => "flag",
		1 => "house",
		2 => "sea",
		) ;

    my $ret = 0 ;
    while (($k, $v) = each %data) {
        my $r = $primary->db_put($k, $v) ;
	#print "put $r $BerkeleyDB::Error\n";
        $ret += $r;
    }
    ok 55, $ret == 0 ;

    # check the records in the secondary
    ok 56, countRecords($secondary) == 3 ;

    ok 57, $secondary->db_get("flag", $v) == 0;
    ok 58, $v eq "flag";

    ok 59, $secondary->db_get("house", $v) == 0;
    ok 60, $v eq "house";

    ok 61, $secondary->db_get("sea", $v) == 0;
    ok 62, $v eq "sea" ;

    # pget to primary database is illegal
    ok 63, $primary->db_pget(0, $pk, $v) != 0 ;

    # pget to secondary database is ok
    ok 64, $secondary->db_pget('house', $pk, $v) == 0 ;
    ok 65, $pk == 1 ;
    ok 66, $v  eq 'house';

    ok 67, my $p_cursor = $primary->db_cursor();
    ok 68, my $s_cursor = $secondary->db_cursor();

    # c_get from primary 
    $k = 1;
    ok 69, $p_cursor->c_get($k, $v, DB_SET) == 0;
    ok 70, $k == 1;
    ok 71, $v  eq 'house';

    # c_get from secondary
    $k = 'sea';
    ok 72, $s_cursor->c_get($k, $v, DB_SET) == 0;
    ok 73, $k eq 'sea' 
        or warn "# key [$k]\n";
    ok 74, $v  eq 'sea';

    # c_pget from primary database should fail
    $k = 1;
    ok 75, $p_cursor->c_pget($k, $pk, $v, DB_FIRST) != 0;

    # c_pget from secondary database 
    $k = 'sea';
    ok 76, $s_cursor->c_pget($k, $pk, $v, DB_SET) == 0;
    ok 77, $k eq 'sea' ;
    ok 78, $pk == 2 ;
    ok 79, $v  eq 'sea';

    # check put to secondary is illegal
    ok 80, $secondary->db_put("tom", "dick") != 0;
    ok 81, countRecords($secondary) == 3 ;

    # delete from primary
    ok 82, $primary->db_del(2) == 0 ;
    ok 83, countRecords($primary) == 2 ;

    # check has been deleted in secondary
    ok 84, $secondary->db_get("sea", $v) != 0;
    ok 85, countRecords($secondary) == 2 ;

    # delete from secondary
    ok 86, $secondary->db_del('flag') == 0 ;
    ok 87, countRecords($secondary) == 1 ;


    # check deleted from primary
    ok 88, $primary->db_get(0, $v) != 0;
    ok 89, countRecords($primary) == 1 ;

}

{
    # db->associate -- primary hash, secondary recno

    sub sec_key4
    {
        #print "in sec_key4\n";
        my $pkey = shift ;
        my $pdata = shift ;

       $_[0] = length $pdata ;
        return 0;
    }

    my ($Dfile1, $Dfile2);
    my $lex = new LexFile $Dfile1, $Dfile2 ;
    my %hash ;
    my $status;
    my ($k, $v, $pk) = ('','','');

    # create primary database
    ok 90, my $primary = new BerkeleyDB::Hash -Filename => $Dfile1, 
				     -Flags    => DB_CREATE ;

    # create secondary database
    ok 91, my $secondary = new BerkeleyDB::Recno -Filename => $Dfile2, 
                     #-Property => DB_DUP,
				     -Flags    => DB_CREATE ;

    # associate primary with secondary
    ok 92, $primary->associate($secondary, \&sec_key4) == 0;

    # add data to the primary
    my %data =  (
		"red"	=> "flag",
		"green"	=> "house",
		"blue"	=> "sea",
		) ;

    my $ret = 0 ;
    while (($k, $v) = each %data) {
        my $r = $primary->db_put($k, $v) ;
	#print "put $r $BerkeleyDB::Error\n";
        $ret += $r;
    }
    ok 93, $ret == 0 ;

    # check the records in the secondary
    ok 94, countRecords($secondary) == 3 ;

    ok 95, $secondary->db_get(0, $v) != 0;
    ok 96, $secondary->db_get(1, $v) != 0;
    ok 97, $secondary->db_get(2, $v) != 0;
    ok 98, $secondary->db_get(3, $v) == 0;
    ok 99, $v eq "sea";

    ok 100, $secondary->db_get(4, $v) == 0;
    ok 101, $v eq "flag";

    ok 102, $secondary->db_get(5, $v) == 0;
    ok 103, $v eq "house";

    # pget to primary database is illegal
    ok 104, $primary->db_pget(0, $pk, $v) != 0 ;

    # pget to secondary database is ok
    ok 105, $secondary->db_pget(4, $pk, $v) == 0 ;
    ok 106, $pk eq 'red'
        or warn "# $pk\n";;
    ok 107, $v  eq 'flag';

    ok 108, my $p_cursor = $primary->db_cursor();
    ok 109, my $s_cursor = $secondary->db_cursor();

    # c_get from primary 
    $k = 'green';
    ok 110, $p_cursor->c_get($k, $v, DB_SET) == 0;
    ok 111, $k  eq 'green';
    ok 112, $v  eq 'house';

    # c_get from secondary
    $k = 3;
    ok 113, $s_cursor->c_get($k, $v, DB_SET) == 0;
    ok 114, $k  == 3 ;
    ok 115, $v  eq 'sea';

    # c_pget from primary database should fail
    $k = 1;
    ok 116, $p_cursor->c_pget($k, $pk, $v, DB_SET) != 0;

    # c_pget from secondary database 
    $k = 5;
    ok 117, $s_cursor->c_pget($k, $pk, $v, DB_SET) == 0;
    ok 118, $k  == 5 ;
    ok 119, $pk  eq 'green';
    ok 120, $v  eq 'house';

    # check put to secondary is illegal
    ok 121, $secondary->db_put(77, "dick") != 0;
    ok 122, countRecords($secondary) == 3 ;

    # delete from primary
    ok 123, $primary->db_del("green") == 0 ;
    ok 124, countRecords($primary) == 2 ;

    # check has been deleted in secondary
    ok 125, $secondary->db_get(5, $v) != 0;
    ok 126, countRecords($secondary) == 2 ;

    # delete from secondary
    ok 127, $secondary->db_del(4) == 0 ;
    ok 128, countRecords($secondary) == 1 ;


    # check deleted from primary
    ok 129, $primary->db_get("red", $v) != 0;
    ok 130, countRecords($primary) == 1 ;

}
