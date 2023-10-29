#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "gawkapi.h"

#define __module__ "arrayfuncs"
#define eprint(fmt, ...) fprintf(stderr, __msg_prologue fmt, __module__, __func__, ##__VA_ARGS__)
#define _DEBUGLVL 0
#if (_DEBUGLVL)
#define dprint eprint
#define __msg_prologue "Debug: %s @%s: "
#else
#define __msg_prologue "Error: %s @%s: "
#define dprint(fmt, ...) do {} while (0)
#endif

struct subarrays {
  awk_value_t index_val;
  awk_value_t value_val;
  awk_value_t source_arr_value;
  awk_value_t dest_arr_value;
  awk_array_t source_array;
  awk_array_t dest_array;
  awk_flat_array_t *flat_array;
};

static awk_value_t * do_copy_array(int nargs, awk_value_t *result, struct awk_ext_func *finfo);
static awk_value_t * do_deep_flat_array(int nargs, awk_value_t *result, struct awk_ext_func *finfo);
static awk_value_t * do_deep_flat_array_idx(int nargs, awk_value_t *result, struct awk_ext_func *finfo);
//XXX+TODO: add depth parameter to flat to the given depth only
// write do_check_equals function

/* ----- boilerplate code ----- */
int plugin_is_GPL_compatible;
static const gawk_api_t *api;

static awk_ext_id_t ext_id;
static const char *ext_version = "0.1";

static awk_ext_func_t func_table[] = {
  { "copy_array", do_copy_array, 2, 2, awk_false, NULL },
  { "deep_flat_array", do_deep_flat_array, 2, 2, awk_false, NULL },
  { "deep_flat_array_idx", do_deep_flat_array_idx, 2, 2, awk_false, NULL },
};

static awk_bool_t (*init_func)(void) = NULL;


int dl_load(const gawk_api_t *api_p, void *id) {
  api = api_p;
  ext_id = (awk_ext_id_t) &id;
  int errors = 0;
  long unsigned int i;
  
  if (api->major_version != GAWK_API_MAJOR_VERSION
      || api->minor_version < GAWK_API_MINOR_VERSION) {
    eprint("incompatible api version:  %d.%d != %d.%d (extension/gawk version)\n",
	   GAWK_API_MAJOR_VERSION, GAWK_API_MINOR_VERSION, api->major_version, api->minor_version);
    exit(1);
  }
  
  for (i=0; i < sizeof(func_table) / sizeof(awk_ext_func_t); i++) {
    if (! add_ext_func("awk", & func_table[i])) {
      eprint("can't add extension function <%s>\n", func_table[0].name);
      errors++;
    }
  }
  if (ext_version != NULL) {
    register_ext_version(ext_version);
  }
  return (errors == 0);
}

//dl_load_func(func_table, "arrcopy", "awk") // ??? "arrcopy"  /* XXX */
/* ---------------------------- */


/*********************/
/* UTILITY FUNCTIONS */
/*********************/

static int copy_element (awk_value_t arr_item, awk_value_t * dest ) {
  switch (arr_item.val_type) {
  case AWK_STRING:
    make_const_string(arr_item.str_value.str, arr_item.str_value.len, dest);
    return 1;
  case AWK_REGEX:
    make_const_regex(arr_item.str_value.str, arr_item.str_value.len, dest);
    return 1;
  case AWK_STRNUM:
    make_const_user_input(arr_item.str_value.str, arr_item.str_value.len, dest);
    //make_const_string(arr_item.str_value.str, arr_item.str_value.len, dest);
    return 1;
  case AWK_NUMBER:
    make_number(arr_item.num_value, dest);
    return 1;
  case AWK_ARRAY:
    /* later... */
    return 0;
/* not in gawkapi.h (3.0) */
#ifdef AWK_BOOL
  case AWK_BOOL:
    make_bool(arr_item.bool_value, dest);
    return 1;
#endif
  case AWK_UNDEFINED:
    dprint("Undefined: type <%d>\n", AWK_UNDEFINED);
    make_null_string(dest);
    return 1;
  case AWK_SCALAR:  //XXX: should not happen
    eprint("Unsupported type: %d (scalar)\n", arr_item.val_type);
    return 0;
  case AWK_VALUE_COOKIE:  //XXX: should not happen
    eprint("Unsupported type: %d (value_cookie)\n", arr_item.val_type);
    return 0;
  default:  //XXX: should not happen
    eprint("Unknown value type: <%d>\n", arr_item.val_type);
    return 0;
  }
}

struct subarrays * alloc_subarray_list(struct subarrays *list, size_t new_size, int init) {
  if (init) {
    dprint("(malloc) size=%zu\n", new_size);
    if (NULL == (list = malloc(sizeof(struct subarrays) * new_size)))
      return NULL;
    else
      return list;
  } else {
    dprint("(realloc) new_size=%zu\n", new_size);
    if (NULL == (list = realloc(list, sizeof(struct subarrays) * new_size)))
      return NULL;
    else
      return list;
  }
}

/***********************/
/* EXTENSION FUNCTIONS */
/***********************/

static awk_value_t * do_deep_flat_array(int nargs, awk_value_t *result, struct awk_ext_func *finfo) {
  /* Flattens the $nargs[0] array into the $nargs[1] array *without* deleting
   * elements already present in the latter.
   * Flattens possibly present subarrays.
   * The flattened array will be indexed with integer values starting from 0.
   */
  assert(result != NULL);
  make_number(0.0, result);
  if (nargs < 2) {
    eprint("two args expected: source_array, dest_array\n");
    return result;
  }
  struct subarrays *list;
  awk_value_t dest_arr_value;
  awk_value_t index_val;
  awk_array_t dest_array;
  
  size_t i, idx = 0, dest_idx = 0;
  size_t size = 0;
  size_t maxsize = 10;
  awk_valtype_t ret;

  if (NULL == (list = alloc_subarray_list(list, maxsize, 1))) {
    eprint("Can't allocate array lists!\n");
    goto out;
  }
  
  if (! get_argument(0, AWK_ARRAY, & list[size].source_arr_value)) {
    eprint("can't retrieve source array\n");
    goto out;
  }
  if (! get_argument(1, AWK_ARRAY, & dest_arr_value)) {
    eprint("can't retrieve dest array\n");
    goto out;
  }

  dest_array = dest_arr_value.array_cookie;                            /*** MANDATORY ***/
  list[size].source_array = list[size].source_arr_value.array_cookie;  /*** MANDATORY ***/
  size += 1;
  
  do {
    dprint("idx, size, maxsize = %zu %zu %zu\n", idx, size, maxsize);
    if (size >= maxsize-1) {
      maxsize *= 10;
      if (NULL == (list = alloc_subarray_list(list, maxsize, 0))) {
	eprint("Can't reallocate array lists!\n");
	goto out;
      }
    }
    /* flat the array */
    if (! flatten_array_typed(list[idx].source_array,
			      & list[idx].flat_array, AWK_STRING, AWK_UNDEFINED)) {
      eprint("could not flatten source array (idx = %zu)\n", idx);
      goto out;
    }

    dprint("list[%zu].flat_array->count = %zu items\n", idx, list[idx].flat_array->count);
    for (i = 0; i < list[idx].flat_array->count; i++)  {
      if (! (ret = copy_element(list[idx].flat_array->elements[i].value, & list[idx].value_val))) {
	if (list[idx].flat_array->elements[i].value.val_type == AWK_ARRAY) {
	  /* is a subarray, save it and procede */
	  dprint("subarray at index %zu (at size %zu)\n", i, size);
	  list[size].source_array = list[idx].flat_array->elements[i].value.array_cookie;
	  size += 1;
	} else {
	  eprint("Unknown element at index %zu (val_type=%d)\n", i, list[idx].value_val.val_type);
	  goto out;
	}
      } else {
	make_number(dest_idx, & index_val);
	if (! set_array_element(dest_array, & index_val, & list[idx].value_val)) {
	  eprint("set_array_element() failed on scalar value at list[%zu] (dest_idx = %zu)\n", idx, dest_idx);
	  goto out;
	}
	dest_idx += 1;
      }
    }
    idx += 1;
  } while (idx < size);

  make_number(1, result);
 out:
  /* must be called before exit */
  dprint("Release flattened array...\n");
  for (i=0; i < idx; i++) {
    dprint("i, idx, size, maxsize = %zu %zu %zu %zu\n", i, idx, size, maxsize);
    if (! release_flattened_array(list[i].source_array, list[i].flat_array)) {
      eprint("error in release_flattened_array() at index %ld\n", i);
    }
  }
  free(list);
  return result;
}

static awk_value_t * do_deep_flat_array_idx(int nargs, awk_value_t *result, struct awk_ext_func *finfo) {
  /* Flattens the $nargs[0] array indices into the $nargs[1] array *without* deleting
   * elements already present in the latter.
   * Flattens possibly present subarrays.
   * The flattened array will be indexed with integer values starting from 0.
   */
  assert(result != NULL);
  make_number(0.0, result);
  if (nargs < 2) {
    eprint("two args expected: source_array, dest_array\n");
    return result;
  }
  struct subarrays *list;
  awk_value_t dest_arr_value;
  awk_value_t index_val;
  awk_array_t dest_array;
  
  size_t i, idx = 0, dest_idx = 0;
  size_t size = 0;
  size_t maxsize = 10;
  awk_valtype_t ret;

  if (NULL == (list = alloc_subarray_list(list, maxsize, 1))) {
    eprint("Can't allocate array lists!\n");
    goto out;
  }
  
  if (! get_argument(0, AWK_ARRAY, & list[size].source_arr_value)) {
    eprint("can't retrieve source array\n");
    goto out;
  }
  if (! get_argument(1, AWK_ARRAY, & dest_arr_value)) {
    eprint("can't retrieve dest array\n");
    goto out;
  }

  dest_array = dest_arr_value.array_cookie;                            /*** MANDATORY ***/
  list[size].source_array = list[size].source_arr_value.array_cookie;  /*** MANDATORY ***/
  size += 1;
  
  do {
    dprint("idx, size, maxsize = %zu %zu %zu\n", idx, size, maxsize);
    if (size >= maxsize-1) {
      maxsize *= 10;
      if (NULL == (list = alloc_subarray_list(list, maxsize, 0))) {
	eprint("Can't reallocate array lists!\n");
	goto out;
      }
    }
    /* flat the array */
    if (! flatten_array_typed(list[idx].source_array,
			      & list[idx].flat_array, AWK_STRING, AWK_UNDEFINED)) {
      eprint("could not flatten source array (idx = %zu)\n", idx);
      goto out;
    }

    dprint("list[%zu].flat_array->count = %zu items\n", idx, list[idx].flat_array->count);
    for (i = 0; i < list[idx].flat_array->count; i++)  {
      if (! (ret = copy_element(list[idx].flat_array->elements[i].value, & list[idx].value_val))) {
	if (list[idx].flat_array->elements[i].value.val_type == AWK_ARRAY) {
	  /* is a subarray, save it for later but *also* set the index val */
	  dprint("subarray at index %zu (at size %zu)\n", i, size);
	  list[size].source_array = list[idx].flat_array->elements[i].value.array_cookie;
	  size += 1;
	} else {
	  eprint("Unknown element at index %zu (val_type=%d)\n", i, list[idx].value_val.val_type);
	  goto out;
	}
      }
      make_number(dest_idx, & index_val);
      if (! copy_element(list[idx].flat_array->elements[i].index, & list[idx].index_val)) {
	eprint("copy_element() error at array index %zu (arraylist index %zu)\n", i, idx);
	goto out;
      }
      if (! set_array_element(dest_array, & index_val, & list[idx].index_val)) {
	eprint("set_array_element() failed at list[%zu] (dest_idx = %zu)\n", idx, dest_idx);
	goto out;
      }
      dest_idx += 1;
    }
    idx += 1;
  } while (idx < size);

  make_number(1, result);
 out:
  /* must be called before exit */
  dprint("Release flattened array...\n");
  for (i=0; i < idx; i++) {
    dprint("i, idx, size, maxsize = %zu %zu %zu %zu\n", i, idx, size, maxsize);
    if (! release_flattened_array(list[i].source_array, list[i].flat_array)) {
      eprint("error in release_flattened_array() at index %ld\n", i);
    }
  }
  free(list);
  return result;
}

static awk_value_t * do_copy_array(int nargs, awk_value_t *result, struct awk_ext_func *finfo) {
  /* Copies the $nargs[0] array into the $nargs[1] array, *without* deleting
   * elements already present in the latter.
   */
  assert(result != NULL);
  make_number(0.0, result);
  if (nargs < 2) {
    eprint("two args expected: source, dest\n");
    return result;
  }

  struct subarrays *list;
  size_t i, idx = 0;
  size_t size = 0;
  size_t maxsize = 10;
  awk_valtype_t ret;
  
  if (NULL == (list = alloc_subarray_list(list, maxsize, 1))) {
    eprint("Can't allocate array lists!\n");
    goto out;
  }

  /* SOURCE ARRAY */
  if (! get_argument(0, AWK_ARRAY, & list[size].source_arr_value)) {
    eprint("can't retrieve source array\n");
    goto out;
  }
  /* DEST ARRAY */
  if (! get_argument(1, AWK_ARRAY, & list[size].dest_arr_value))  {
    eprint("can't retrieve dest array\n");
    goto out;
  }
  list[size].dest_array = list[size].dest_arr_value.array_cookie;     /*** MANDATORY ***/
  list[size].source_array = list[size].source_arr_value.array_cookie; /*** MANDATORY ***/
  size += 1;
  
  do {
    dprint("idx, size, maxsize = %zu %zu %zu\n", idx, size, maxsize);
    if (size >= maxsize-1) {
      maxsize *= 10;
      if (NULL == (list = alloc_subarray_list(list, maxsize, 0))) {
	eprint("Can't reallocate array lists!\n");
	goto out;
      }
    }
    /* flat the array */
    if (! flatten_array_typed(list[idx].source_array,
			      & list[idx].flat_array, AWK_STRING, AWK_UNDEFINED)) {
      eprint("could not flatten source array\n");
      goto out;
    }

    dprint("list[%zu].flat_array->count = %zu items\n", idx, list[idx].flat_array->count);
    for (i = 0; i < list[idx].flat_array->count; i++)  {
      if (! copy_element(list[idx].flat_array->elements[i].index, & list[idx].index_val)) {
	eprint("copy_element() error at array index %zu (arraylist index %zu)\n", i, idx);
	goto out;
      }
      if (! (ret = copy_element(list[idx].flat_array->elements[i].value, & list[idx].value_val))) {
	if (list[idx].flat_array->elements[i].value.val_type == AWK_ARRAY) {
	  /* is a subarray, save it and procede */
	  dprint("subarray at index %zu\n", i);
	  list[size].dest_array = create_array();
	  list[size].dest_arr_value.val_type = AWK_ARRAY;
	  list[size].dest_arr_value.array_cookie = list[size].dest_array;
	
	  if (! set_array_element(list[idx].dest_array, & list[idx].index_val, & list[size].dest_arr_value)) {
	    eprint("set_array_element() failed on subarray at index %zu\n", idx);
	    size -=1;
	    goto out;
	  }
	  list[size].source_array = list[idx].flat_array->elements[i].value.array_cookie;
	  list[size].dest_array = list[size].dest_arr_value.array_cookie; /*** MANDATORY -- after set_array_element ***/
	  size += 1;
	} else {
	  eprint("Unknown element at index %zu (val_type=%d)\n", i, list[idx].value_val.val_type);
	  goto out;
	}
      } else {
	if (! set_array_element(list[idx].dest_array, & list[idx].index_val, & list[idx].value_val)) {
	  eprint("set_array_element() failed on value at index %zu\n", idx);
	  goto out;
	}
      }
    }
    idx += 1;
  } while (idx < size);
  make_number(1.0, result);
 out:
  /* MANDATORY -- must be called before exit */
  dprint("Release flattened array...\n");
  for (i=0; i < idx; i++) {
    dprint("i, idx, size, maxsize = %zu %zu %zu %zu\n", i, idx, size, maxsize);
    if (! release_flattened_array(list[i].source_array, list[i].flat_array)) {
      eprint("error in release_flattened_array() at index %ld\n", i);
    }
  }
  free(list);
  return result;
}


/* COMPILE WITH:
crap0101@orange:~/test$ gcc -fPIC -shared -DHAVE_CONFIG_H -c -O -g -I/usr/include -Wall -Wextra arrayfuncs.c && gcc -o arrayfuncs.so -shared arrayfuncs.o && cp arrayfuncs.so ~/local/lib/awk/
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
*/


/*  BACKUP COPY */
/* static awk_value_t * do_copy_array(int nargs, awk_value_t *result, struct awk_ext_func *finfo) { */
/*   assert(result != NULL); */
/*   make_number(0.0, result); */
/*   if (nargs < 2) { */
/*     eprint("copy_array: two args expected: source, dest\n"); */
/*     return result; */
/*   } */

/*   awk_value_t source_arr_value, dest_arr_value; */
/*   awk_array_t source_array, dest_array; */
/*   awk_value_t index_val, value_val; */
/*   awk_flat_array_t *flat_array; */

/*   size_t i; */

/*   /\* SOURCE ARRAY *\/ */
/*   if (! get_argument(0, AWK_ARRAY, & source_arr_value)) { */
/*     eprint("copy_array: can't retrieve source array\n"); */
/*     return result; */
/*   } */
/*   source_array = source_arr_value.array_cookie; */
/*   if (! flatten_array_typed(source_array, & flat_array, AWK_STRING, AWK_UNDEFINED)) { */
/*     eprint("copy_array: could not flatten source array\n"); */
/*     return result; */
/*   } */

/*   /\* DEST ARRAY *\/ */
/*   if (! get_argument(1, AWK_ARRAY, & dest_arr_value))  { */
/*     eprint("copy_array: can't retrieve dest array\n"); */
/*     goto out; */
/*   } */
/*   dest_array = dest_arr_value.array_cookie; /\*** MANDATORY ***\/ */

/*   for (i = 0; i < flat_array->count; i++)  { */
/*     if (! copy_element(flat_array->elements[i].index, & index_val)) { */
/*       goto out; */
/*     } */
/*     if (! copy_element(flat_array->elements[i].value, & value_val)) { */
/*       goto out; */
/*     } */
/*     if (! set_array_element(dest_array, & index_val, & value_val)) { */
/*       eprint("set_array_element() failed"); */
/*     } */
/*   } */
/*   make_number(1.0, result); */
/*  out: */
/*   /\* must be called before exit *\/ */
/*   if (! release_flattened_array(source_array, flat_array)) { */
/*     eprint("copy_array: error in release_flattened_array()\n"); */
/*     } */
/*   return result; */
/* } */
