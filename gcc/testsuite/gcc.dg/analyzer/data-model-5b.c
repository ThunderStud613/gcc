/* A toy re-implementation of CPython's object model.  */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>

typedef struct base_obj base_obj;
typedef struct type_obj type_obj;
typedef struct string_obj string_obj;

struct base_obj
{
  struct type_obj *ob_type;
  int ob_refcnt;
};

struct type_obj
{
  base_obj tp_base;
  void (*tp_dealloc) (base_obj *);
};

struct string_obj
{
  base_obj str_base;
  size_t str_len;
  char str_buf[];
};

extern void type_del (base_obj *);
extern void str_del (base_obj *);

type_obj type_type = {
  { &type_type, 1},
  type_del
};

type_obj str_type = {
  { &str_type, 1},
  str_del
};

base_obj *alloc_obj (type_obj *ob_type, size_t sz)
{
  base_obj *obj = (base_obj *)malloc (sz);
  if (!obj)
    return NULL;
  obj->ob_type = ob_type;
  obj->ob_refcnt = 1;
  return obj;
}

string_obj *new_string_obj (const char *str)
{
  //__analyzer_dump ();
  size_t len = strlen (str);
#if 1
  string_obj *str_obj
    = (string_obj *)alloc_obj (&str_type, sizeof (string_obj) + len + 1);
#else
  string_obj *str_obj = (string_obj *)malloc (sizeof (string_obj) + len + 1);
  if (!str_obj)
    return NULL;
  str_obj->str_base.ob_type = &str_type;
  str_obj->str_base.ob_refcnt = 1;
#endif
  str_obj->str_len = len; /* { dg-warning "dereference of NULL 'str_obj'" } */
  memcpy (str_obj->str_buf, str, len);
  str_obj->str_buf[len] = '\0';
  return str_obj;
}

void unref (string_obj *obj)
{
  //__analyzer_dump();
  if (--obj->str_base.ob_refcnt == 0)
    {
      //__analyzer_dump();
      obj->str_base.ob_type->tp_dealloc ((base_obj *)obj); /* { dg-bogus "use of uninitialized value '<unknown>'" "" { xfail *-*-* } } */
      // TODO (xfail): not sure what's going on here
    }
}

void test_1 (const char *str)
{
  string_obj *obj = new_string_obj (str);
  //__analyzer_dump();
  if (obj)
    unref (obj);
} /* { dg-bogus "leak of 'obj'" "" { xfail *-*-* } } */
// TODO (xfail): not sure why this is treated as leaking
