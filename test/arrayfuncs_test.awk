
@include "arrlib.awk"
@include "testing.awk"

@load "arrayfuncs"

#################
############# N O T E S ##############
# required: see @include             #
# required: see @load                #
# required (shell): sort mktemp diff #
######################################


##########################
# PRIVATE TEST FUNCTIONS #
##########################

function _make_arr_dep_rec(arr, level,    idx, i) {
    if (level < 1)
	return
    if (arrlib::is_empty(arr))
	for (i=0; i<2; i++)
	    arr[i]=0
    for (idx in arr) {
	delete arr[idx]
	for (i=1; i <= 2; i++)
	    arr[idx][i] = i*level
	_make_arr_dep_rec(arr[idx], level-1)
    }
}

function _total_idx(arr,    i, idx) {
    i = 0
    for (idx in arr)
	if (awk::isarray(arr[idx]))
	    i += 1 + _total_idx(arr[idx])
	else
	    i += 1
    return i
}


BEGIN {
    # arr with subarrays:
    for (i=0;i<5;i++)
	if (i % 2 == 0) {
	    for (j=0;j<5;j++)
		if (j % 2 == 0)
		    arr[i][j] = i","j
		else
		    arr[i][j] = (i+1)*(j+1)
	} else
	    arr[i] = i
    ##################################################
    # a[0]=0;a[1]=11;a[2][0]=20;a[2][1]=21
    # print "idx:";arrlib::print_idx(a)
    # print "val:";arrlib::print_val(a)
    # exit(111)
    testing::start_test_report()

    # TEST do_copy_array
    a[0]=1; a[1]=2; a[3]=3; a["foo"]="foo"; a["bar"]=2.3
    c[10]=1;c["foobar"]=111
    print "* a:"; arrlib::array_print(a)
    print "* copy_array(a,b) ..."
    copy_array(a,b)
    testing::assert_true(arrlib::equals(a, b), 1, "> arrlib::equals(a, b)")
    print "* a:"; arrlib::array_print(a)
    printf "-----\n* b:\n"; arrlib::array_print(b)
    print "* set b[\"foo\"]=\"foobar\" ..."
    b["foo"]="foobar"
    testing::assert_false(arrlib::equals(a, b), 1, "> ! arrlib::equals(a, b)")
    testing::assert_false(arrlib::exists(a, "foobar"), 1, "> ! arrlib::exists(a, \"foobar\")")
    print "* b:"; arrlib::array_print(b)
    print "* a:"; arrlib::array_print(a)
    print "* set a[\"foo\"]= 33 ..."
    a["foo"]= 33
    testing::assert_false(arrlib::equals(a, b), 1, "> ! arrlib::equals(a, b)")
    testing::assert_false(arrlib::exists(b, 33), 1, "> ! arrlib::exists(b, 33)")
    print "* set b[\"foo\"]= 33 ..."
    b["foo"] = 33
    testing::assert_true(arrlib::equals(a, b), 1, "> arrlib::equals(a, b)")
    print "* set a[5]= 55 ..."
    a[5]= 5
    testing::assert_false(arrlib::equals(a, b), 1, "> ! arrlib::equals(a, b)")
    testing::assert_false(arrlib::exists_index(b, 5), 1, "> ! arrlib::exists_index(b, 5)")
    print "* delete a[5]"
    delete a[5]
    testing::assert_true(arrlib::equals(a, b), 1, "> arrlib::equals(a, b)")
    print "* a:"; arrlib::array_print(a)
    print "* b:"; arrlib::array_print(b)
    print "----------"
    print "* copy_array(c,b) ..."; copy_array(c,b)
    print "* copy_array(a,c) ..."; copy_array(a,c)
    testing::assert_true(arrlib::equals(b, c), 1, "> arrlib::equals(b, c)")
    print "* c:"; arrlib::array_print(c)
    printf "----\nb:\n"; arrlib::array_print(b)
    _blen = arrlib::array_deep_length(b)
    print "* _blen = arrlib::array_deep_length(b) =", _blen
    print "* delete a,c ..."
    testing::assert_true(isarray(b), 1, "> isarray(b)")
    testing::assert_equal(_blen, arrlib::array_deep_length(b), 1, "> arrlib::array_deep_length(b) == _blen")
    print "* delete a, c ..."
    delete a
    delete c
    testing::assert_true(isarray(c), 1, "> isarray(c)")
    testing::assert_false(arrlib::equals(b, c), 1, "> ! arrlib::equals(b, c)")
    print "----------"
    arr3[0]=0;arr3[1]=11;arr3[2]=22;arr3[3]=33;
    print "* arr3:"; arrlib::array_print(arr3)
    print "* copy_array(arr3, arr2) ..."
    copy_array(arr3, arr2)
    print "* arr2:"; arrlib::array_print(arr2)
    testing::assert_true(isarray(arr2), 1, "isarray(arr2)")
    testing::assert_equal(arrlib::array_length(arr2), arrlib::array_length(arr3), 1, "length arr2 == length arr3")

    # test with subarrays
    print "* delete arr2 ..."
    delete arr2
    # add some elements to arr
    arr["foo"]["bar"] = "bar"
    arr["foo"]["baz"] = 111
    arr[2]["eggs"] = "spam"
    print "* arr:"; arrlib::array_print(arr)
    print "* copy_array(arr, arr2) ..."
    copy_array(arr, arr2)
    testing::assert_true(arrlib::equals(arr, arr2), 1, "> arrlib::equals(arr, arr2)")
    print "* arr2:"; arrlib::array_print(arr2)

    #XXX+TODO: unassigned values (must be managed in the .c file)
    #print "* c_a:", copy_array(arr2, arr3) #XXX+todo for arrfuncs
    #arr3["FAKE_IDX"][1]
    #arr3["FAKE_IDX"][2]
    #print "* bc:";arrlib::array_print(arr3)

    # test wrong inputs
    testing::assert_false(copy_array(arr, 2), 1, "> (must fail) copy_array(arr, 2)")
    testing::assert_true(arrlib::equals(arr, arr2), 1, "> arrlib::equals(arr, arr2)") # checking if not messed up
    testing::assert_false(copy_array(2, arr), 1, "> (fail) copy_array(2, arr)")
    testing::assert_true(arrlib::equals(arr, arr2), 1, "> arrlib::equals(arr, arr2)") # checking if not messed up
    testing::assert_false(copy_array(arr, 2), 1, "> (fail) copy_array(1, 2)")
    #print "> (fail) copy_array(arr) ->", ! copy_array(arr) # fatal error
    #print "> (fail) copy_array() ->", ! copy_array(arr) # fatal error

    # test misc
    delete c
    c[0]=0;c[1]=1
    print "* c:"; arrlib::array_print(c)
    print "* copy_array(c, d) ..."; copy_array(c,d)
    print "* delete c ..."
    delete c
    print "* d:"; arrlib::array_print(d)
    testing::assert_true(isarray(d), 1, "> isarray(d)")
    _dlen = arrlib::array_deep_length(d)
    print "* _dlen = arrlib::array_deep_length(d):", _dlen
    print "* copy_array(d, e) ..."; copy_array(d, e)
    testing::assert_false(copy_array(c, d), 1, "> (fail) copy_array(c, d)")
    print "* d:"; arrlib::array_print(d)
    testing::assert_true(isarray(d), 1, "> isarray(d)")
    testing::assert_equal(_dlen, arrlib::array_deep_length(d), 1, "> arrlib::array_deep_length(d) == _dlen")
    testing::assert_true(arrlib::equals(d, e), 1, "> arrlib::equals(d, e)")
    print "* e:"; arrlib::array_print(e)
    
    # test big_array with subarrays
    _make_arr_dep_rec(big_array, 15)
    _big_len = arrlib::array_deep_length(big_array)
    print "* _big_len = arrlib::array_deep_length(big_array):", _big_len
    print "* copy_array(big_array, big2) ..."; copy_array(big_array, big2)
    testing::assert_equal(_big_len, arrlib::array_deep_length(big2), 1, "> arrlib::array_deep_length(big2) == _big_len")
    testing::assert_true(arrlib::equals(big_array, big2), 1, "> arrlib::equals(big_array, big2)")
    
    # TEST deep_flat_array
    print "* delete c"
    delete c
    # note: deep_flat_array indexes from 0
    c[0]=0;c[1]=1;c[2]=2;c[3]=3;c[4]=4;c[5]=5;c[6]=6;c[7]=7;c[8]=8;c[9]=9;c[10]=10;
    c[11]=11;c[12]=12;c[13]=13;c[14]=14;c[15]=15;c[16]=16;c[17]=17;c[18]=18;c[19]=19

    # test no deep
    print "* c:"; arrlib::array_print(c)
    print "deep_flat_array(c, already_flat)"
    deep_flat_array(c, already_flat)
    testing::assert_true(arrlib::equals(c, already_flat), 1, "> arrlib::equals(c, already_flat)")
    testing::assert_equal(arrlib::array_deep_length(c), arrlib::array_length(already_flat), 1,
			  "> arrlib::array_deep_length(c) == arrlib::array_deep_length(already_flat)")
    delete d
    d[40]=40;d[41]=41;d[42]=42;d[43]=43;d[44]=44;d[45]=45;d[46]=46;d[47]=47;d[48]=48;d[49]=49
    print "* d:"; arrlib::array_print(d)
    print "deep_flat_array(d, already_flat)"
    deep_flat_array(d, already_flat)
    #print "> arrlib::equals(d, already_flat) =", arrlib::equals(d, already_flat)
    if (1) {
	print "deep_flat_array(d, already_flat)"; deep_flat_array(d, c)
    } else {  # works as well
	print "* copy array d in array c changing indexes..."
	for (_i in d) c[_i-40] = d[_i]
    }
    testing::assert_true(arrlib::equals(c, already_flat), 1, "> arrlib::equals(c, already_flat)")
    print "* c:"; arrlib::array_print(c)
    print "* already_flat:"; arrlib::array_print(already_flat)
    print "------------"
    
    # test really deep
    arr2[2][5] = 2
    print "* arr2:"; arrlib::array_print(arr2)
    print "deep_flat_array(arr2, flatten2)"
    deep_flat_array(arr2, flatten2)
    print "* flatten2:"; arrlib::array_print(flatten2)
    testing::assert_equal(arrlib::array_deep_length(arr2), arrlib::array_length(flatten2), 1,
			  "> arrlib::array_deep_length(arr2) == arrlib::array_length(flatten2)")
    _t1 = awkpot::get_tempfile()
    print "* _prev_order = set_sort_order(\"@ind_num_desc\")"
    _prev_order = awkpot::set_sort_order("@ind_num_desc")
    arrlib::print_val(arr2, _t1)
    # old tests with external commands:
    # arrlib::print_val(flatten2, _t2)
    # #_args[0]="-o";_args[1]=_t1s;_args[2]=_t1
    # #awkpot::run_command("sort", 3, _args, 1, _run_values1)
    # #_args[1]=_t2s;_args[2]=_t2
    # #awkpot::run_command("sort", 3, _args, 1, _run_values2)
    #testing::assert_equal(system(sprintf("diff %s %s > /dev/null", _t1s, _t2s)), 0, 1,
    #	                   "> same values in arr2 and flatten2")    
    awkpot::read_file(_t1, _t1arr)
    awk::asort(_t1arr)
    awk::asort(flatten2)
    printf "* arr2 (str)     |%s|\n", arrlib::sprintf_val(arr2,":")
    printf "* flatten2 (str) |%s|\n", arrlib::sprintf_val(flatten2,":")
    testing::assert_equal(arrlib::sprintf_val(_t1arr), arrlib::sprintf_val(flatten2), 1,
	"> (sprintf_val) _t1arr == flatten2")
    testing::assert_true(arrlib::equals(_t1arr, flatten2, 0, 0), 1, "> arrlib::equals(_t1arr, flatten2)")

    print "* deep_flat_array(big_array, bigflat)"
    deep_flat_array(big_array, bigflat)
    print "* deep_flat_array(big2, bigflat2)"
    deep_flat_array(big2, bigflat2)
    testing::assert_equal(arrlib::array_deep_length(big2), arrlib::array_length(bigflat2), 1,
			  "> arrlib::array_deep_length(big2) == arrlib::array_length(bigflat2)")
    testing::assert_true(arrlib::equals(bigflat, bigflat2), 1, "> arrlib::equals(bigflat, bigflat2)")
    _t1 = awkpot::get_tempfile()
    arrlib::print_val(big_array, _t1)
    delete _t1arr
    awkpot::read_file(_t1, _t1arr)
    awk::asort(_t1arr)
    awk::asort(bigflat)
    testing::assert_equal(arrlib::array_deep_length(_t1arr), arrlib::array_length(bigflat), 1,
			  "> arrlib::array_deep_length(_t1arr) == arrlib::array_length(bigflat)")
    testing::assert_equal(arrlib::sprintf_val(_t1arr), arrlib::sprintf_val(bigflat), 1,
	"> (sprintf_val) _t1arr == bigflat")

    # TEST deep_flat_array_idx
    print "* arr:"; arrlib::array_print(arr)
    print "* deep_flat_array_idx(arr, arri)"
    deep_flat_array_idx(arr, arri)
    print "* arri:"; arrlib::array_print(arri)
    testing::assert_equal(_total_idx(arr), arrlib::array_length(arri), 1, "> _total_idx(arr) == arrlib::array_length(arri)")
    _t1 = awkpot::get_tempfile()
    arrlib::print_idx(arr, _t1)
    delete _t1arr
    awkpot::read_file(_t1, _t1arr)
    awk::asort(_t1arr)
    awk::asort(arri)
    testing::assert_equal(arrlib::array_deep_length(_t1arr), arrlib::array_length(arri), 1,
			  "> arrlib::array_deep_length(_t1arr) == arrlib::array_length(arri)")
    testing::assert_equal(arrlib::sprintf_val(_t1arr), arrlib::sprintf_val(arri), 1,
	"> (sprintf_val) _t1arr == arri")

    print "* set_sort_order(_prev_order)"; awkpot::set_sort_order(_prev_order)
    

    testing::end_test_report()
    testing::report()
    
    exit(0)
    # exit ####
    # print ("foo_array:")
    # foo_array(x, 3)
    # #print "arr[foo]=", SYMTAB["x"]["foo"] # provando direttamente x["foo"] fallisce sym_update() in arrcopy.c (!!!)
    # # probabilmente perchè parsando questo file .awk inserisce già nella symtab il simbolo...
    # arrlib::array_print(x);
    # #for (i in new_array) printf("new_array[%s] = %s\n", i, new_array[i])
    # print "--------"
    # exit #######
    #
    # printf "dest_arr= %s\n", isarray(SYMTAB["dest_arr"])
    # for (i in SYMTAB["dest_arr"]) printf "* dest_arr[%d] = \n", i, SYMTAB["dest_arr"][i]
}


