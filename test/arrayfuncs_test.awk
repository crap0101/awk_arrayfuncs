
@include "arrlib.awk"
# https://github.com/crap0101/awk_arrlib
@include "testing.awk"
# https://github.com/crap0101/laundry_basket/blob/master/testing.awk

@load "arrayfuncs"

#######################################################
################## N O T E S ##########################
#######################################################
# required: see @include                              #
# required: see @load                                 #
# required (shell): mktemp (for awkpot::get_tempfile) #
#######################################################


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
    if (awk::ARRAYFUNCS_DEBUG) {
	dprint = "awkpot::dprint_real"
	# to set dprint in awkpot functions also (defaults to dprint_fake)
	awkpot::set_dprint(dprint)
    } else {
	dprint = "awkpot::dprint_fake"
    }
    if (awk::GLOBAL_MUST_EXIT != "")
	testing::GLOBAL_MUST_EXIT = awk::GLOBAL_MUST_EXIT

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

    # start report
    testing::start_test_report()

    # TEST do_copy_array
    # test with flat arrays
    a[0]=1; a[1]=2; a[3]=3; a["foo"]="foo"; a["bar"]=2.3
    c[10]=1;c["foobar"]=111

    @dprint("* a:") && arrlib::array_print(a)
    @dprint("* copy_array(a, b)")
    copy_array(a, b)

    testing::assert_true(arrlib::equals(a, b), 1, "> arrlib::equals(a, b)")
    @dprint("* a:") && arrlib::array_print(a)
    @dprint("* b:") && arrlib::array_print(b)

    # add some unassigned values
    @dprint("* set unassigned values to a")
    a["baz"]
    a["foobar"]

    @dprint("* copy_array(a, cc)")
    copy_array(a,cc)
    @dprint("* a:") && arrlib::array_print(a)
    @dprint("* cc:") && arrlib::array_print(cc)
    @dprint(sprintf("* a[\"baz\"] <%s> (type: %s) cc[\"baz\"] <%s> (type: %s) | (eq? %d)\n",
		    a["baz"], typeof(a["baz"]), cc["baz"], typeof(cc["baz"]), a["baz"] == cc["baz"]))
    testing::assert_true(arrlib::equals(a, cc), 1, "> arrlib::equals(a, cc)")

    @dprint("* arrlib::array_copy(a, ccc)")
    arrlib::array_copy(a, ccc)
    testing::assert_true(arrlib::equals(cc, ccc), 1, "> arrlib::equals(cc, ccc)")

    @dprint("* arrlib::remove_unassigned(cc)")
    arrlib::remove_unassigned(cc)
    testing::assert_false(arrlib::equals(cc, ccc), 1, "> ! arrlib::equals(cc, ccc)")
    testing::assert_false(arrlib::equals(a, cc), 1, "> ! arrlib::equals(a, cc)")
    @dprint("* arrlib::remove_unassigned(ccc)")
    arrlib::remove_unassigned(ccc)
    testing::assert_true(arrlib::equals(cc, ccc), 1, "> arrlib::equals(cc, ccc)")

    @dprint("* arrlib::remove_unassigned(a)")
    arrlib::remove_unassigned(a)
    delete cc
    delete ccc

    # add and remove values
    @dprint("* set b[\"foo\"]=\"foobar\"")
    b["foo"]="foobar"
    testing::assert_false(arrlib::equals(a, b), 1, "> ! arrlib::equals(a, b)")
    testing::assert_false(arrlib::exists(a, "foobar"), 1, "> ! arrlib::exists(a, \"foobar\")")
    @dprint("* b:") && arrlib::array_print(b)
    @dprint("* a:") && arrlib::array_print(a)
    @dprint("* set a[\"foo\"]= 33")
    a["foo"]= 33
    testing::assert_false(arrlib::equals(a, b), 1, "> ! arrlib::equals(a, b)")
    testing::assert_false(arrlib::exists(b, 33), 1, "> ! arrlib::exists(b, 33)")
    @dprint("* set b[\"foo\"]= 33")
    b["foo"] = 33
    testing::assert_true(arrlib::equals(a, b), 1, "> arrlib::equals(a, b)")
    @dprint("* set a[5]= 55")
    a[5]= 5
    testing::assert_false(arrlib::equals(a, b), 1, "> ! arrlib::equals(a, b)")
    testing::assert_false(arrlib::exists_index(b, 5), 1, "> ! arrlib::exists_index(b, 5)")
    @dprint("* delete a[5]")
    delete a[5]
    testing::assert_true(arrlib::equals(a, b), 1, "> arrlib::equals(a, b)")
    @dprint("* a:") && arrlib::array_print(a)
    @dprint("* b:") && arrlib::array_print(b)

    @dprint("* copy_array(c, b)")
    copy_array(c, b)
    @dprint("* copy_array(a, c)")
    copy_array(a, c)
    testing::assert_true(arrlib::equals(b, c), 1, "> arrlib::equals(b, c)")
    @dprint("* c:") && arrlib::array_print(c)
    @dprint("* b:") && arrlib::array_print(b)
    _blen = arrlib::array_deep_length(b)
    @dprint(sprintf("* _blen = arrlib::array_deep_length(b) = %d", _blen))
    testing::assert_true(isarray(b), 1, "> isarray(b)")
    testing::assert_equal(_blen, arrlib::array_length(b), 1, "> arrlib::array_length(b) == _blen")
    @dprint("* delete a")
    delete a
    testing::assert_true(isarray(c), 1, "> isarray(c)")
    testing::assert_true(arrlib::equals(b, c), 1, "> arrlib::equals(b, c)")
    delete c
    testing::assert_true(isarray(b), 1, "> isarray(b)")
    testing::assert_equal(_blen, arrlib::array_length(b), 1, "> arrlib::array_length(b) == _blen")

    # build an array from several arrays
    arr3[0]=0;arr3[1]=11;arr3[2]=22;arr3[3]=33;
    arr4["foo"]=1; arr4["bar"]=5
    @dprint("* arr3:") && arrlib::array_print(arr3)
    @dprint("* arr4:") && arrlib::array_print(arr4)

    @dprint("* copy_array(arr3, arr2)")
    copy_array(arr3, arr2)
    @dprint("* copy_array(arr4, arr2)")
    copy_array(arr4, arr2)
    
    @dprint("* arr2:") && arrlib::array_print(arr2)
    testing::assert_true(isarray(arr2), 1, "> isarray(arr2)")
    testing::assert_false(arrlib::equals(arr2, arr3), 1, "> ! arrlib::equals(arr2, arr3)")
    testing::assert_not_equal(arrlib::array_length(arr2), arrlib::array_length(arr3), 1, "> length arr2 != length arr3")

    @dprint("* copy_array(arr4, arr3)")
    copy_array(arr4, arr3)
    testing::assert_true(arrlib::equals(arr2, arr3), 1, "> arrlib::equals(arr2, arr3)")

    @dprint("* delete arr2, arr3, arr4")
    delete arr2; delete arr3; delete arr4
  
    
    # test with subarrays
    # add some elements to arr
    arr["foo"]["bar"] = "bar"
    arr["foo"]["baz"] = 111
    arr[22]["eggs"] = "spam"
    @dprint("* arr:") &&  arrlib::array_print(arr)
    @dprint("* copy_array(arr, arr2)")
    copy_array(arr, arr2)
    testing::assert_true(arrlib::equals(arr, arr2), 1, "> arrlib::equals(arr, arr2)")
    @dprint("* arr2:") && arrlib::array_print(arr2)

    @dprint("* delete arr[22]")
    delete arr[22]
    @dprint("* copy_array(arr, arr3)")
    copy_array(arr, arr3)
    testing::assert_false(arrlib::equals(arr, arr2), 1, "> ! arrlib::equals(arr, arr2)")
    testing::assert_true(arrlib::equals(arr3, arr), 1, "> arrlib::equals(arr3, arr)")

    @dprint("* reset arr[22]")
    arr[22]["eggs"] = "spam"
    @dprint("* arr:") && arrlib::array_print(arr)
    @dprint("* arr2:") && arrlib::array_print(arr2)
    testing::assert_true(arrlib::equals(arr, arr2), 1, "> arrlib::equals(arr, arr2)")
    testing::assert_false(arrlib::equals(arr3, arr2), 1, "> ! arrlib::equals(arr3, arr2)")


    # test wrong inputs
    testing::assert_false(copy_array(arr, 2), 1, "> (must fail) copy_array(arr, 2)")
    testing::assert_true(arrlib::equals(arr, arr2), 1, "> arrlib::equals(arr, arr2)") # checking if not messed up
    testing::assert_false(copy_array(2, arr), 1, "> (fail) copy_array(2, arr)")
    testing::assert_true(arrlib::equals(arr, arr2), 1, "> arrlib::equals(arr, arr2)") # checking if not messed up
    testing::assert_false(copy_array(arr, 2), 1, "> (fail) copy_array(1, 2)")
    #@dprint("> (fail) copy_array(arr) ->", ! copy_array(arr)) # fatal error
    #@dprint("> (fail) copy_array() ->", ! copy_array(arr)) # fatal error

    # test misc
    delete c
    c[0]=0;c[1]=1
    @dprint("* c:") && arrlib::array_print(c)
    @dprint("* copy_array(c, d)")
    copy_array(c,d)
    @dprint("* delete c")
    delete c
    @dprint("* d:") && arrlib::array_print(d)
    testing::assert_true(isarray(d), 1, "> isarray(d)")
    _dlen = arrlib::array_deep_length(d)
    @dprint(sprintf("* _dlen = arrlib::array_deep_length(d) = %d", _dlen))
    @dprint("* copy_array(d, e)")
    copy_array(d, e)
    testing::assert_false(copy_array(c, d), 1, "> (fail) copy_array(c, d)")
    @dprint("* d:") && arrlib::array_print(d)
    testing::assert_true(isarray(d), 1, "> isarray(d)")
    testing::assert_equal(_dlen, arrlib::array_deep_length(d), 1, "> arrlib::array_deep_length(d) == _dlen")
    testing::assert_true(arrlib::equals(d, e), 1, "> arrlib::equals(d, e)")
    @dprint("* e:") && arrlib::array_print(e)
    
    # test big_array with subarrays
    _make_arr_dep_rec(big_array, 15)
    _big_len = arrlib::array_deep_length(big_array)
    @dprint(sprintf("* _big_len = arrlib::array_deep_length(big_array) = %d", _big_len))
    @dprint("* copy_array(big_array, big2)")
    copy_array(big_array, big2)
    testing::assert_equal(_big_len, arrlib::array_deep_length(big2), 1, "> arrlib::array_deep_length(big2) == _big_len")
    testing::assert_true(arrlib::equals(big_array, big2), 1, "> arrlib::equals(big_array, big2)")
    
    # TEST deep_flat_array
    @dprint("* delete c")
    delete c
    # note: deep_flat_array indexes from 0
    c[0]=0;c[1]=1;c[2]=2;c[3]=3;c[4]=4;c[5]=5;c[6]=6;c[7]=7;c[8]=8;c[9]=9;c[10]=10;
    c[11]=11;c[12]=12;c[13]=13;c[14]=14;c[15]=15;c[16]=16;c[17]=17;c[18]=18;c[19]=19

    # test no deep
    @dprint("* c:") && arrlib::array_print(c)
    @dprint("* deep_flat_array(c, already_flat)")
    deep_flat_array(c, already_flat)
    testing::assert_true(arrlib::equals(c, already_flat), 1, "> arrlib::equals(c, already_flat)")
    testing::assert_equal(arrlib::array_deep_length(c), arrlib::array_length(already_flat), 1,
			  "> arrlib::array_deep_length(c) == arrlib::array_deep_length(already_flat)")
    delete d
    d[40]=40;d[41]=41;d[42]=42;d[43]=43;d[44]=44;d[45]=45;d[46]=46;d[47]=47;d[48]=48;d[49]=49
    @dprint("* d:") && arrlib::array_print(d)
    @dprint("* deep_flat_array(d, already_flat)")
    deep_flat_array(d, already_flat)
    #@dprint(sprintf("> arrlib::equals(d, already_flat) = %d", arrlib::equals(d, already_flat)))
    if (1) {
	@dprint("* deep_flat_array(d, already_flat)")
	deep_flat_array(d, c)
    } else {  # works as well
	@dprint("* copy array d in array c changing indexes")
	for (_i in d) c[_i-40] = d[_i]
    }
    testing::assert_true(arrlib::equals(c, already_flat), 1, "> arrlib::equals(c, already_flat)")
    @dprint("* c:") && arrlib::array_print(c)
    @dprint("* already_flat:") && arrlib::array_print(already_flat)
    
    # test really deep
    arr2[2][5] = 2
    @dprint("* arr2:") && arrlib::array_print(arr2)
    @dprint("* deep_flat_array(arr2, flatten2)")
    deep_flat_array(arr2, flatten2)
    @dprint("* flatten2:") && arrlib::array_print(flatten2)
    testing::assert_equal(arrlib::array_deep_length(arr2), arrlib::array_length(flatten2), 1,
			  "> arrlib::array_deep_length(arr2) == arrlib::array_length(flatten2)")
    _t1 = awkpot::get_tempfile()
    @dprint("* _prev_order = set_sort_order(\"@ind_num_desc\")")
    _prev_order = awkpot::set_sort_order("@ind_num_desc")
    @dprint("* print_val(arr2, _t1)")
    arrlib::print_val(arr2, _t1)
 
    awkpot::read_file_arr(_t1, _t1arr)
    awk::asort(_t1arr)
    awk::asort(flatten2)
    @dprint(sprintf("* arr2 (str)     |%s|", arrlib::sprintf_val(arr2,":")))
    @dprint(sprintf("* flatten2 (str) |%s|", arrlib::sprintf_val(flatten2,":")))
    @dprint(sprintf("* (sprintf) _t1arr: <%s> flatten2: <%s>", arrlib::sprintf_val(_t1arr), arrlib::sprintf_val(flatten2)))
    testing::assert_equal(arrlib::sprintf_val(_t1arr), arrlib::sprintf_val(flatten2), 1,
	"> (sprintf_val) _t1arr == flatten2")
    testing::assert_true(arrlib::equals(_t1arr, flatten2, 0, 0), 1, "> arrlib::equals(_t1arr, flatten2)")

    @dprint("* deep_flat_array(big_array, bigflat)")
    deep_flat_array(big_array, bigflat)
    @dprint("* deep_flat_array(big2, bigflat2)")
    deep_flat_array(big2, bigflat2)
    testing::assert_equal(arrlib::array_deep_length(big2), arrlib::array_length(bigflat2), 1,
			  "> arrlib::array_deep_length(big2) == arrlib::array_length(bigflat2)")
    testing::assert_true(arrlib::equals(bigflat, bigflat2), 1, "> arrlib::equals(bigflat, bigflat2)")
    _t1 = awkpot::get_tempfile()
    arrlib::print_val(big_array, _t1)
    delete _t1arr
    awkpot::read_file_arr(_t1, _t1arr)
    awk::asort(_t1arr)
    awk::asort(bigflat)
    testing::assert_equal(arrlib::array_deep_length(_t1arr), arrlib::array_length(bigflat), 1,
			  "> arrlib::array_deep_length(_t1arr) == arrlib::array_length(bigflat)")
    testing::assert_equal(arrlib::sprintf_val(_t1arr), arrlib::sprintf_val(bigflat), 1,
	"> (sprintf_val) _t1arr == bigflat")

    # TEST deep_flat_array_idx
    @dprint("* arr:") && arrlib::array_print(arr)
    @dprint("* deep_flat_array_idx(arr, arri)")
    deep_flat_array_idx(arr, arri)
    testing::assert_equal(_total_idx(arr), arrlib::array_length(arri), 1, "> _total_idx(arr) == arrlib::array_length(arri)")
    _t1 = awkpot::get_tempfile()
    arrlib::print_idx(arr, _t1)
    delete _t1arr
    delete _arri
    awkpot::read_file_arr(_t1, _t1arr)
    # test get_idx also
    arrlib::get_idx(arr, _arri)
    
    # forces values to be type string, cuz read_file_arr (which use getline)
    # may be marks some integers as strnum.
    @dprint("* force_type:");
    for (i in _t1arr) {
	if (awkpot::force_type(_t1arr[i], "string", _t1idx))
	    _t1arr[i] = _t1idx["newval"]
	else
	    @dprint(srpintf("* ERROR (force_type) from <%s> (%s) to string: <%s> (%s)",
			    _t1idx["val"],_t1idx["val_type"], _t1idx["newval"], _t1idx["newval"]))
    }
    delete _t1idx

    # removes original indexes (ones gets from get_idx differs from the ones gets from _read_file_arr)
    awk::asort(_arri)
    awk::asort(_t1arr)
    awk::asort(arri)
    testing::assert_true(arrlib::equals(_arri, arri), 1, "> equals (_arri, arri)")
    testing::assert_true(arrlib::equals(_arri, _t1arr), 1, "> equals (_arri, _t1arr)")

    @dprint("* arri:") && arrlib::array_print(arri)
    @dprint("* _arri:") && arrlib::array_print(_arri)
    @dprint("* _t1arr:") && arrlib::array_print(_t1arr)

    testing::assert_equal(arrlib::sprintf_val(_t1arr), arrlib::sprintf_val(arri),
			  1, "> (sprintf_val) _t1arr == arri")
    testing::assert_equal(arrlib::sprintf_val(_arri), arrlib::sprintf_val(arri),
			  1, "> (sprintf_val) _arri == arri")
    @dprint("* set_sort_order(_prev_order)")
    awkpot::set_sort_order(_prev_order)
    

    testing::end_test_report()
    testing::report()

    # run:
    # ~$ awk -v ARRAYFUNCS_DEBUG=1 -f arrayfuncs_test.awk
    # ~$ awk -v GLOBAL_MUST_EXIT=0 -v ARRAYFUNCS_DEBUG=1 -f arrayfuncs_test.awk
    
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


