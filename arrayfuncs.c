/*
Copyright (C) 2023,  Marco Chieppa | crap0101

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3 of the License,
or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <https://www.gnu.org/licenses>.
*/

/*
 * Description: utilities to be used with gawk's arrays.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "gawkapi.h"

// define these before include awk_extensions.h
#define _DEBUGLEVEL 0
#define __module__ "arrayfuncs"
#define __namespace__ "array"
#define _array_prefix "arrayfuncs_array__"
static const gawk_api_t *api;
static awk_ext_id_t ext_id;
static const char *ext_version = "0.2";

// ... and now include other own utilities
#include "awk_extensions.h"
// https://github.com/crap0101/laundry_basket/blob/master/awk_extensions.h

struct subarrays {
  awk_value_t source_index_val;
  awk_value_t source_value_val;
  awk_value_t dest_index_val;
  awk_value_t dest_value_val;
  
  awk_value_t source_arr_value;
  awk_value_t dest_arr_value;
  
  awk_array_t source_array;
  awk_array_t dest_array;

  awk_flat_array_t *source_flat_array;
  awk_flat_array_t *dest_flat_array;
};

static awk_value_t * do_equals(int nargs, awk_value_t *result, struct awk_ext_func *finfo);
static awk_value_t * do_copy(int nargs, awk_value_t *result, struct awk_ext_func *finfo);
static awk_value_t * do_deep_flat(int nargs, awk_value_t *result, struct awk_ext_func *finfo);
static awk_value_t * do_deep_flat_idx(int nargs, awk_value_t *result, struct awk_ext_func *finfo);
static awk_value_t * do_uniq(int nargs, awk_value_t *result, struct awk_ext_func *finfo);
//XXX+TODO: add depth parameter to flat to the given depth only


/* ----- boilerplate code ----- */
int plugin_is_GPL_compatible;

static awk_ext_func_t func_table[] = {
  { "equals", do_equals, 2, 2, awk_false, NULL },
  { "copy", do_copy, 2, 2, awk_false, NULL },
  { "deep_flat", do_deep_flat, 2, 2, awk_false, NULL },
  { "deep_flat_idx", do_deep_flat_idx, 2, 2, awk_false, NULL },
  { "uniq", do_uniq, 2, 2, awk_false, NULL },
};

__attribute__((unused)) static awk_bool_t (*init_func)(void) = NULL;


int
dl_load(const gawk_api_t *api_p, void *id)
{
  api = api_p;
  ext_id = (awk_ext_id_t) &id;
  int errors = 0;
  long unsigned int i;
  
  if (api->major_version < 3) { // != GAWK_API_MAJOR_VERSION
      //  || api->minor_version < GAWK_API_MINOR_VERSION) {
    eprint("incompatible api version:  %d.%d != %d.%d (extension/gawk version)\n",
	   GAWK_API_MAJOR_VERSION, GAWK_API_MINOR_VERSION,
	   api->major_version, api->minor_version);
    exit(1);
  }
  
  for (i=0; i < sizeof(func_table) / sizeof(awk_ext_func_t); i++) {
    if (! add_ext_func(__namespace__, & func_table[i])) {
      eprint("can't add extension function <%s>\n", func_table[i].name);
      errors++;
    }
  }
  if (ext_version != NULL) {
    register_ext_version(ext_version);
  }
  return (errors == 0);
}

/* ----- end of boilerplate code ----------------------- */


/*********************/
/* UTILITY FUNCTIONS */
/*********************/


int
compare_element(awk_value_t item1, awk_value_t item2)
{
  /*
   * Compares if $item1 equals $item2. 
   * Returns 1 if succedes, 0 otherwise.
   * Works with AWK_(STRING|REGEX|STRNUM|NUMBER|UNDEFINED) and,
   * if available, AWK_BOOL.
   * For others val_type (such AWK_ARRAY, which are much more complex
   * to handle, always returns 0. Such cases must be checked before
   * or after the calling of this function.
   */
  if (item1.val_type != item2.val_type)
    return 0;
  switch (item1.val_type) {
  case AWK_STRING: case AWK_REGEX: case AWK_STRNUM:
    return (item1.str_value.len == item2.str_value.len
	    && (! strcmp(item1.str_value.str, item2.str_value.str)));
  case AWK_NUMBER:
    return (item1.num_type == item2.num_type
	    && item1.num_value == item2.num_value);
/* not in gawkapi.h (3.0) */
#ifdef AWK_BOOL
  case AWK_BOOL:
    return item1.bool_value == item2.bool_value;
#endif
  case AWK_UNDEFINED:
    // pretty simple
    return 1;
  case AWK_ARRAY:
    eprint("Unsupported type: <%s>\n", _val_types[item1.val_type]);
    return 0;
  case AWK_SCALAR:       // should not happen
    eprint("Unsupported type: <%s<\n", _val_types[item1.val_type]);
    return 0;
  case AWK_VALUE_COOKIE: // should not happen
    eprint("Unsupported type: <%s>\n", _val_types[item1.val_type]);
    return 0;
  default:               // could happen
    eprint("Unknown val_type: <%d>\n", item1.val_type);
    return 0;
  }
  return 0; // just for silencing warning
}


struct subarrays*
alloc_subarray_list(struct subarrays *list, size_t new_size)
{
  /*
   * Allocates (or reallocates) a $list of <struct subarrays> of size $new_size.
   * Returns the pointer to the allocated memory, or NULL if fails.
   */
    dprint("(re)alloc new_size: <%zu>\n", new_size);
    return realloc(list, sizeof(struct subarrays) * new_size);
}


int
release_subarrays(struct subarrays *list,
			  size_t idx,
			  int rs, int rd) {
  /*
   * Releases the struct subarrays $list up to $idx index,
   * calling release_flattened_array() on the source_* arrays member
   * if $rs is true and on the dest_* arrays member ig $rd is true.
   * Returns true if success, false otherwise.
   */
  size_t i;
  int result = 1;
  dprint("Release flattened array...\n");
  for (i=0; i < idx; i++) {
    if (rs) { // release source arrays
      if (! release_flattened_array(list[i].source_array,
				    list[i].source_flat_array)) {
	result = 0;
	dprint("in release_flattened_array() at index %ld [source array]\n", i);
      }
    }
    if (rd) { // release dest arrays
      if (! release_flattened_array(list[i].dest_array,
				    list[i].dest_flat_array)) {
	result = 0;
	dprint("in release_flattened_array() at index %ld [dest array]\n", i);
      }
    }
  }
  return result;
}


struct subarrays*
_deep_flat(struct subarrays *list,
	   awk_array_t *dest_array,
	   size_t *idx,
	   size_t *size,
	   size_t *maxsize)
{
  /*
   * Private function to flat arrays.
   * Given a $list of <struct subarrays *>, fills $dest_array with the array's
   * values of the structs in $list, indexed with numbers starting from 0.
   * Returns $list, or NULL if fails.
   */
  size_t i, dest_idx = 0;
  awk_valtype_t ret;

  do {
    dprint("idx, size, maxsize = <%zu> <%zu> <%zu>\n", *idx, *size, *maxsize);
    if (*size >= *maxsize-1) {
      *maxsize *= 10;
      if (NULL == (list = alloc_subarray_list(list, *maxsize)))
	fatal(ext_id, "Can't reallocate array lists: %s", strerror(errno));
    }

    /* flat the array
     * NOTE_A: flatten_array_typed return false if the array is empty,
     * contains no data etc... so seem safe to don't bother too much
     * about that (big problems are fatal!).
     * So, we can ignore this and proceed with the other elements.
     */
    if (! flatten_array_typed(list[*idx].source_array,
			      & list[*idx].source_flat_array,
			      AWK_STRING, AWK_UNDEFINED)) {
      dprint("could not flatten source array (idx = %zu)\n", *idx);
      *idx += 1;
      continue;
    }
   
    dprint("list[%zu].flat_array->count = <%zu> items\n",
	   *idx, list[*idx].source_flat_array->count);

    for (i = 0; i < list[*idx].source_flat_array->count; i++)  {
      if (! (ret = copy_element(list[*idx].source_flat_array->elements[i].value,
				& list[*idx].dest_value_val))) {
	
	if (list[*idx].source_flat_array->elements[i].value.val_type == AWK_ARRAY) {
	  // is a subarray, save it and procede
	  dprint("subarray at index <%zu> (size: %zu)\n", i, *size);
	  list[*size].source_array = list[*idx].source_flat_array->elements[i].value.array_cookie;
	  *size += 1;
	} else {
	  fatal(ext_id,
		"Unknown element at index <%zu> (val_type=%d)",
		i, list[*idx].dest_value_val.val_type);
	}
      } else {
	
	make_number(dest_idx, & list[*idx].dest_index_val);
	if (! set_array_element(*dest_array,
				& list[*idx].dest_index_val,
				& list[*idx].dest_value_val)) {
	  fatal(ext_id,
		"set_array_element() failed on scalar value"
		"at list[%zu] (dest_idx = <%zu>)",
		*idx, dest_idx);
	}
	dest_idx += 1;
      }
    }
    *idx += 1;
  } while (*idx < *size);
  return list;
}


struct subarrays*
_deep_flat_idx(struct subarrays *list,
	       awk_array_t *dest_array,
	       size_t *idx,
	       size_t *size,
	       size_t *maxsize)
{
  /*
   * Private function to flat arrays.
   * Given a $list of <struct subarrays *>, fills $dest_array
   * with the array's indexes of the structs in $list as values,
   * indexed with numbers starting from 0.
   * Returns $list, or NULL if fails.
   */
  size_t i, dest_idx = 0;
  awk_value_t dest_idx_val;
  awk_valtype_t ret;
  
  do {
    dprint("idx, size, maxsize = <%zu> <%zu> <%zu>\n", *idx, *size, *maxsize);
    if (*size >= *maxsize-1) {
      *maxsize *= 10;
      if (NULL == (list = alloc_subarray_list(list, *maxsize)))
	fatal(ext_id, "Can't reallocate array lists: %s", strerror(errno));
    }
    /* flat the array */
    if (! flatten_array_typed(list[*idx].source_array,
			      & list[*idx].source_flat_array,
			      AWK_STRING, AWK_UNDEFINED)) {
      // skip, see NOTE_A
      dprint("could not flatten source array (idx = <%zu>)\n", *idx);
      *idx += 1;
      continue;
    }

    dprint("list[%zu].flat_array->count = %zu items\n",
	   *idx, list[*idx].source_flat_array->count);

    for (i = 0; i < list[*idx].source_flat_array->count; i++)  {
      if (! (ret = copy_element(list[*idx].source_flat_array->elements[i].value,
				& list[*idx].dest_value_val))) {
	if (list[*idx].source_flat_array->elements[i].value.val_type == AWK_ARRAY) {
	  // is a subarray, save it for later but *also* set the index val
	  dprint("subarray at index <%zu> (size: %zu)\n", i, *size);
	  list[*size].source_array = list[*idx].source_flat_array->elements[i].value.array_cookie;
	  *size += 1;
	} else {
	  fatal(ext_id, "Unknown element at index <%zu> (val_type=%d)",
		i, list[*idx].dest_value_val.val_type);
	}
      }
      make_number(dest_idx, & dest_idx_val);
      if (! copy_element(list[*idx].source_flat_array->elements[i].index,
			 & list[*idx].dest_index_val)) {
	fatal(ext_id,
	      "copy_element() at array index %zu (arraylist index %zu)",
	      i, *idx);
      }
      if (! set_array_element(*dest_array,
			      & dest_idx_val,
			      & list[*idx].dest_index_val)) {
	fatal(ext_id,
	      "set_array_element() failed at list[%zu] (dest_idx = %zu)",
	      *idx, dest_idx);
      }
      dest_idx += 1;
    }
    *idx += 1;
  } while (*idx < *size);
  return list;
}


/***********************/
/* EXTENSION FUNCTIONS */
/***********************/

static awk_value_t*
do_equals(int nargs,
	  awk_value_t *result,
	  __attribute__((unused)) struct awk_ext_func *finfo)
{
  /* 
   * Returns true if the array at $nargs[0]
   * equals the array at $nargs[1], else false.
   * Exits with a fatal error if there are big issues.
   * NOTE: comparing deleted arrays always evaluate to false.
   */
  assert(result != NULL);
  make_number(0.0, result);
  if (nargs != 2)
    fatal(ext_id, "two args expected: array_1, array_2");

  struct subarrays *list = NULL;
  size_t i, idx = 0;
  size_t size = 0;
  size_t maxsize = 10;
  
  if (NULL == (list = alloc_subarray_list(list, maxsize)))
    fatal(ext_id, "Can't allocate array lists!");

  /* SOURCE ARRAY */
  if (! get_argument(0, AWK_ARRAY, & list[size].source_arr_value))
    fatal(ext_id, "can't retrieve array (1st arg)");

  /* DEST ARRAY */
  if (! get_argument(1, AWK_ARRAY, & list[size].dest_arr_value))
    fatal(ext_id, "can't retrieve array (2nd arg)");

  list[size].dest_array = list[size].dest_arr_value.array_cookie;     /*** MANDATORY ***/
  list[size].source_array = list[size].source_arr_value.array_cookie; /*** MANDATORY ***/
  size += 1;
  
  do {
    dprint("idx, size, maxsize = %zu %zu %zu\n", idx, size, maxsize);
    if (size >= maxsize-1) {
      maxsize *= 10;
      if (NULL == (list = alloc_subarray_list(list, maxsize)))
	fatal(ext_id, "Can't reallocate arrays list: %s", strerror(errno));
    }
    /* flat the arrays */
    if (! flatten_array_typed(list[idx].source_array,
			      & list[idx].source_flat_array,
			      AWK_STRING, AWK_UNDEFINED)) {
      dprint("could not flatten (1st) array\n");
      goto out;
    }
    if (! flatten_array_typed(list[idx].dest_array,
			      & list[idx].dest_flat_array,
			      AWK_STRING, AWK_UNDEFINED)) {
      dprint("could not flatten (2nd) array\n");
      goto out;
    }

    if (list[idx].source_flat_array->count != list[idx].dest_flat_array->count) {
      dprint("mismatch count at index <%zu>", idx);
      goto out;
    }
    
    dprint("list[%zu].source_flat_array->count = <%zu> items\n",
	   idx, list[idx].source_flat_array->count);
    for (i = 0; i < list[idx].source_flat_array->count; i++)  {
      if (list[idx].source_flat_array->elements[i].index.val_type
	  != list[idx].dest_flat_array->elements[i].index.val_type) {
	dprint("mismatch val_type (index) at index <%zu>", idx);
	goto out;
      }
      if (list[idx].source_flat_array->elements[i].value.val_type
	  != list[idx].dest_flat_array->elements[i].value.val_type) {
	dprint("mismatch val_type (value) at index <%zu>", idx);
	goto out;
      }
      if (list[idx].source_flat_array->elements[i].value.val_type == AWK_ARRAY) {
	list[size].source_array = list[idx].source_flat_array->elements[i].value.array_cookie;
	list[size].dest_array = list[idx].dest_flat_array->elements[i].value.array_cookie;
	size += 1;
      } else {
	if (! (compare_element(list[idx].source_flat_array->elements[i].index,
			       list[idx].dest_flat_array->elements[i].index)
	       && compare_element(list[idx].source_flat_array->elements[i].value,
				  list[idx].dest_flat_array->elements[i].value))) {
	  dprint("compare_element returns false at index <%zu>", idx);
	  goto out;
	}
      }
    }
    idx += 1;
  } while (idx < size);
  
  make_number(1.0, result);
 out:
  /* MANDATORY -- must be called before exit */
  release_subarrays(list, idx, 1, 1);
  free(list);
  return result;
}


static awk_value_t*
do_copy(int nargs,
	awk_value_t *result,
	__attribute__((unused)) struct awk_ext_func *finfo)
{
  /* 
   * Copies the $nargs[0] array into the $nargs[1] array, *without* deleting
   * elements already present in the latter (sure they can be overwted).
   * Exits with a fatal error if there are big issues, returns false if
   * everything is not exactly ok but overall there are no errors respecting
   * the requested operations, true if everything is fine.
   */
  assert(result != NULL);
  make_number(0.0, result);
  
  struct subarrays *list = NULL;
  size_t i, idx = 0;
  size_t size = 0;
  size_t maxsize = 10;
  awk_valtype_t ret;
  
  if (nargs != 2)
    fatal(ext_id, "two args expected: source, dest");
  if (NULL == (list = alloc_subarray_list(list, maxsize)))
    fatal(ext_id, "Can't allocate arrays list!");

  /* SOURCE ARRAY */
  if (! get_argument(0, AWK_ARRAY, & list[size].source_arr_value))
    fatal(ext_id, "can't retrieve source array");

  /* DEST ARRAY */
  if (! get_argument(1, AWK_ARRAY, & list[size].dest_arr_value))
    fatal(ext_id, "can't retrieve dest array");
    
  //XXX+TODO: test if works... not found a better way... seems no api facilities...
  if (list[size].source_arr_value.array_cookie == list[size].dest_arr_value.array_cookie)
    fatal(ext_id, "trying to copy an array on itself!");

  list[size].dest_array = list[size].dest_arr_value.array_cookie;     /*** MANDATORY ***/
  list[size].source_array = list[size].source_arr_value.array_cookie; /*** MANDATORY ***/

  size += 1;
  
  do {
    dprint("idx, size, maxsize = <%zu> <%zu> <%zu>\n", idx, size, maxsize);
    if (size >= maxsize-1) {
      maxsize *= 10;
      if (NULL == (list = alloc_subarray_list(list, maxsize)))
	fatal(ext_id, "Can't reallocate array lists: %s", strerror(errno));
    }
    
    /* flat the array */
    if (! flatten_array_typed(list[idx].source_array,
			      & list[idx].source_flat_array,
			      AWK_STRING, AWK_UNDEFINED)) {
      // skip possibly empty subarrays et similia, see NOTE_A
      idx += 1;
      continue;
    }

    dprint("list[%zu].flat_array->count = <%zu> items\n",
	   idx, list[idx].source_flat_array->count);
    for (i = 0; i < list[idx].source_flat_array->count; i++)  {
      if (! copy_element(list[idx].source_flat_array->elements[i].index,
			 & list[idx].dest_index_val)) {
	fatal(ext_id,
	      "copy_element() failed at array index <%zu> "
	      "(arraylist index: %zu)",
	      i, idx);
      }
      if (! (ret = copy_element(list[idx].source_flat_array->elements[i].value,
				& list[idx].dest_value_val))) {
	if (list[idx].source_flat_array->elements[i].value.val_type == AWK_ARRAY) {
	  /* is a subarray, save it and procede */
	  dprint("subarray at index <%zu>\n", i);
	  list[size].dest_array = create_array();
	  list[size].dest_arr_value.val_type = AWK_ARRAY;                   // *** MANDATORY ***
	  list[size].dest_arr_value.array_cookie = list[size].dest_array;   // *** MANDATORY ***
	
	  if (! set_array_element(list[idx].dest_array,
				  & list[idx].dest_index_val,
				  & list[size].dest_arr_value)) {
	    fatal(ext_id,
		  "set_array_element() failed on subarray at index <%zu>",
		  idx);
	    //size -=1;
	    //goto out;
	  }
	  list[size].source_array = list[idx].source_flat_array->elements[i].value.array_cookie;
	  list[size].dest_array = list[size].dest_arr_value.array_cookie; // *** MANDATORY -- after set_array_element() ***
	  size += 1;
	} else {
	  fatal(ext_id,
		"Unknown element at index <%zu> (val_type=%d)",
		i, list[idx].dest_value_val.val_type);
	}
      } else {
	if (! set_array_element(list[idx].dest_array,
				& list[idx].dest_index_val,
				& list[idx].dest_value_val)) {
	  fatal(ext_id,
		"set_array_element() failed on value at index <%zu>", idx);
	}
      }
    }
    idx += 1;
  } while (idx < size);

  make_number(1.0, result);

  /* MANDATORY -- must be called before exit */
  if (! release_subarrays(list, idx, 1, 0))
    make_number(0.0, result);

  free(list);
  return result;
}


static awk_value_t*
do_deep_flat(int nargs,
	     awk_value_t *result,
	     __attribute__((unused)) struct awk_ext_func *finfo)
{
  /*
   * Flattens the $nargs[0] array into the $nargs[1] array *without* deleting
   * elements already present in the latter.
   * Flattens possibly present subarrays.
   * The flattened array will be indexed with integer values starting from 0.
   * Exits with a fatal error if there are big issues, returns false if
   * everything is not exactly ok but overall there are no errors respecting
   * the requested operations, true if everything is fine.
   */
  assert(result != NULL);
  make_number(0, result);

  struct subarrays *list = NULL;
  awk_value_t dest_arr_value;
  awk_array_t dest_array;
  
  size_t idx = 0;
  size_t size = 0;
  size_t maxsize = 10;

  if (nargs < 2)
    fatal(ext_id, "two args expected: source_array, dest_array");
  if (NULL == (list = alloc_subarray_list(list, maxsize)))
    fatal(ext_id, "Can't allocate array lists: %s", strerror(errno));
  if (! get_argument(0, AWK_ARRAY, & list[size].source_arr_value))
    fatal(ext_id, "can't retrieve source array");
  if (! get_argument(1, AWK_ARRAY, & dest_arr_value))
    fatal(ext_id, "can't retrieve dest array");

  //XXX+TODO: test if works... not found a better way... seems no api facilities...
  if (list[size].source_arr_value.array_cookie == dest_arr_value.array_cookie)
    fatal(ext_id, "trying to flat an array on itself!");


  dest_array = dest_arr_value.array_cookie;                            // *** MANDATORY ***
  list[size].source_array = list[size].source_arr_value.array_cookie;  // *** MANDATORY ***
  size += 1;

  list = _deep_flat(list, & dest_array, & idx, & size, & maxsize);

  make_number(1, result);

  // must be called before exit
  if (! release_subarrays(list, idx, 1, 0))
    make_number(0.0, result);

  free(list);
  return result;
}


static awk_value_t*
do_deep_flat_idx(int nargs,
		 awk_value_t *result,
		 __attribute__((unused)) struct awk_ext_func *finfo)
{
  /*
   * Flattens the $nargs[0] array indices into the $nargs[1] array *without* deleting
   * elements already present in the latter.
   * Flattens possibly present subarrays.
   * The flattened array will be indexed with integer values starting from 0.
   * Exits with a fatal error if there are big issues, returns false if
   * everything is not exactly ok but overall there are no errors respecting
   * the requested operations, true if everything is fine.
   */
  assert(result != NULL);
  make_number(0.0, result);

  struct subarrays *list = NULL;
  awk_value_t dest_arr_value;
  awk_array_t dest_array;  
  size_t idx = 0;
  size_t size = 0;
  size_t maxsize = 10;

  if (nargs < 2)
    fatal(ext_id, "two args expected: source_array, dest_array");
  if (NULL == (list = alloc_subarray_list(list, maxsize)))
    fatal(ext_id, "Can't allocate array lists: %s", strerror(errno));
  if (! get_argument(0, AWK_ARRAY, & list[size].source_arr_value))
    fatal(ext_id, "can't retrieve source array");
  if (! get_argument(1, AWK_ARRAY, & dest_arr_value))
    fatal(ext_id, "can't retrieve dest array");

  //XXX+TODO: test if works... not found a better way... seems no api facilities...
  if (list[size].source_arr_value.array_cookie == dest_arr_value.array_cookie)
    fatal(ext_id, "trying to flat an array on itself!");

  dest_array = dest_arr_value.array_cookie;                            /*** MANDATORY ***/
  list[size].source_array = list[size].source_arr_value.array_cookie;  /*** MANDATORY ***/
  size += 1;
  
  list = _deep_flat_idx(list, & dest_array, & idx, & size, & maxsize);
  
  make_number(1, result);

  /* must be called before exit */
  if (! release_subarrays(list, idx, 1, 0))
    make_number(0.0, result);

  free(list);
  return result;
}


static awk_value_t*
do_uniq(int nargs,
	awk_value_t *result,
	__attribute__((unused)) struct awk_ext_func *finfo) {
  /*
   * Populate $nargs[1] with unique elements from $nargs[0] as indexes
   * (and unassigned values). $nargs[2] must be a string about the
   * desired operation, either "i" (for indexes) or "v" (for values).
   * Exits with a fatal error if there are big issues, returns false if
   * everything is not exactly ok but overall there are no errors respecting
   * the requested operations, true if everything is fine.
   * NOTE: is also fatal to pass a $nargs[0] empty or deleted array.
   */
  assert(result != NULL);
  make_number(1.0, result);
  
  struct subarrays *list = NULL;
  awk_value_t what;
  awk_value_t dest_arr_value;
  awk_array_t dest_array;
  awk_value_t flat_arr_value;
  awk_value_t arr_index;
  awk_value_t arr_value;
  awk_array_t flat_array = NULL;
  awk_flat_array_t *flat;
  awk_valtype_t ret;

  String arrname;
  int uniq_on_vals = 0;
  size_t i, idx = 0;
  size_t size = 0;
  size_t maxsize = 10;
  
  if (nargs < 2)
    fatal(ext_id, "at least two args expected: source_array, dest_array");
  if (nargs > 3)
    fatal(ext_id, "too many arguments");
  if (NULL == (list = alloc_subarray_list(list, maxsize)))
    fatal(ext_id, "Can't allocate array lists: %s", strerror(errno));
  
  if (! get_argument(0, AWK_ARRAY, & list[size].source_arr_value))
    fatal(ext_id, "can't retrieve source array");
  if (! get_argument(1, AWK_ARRAY, & dest_arr_value))
    fatal(ext_id, "can't retrieve dest array");
  
  //XXX+TODO: test if works... not found a better way... seems no api facilities...
  if (list[size].source_arr_value.array_cookie == dest_arr_value.array_cookie)
    fatal(ext_id, "trying to uniq() an array on itself!");
  
  if (nargs > 2) {
    if (! get_argument(2, AWK_STRING, & what))
      fatal(ext_id,
	    "can't retrieve dest uniq() string choice (idx|val)");
    if (what.str_value.len != 1)
      fatal(ext_id,
	    "Invalid uniq() string choice (idx|val): <%s>",
	    what.str_value.str);
    switch (what.str_value.str[0]) {
    case 'i':
      uniq_on_vals = 0; break;
    case 'v':
      uniq_on_vals = 1; break;
    default:
      fatal(ext_id,
	    "Invalid uniq() string choice (idx|val): <%s>",
	    what.str_value.str);
    }
  } else {
    uniq_on_vals = 1;
  }

  dest_array = dest_arr_value.array_cookie;                            // *** MANDATORY ***
  list[size].source_array = list[size].source_arr_value.array_cookie;  // *** MANDATORY ***
  size += 1;

  flat_array = create_array();
  flat_arr_value.val_type = AWK_ARRAY;        // *** MANDATORY ***
  flat_arr_value.array_cookie = flat_array;   // *** MANDATORY ***

  if (NULL == (arrname = rand_name(_array_prefix)))
    fatal(ext_id, "Can't create array name");
  if (! sym_update(arrname, & flat_arr_value))
    fatal(ext_id, "sym_update() failed");

  flat_array = flat_arr_value.array_cookie;   // *** MANDATORY after sym_update() ***
     
  if (uniq_on_vals)
    list = _deep_flat(list, & flat_array, & idx, & size, & maxsize);
  else
    list = _deep_flat_idx(list, & flat_array, & idx, & size, & maxsize);

  // flat the flatten array, bleah!
  // in this case, fatal... on the contrary to the cases of NOTE_A
  if (! flatten_array_typed(flat_array, & flat, AWK_STRING, AWK_UNDEFINED))
    fatal(ext_id, "could not flatten source flat_array... maybe empty?");

  // fill the destination array with uniq indexes (and null values)
  for (i = 0; i < flat->count; i++)  {
    if (! (ret = copy_element(flat->elements[i].value, & arr_index))) {
      fatal(ext_id,
	    "Unknown element at index <%zu> (%d)",
	    i, flat->elements[i].value.val_type);
    } else {
      make_null_string(& arr_value);
      if (! set_array_element(dest_array, & arr_index, & arr_value)) {
	fatal(ext_id,
	      "set_array_element() failed "
	      "on scalar value at index <%zu> (%s)",
	      i, _val_types[arr_index.val_type]);
      }
    }
  }

  // must be called before exit
  if (! release_subarrays(list, idx, 1, 0))
    make_number(0.0, result);

  /* XXX: destroy_array() not exposed in gawk API 3.0 */
#ifdef destroy_array
  if (flat_array != NULL)
    if (! destroy_array(flat_array)) {
      make_number(0, result);
      eprint("Can't destroy_array <%s>", arrname);
    }
#endif
  free(list);
  return result;
}



////////////////////////////////////////////////////////////////
////////////////
/* COMPILE WITH (me, not necessary you):
crap0101@orange:~/test$ gcc -fPIC -shared -DHAVE_CONFIG_H -c -O -g -I/usr/include -iquote ~/local/include/awk -Wall -Wextra arrayfuncs.c && gcc -o arrayfuncs.so -shared arrayfuncs.o && cp arrayfuncs.so ~/local/lib/awk/
*/

/******* NOTES ***************************/

/*   dest = create_array(); */
/*   dest_arr_value.val_type = AWK_ARRAY; */
/*   dest_arr_value.array_cookie = new_array; */
/*   sym_update("array", & val);     // install array in the symbol table */
/* new_array = val.array_cookie; */

/* Similarly, if installing a new array as a subarray of an existing
 * array, you must add the new array to its parent before adding any
 * elements to it.
 *
 * You must also retrieve the value of the array_cookie after the call
 * to set_element().
 *
 * Thus, the correct way to build an array is to work "top down".
 * Create the array, and immediately install it in gawk's symbol table
 * using sym_update(), or install it as an element in a previously
 * existing array using set_element().
 * Thus the new array must ultimately be rooted in a global symbol.
 */


/* For example, you might allocate a string value like so:
awk_value_t result;
char *message;
const char greet[] = "Don't Panic!";
emalloc(message, char *, sizeof(greet), "myfunc");
strcpy(message, greet);
make_malloced_string(message, strlen(message), & result);
*/


/*
  dest_array = create_array();
  dest_arr_value.val_type = AWK_ARRAY;
  dest_arr_value.array_cookie = dest_array;
  eprint("foo: %s\n", arr_value.str_value.str);
  // install array in the symbol table 
  if (! sym_update(arr_value.str_value.str,
		   & dest_arr_value)) {
    eprint("foo_array: sym_update() failed on dest array\n");
    return result;
  } 
dest_array = dest_arr_value.array_cookie;  // MANDATORY


XXX: check: https://www.gnu.org/software/gawk/manual/gawk.html#How-To-Create-and-Populate-Arrays
*/
