/**
  str.h - Modern dynamic string and string view implementation for plain C.
  By @ilpeN - https://github.com/ilpenSE

  - Completely safe to use in >=C11 and >=C++98. You can use it in C99 but some semantics would be different.
  - No additional libraries required, just libc.
  - It doesn't use any platform specific thing, it uses standart functions and types.
  - Tested in x86-64 GNU/Linux machine with gcc and clang compilers.
  - Code is self-documented. (You can always read implementation or official documentation of that specific function top of its decleration)

  - This header is self-contained, stb-style header:
  ```c
  #define STR_IMPLEMENTATION
  #include "str.h"
  ```c
  - Define `STR_IMPLEMENTATION` somewhere ONCE! Or you get linker errors of multiple definitions
  - And do NOT define it through command line (-DSTR_IMPLEMENTATION) if you have multiple translation units in that command line.
    Because it usually means define this macro in ALL translation units. And you can violate One-Definition-Rule (ODR)
  - WARNING: If you're using MSVC, when compiling this header or TUs with this header, add /Zc:preprocessor flag to have
    a proper preprocessor instead of some shit-ass preprocessor from 90's

  The MIT License
  Copyright (c) 2026 ilpeN

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#ifndef STR_H
#define STR_H

#ifdef __cplusplus
  #define STRDEF extern "C"
#else
  #define STRDEF extern
#endif

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#if defined(_MSC_VER) && !defined(_MSVC_TRADITIONAL) || _MSVC_TRADITIONAL
  #error "DO NOT use MSVC shitty ass traditional preprocessor, add /Zc:preprocessor flag"
#endif

/*
  Definition of dynamic string structure.
  Owns the underlying buffer in heap pointed by `data`
  Allocation and free must be done by using this library.
  See `str_new`, `str_reserve` and `str_free`
*/
#ifndef STR_String_defined
#define STR_String_defined
typedef struct String {
  char *data;
  size_t count;
  size_t capacity;
} String;
#endif

/*
  Definition of string view structure.
  It's just a view/window of String. Doesn't own anything.
*/
#ifndef STR_StringView_defined
#define STR_StringView_defined
typedef struct StringView {
  const char *data;
  size_t count;
} StringView;
#endif

// Do not use these macros directly.
#define _SV_impl(buf, size) (StringView){buf, size}
#define _SV1(buf)           _SV_impl((buf), strlen((buf)))
#define _SV2(buf, size)     _SV_impl((buf), (size))
#define _SV_GET_MACRO(_1, _2, NAME, ...) NAME

/*
  SV(const char *buf, size_t size = strlen(buf))
  This macro creates string view depending on buf that you provided
  If you don't provide any size, it automatically calls strlen to get size
  Use SVL(literal) for creating string view from string literals.
*/
#define SV(...) _SV_GET_MACRO(__VA_ARGS__, _SV2, _SV1)(__VA_ARGS__)
#define SVS(str) SV((str).data, (str).count)
#define SVL(str_lit) (StringView){str_lit, sizeof(str_lit)-1}

/*
  STR(const char *buf, size_t size = strlen(buf))
  This macro creates dynamic string from given buffer with size.
  Use STRL for string literals.
*/
#define STR(...) str_from_sv(SV(__VA_ARGS__))
#define STRL(str_lit) str_from_sv(SVL(str_lit))

/*
  Use SV_FMT and SV_ARG when printing string view via printf
  String literals can be concatenated while compile-time in C.
  So you can use SV_FMT like this: "sv = "SV_FMT"\n"
  SV_ARG also spreads count and data in format which printf wants

  You can use these when printing dynamic Strings btw.
  Or use STR_FMT and STR_ARG they're identical.
*/
#define SV_ARG(sv) \
  (SV_ISINVALID(sv) ? -1 : (int)(sv).count), \
  (SV_ISINVALID(sv) ? "<invalid>" : (sv).data)
#define SV_FMT "%.*s"

#define STR_FMT SV_FMT
#define STR_ARG(str) \
  (STR_ISINVALID(str) ? -1 : (int)(str).count), \
  (STR_ISINVALID(str) ? "<invalid>" : (str).data)

// Dynamic strings functions

// Creates and returns a new string with certain capacity (`init_capacity`)
STRDEF String str_new(size_t init_capacity);

// Creates and returns String from StringView (directly copies buffer viewed by `sv`)
STRDEF String str_from_sv(StringView sv);

// Alias for `str_from_sv`
#define str_from str_from_sv

/*
  Reserves `extra` amount of bytes in heap for string pointed by `self`.
  Returns true if it succeeded, false otherwise.
  Only reallocates memory if it's needed. (count + extra >= capacity)
  Zero initialization of new block is NOT guaranteed! It does NOT increment count field.
  Updates capacity field if reallocation involved.
*/
STRDEF bool str_reserve(String *self, size_t extra);

/*
  Appends one single char to the string.
  Returns true if it succeded, false otherwise. (reallocates if needed, see `str_reserve` for further information)
  NULL-termination of the string is guaranteed. Increments count field.
*/
STRDEF bool str_append(String *self, char c);

/*
  Concatenates StringView called `sv` to the string.
  Returns true if it succeded, false otherwise. (reallocates if needed, see `str_reserve` for further information)
  NULL-termination of the string is guaranteed. Increments count field.
  If you have raw buffer (like c-string) you need to create StringView from it first. (You can inline creating it to parameters)
*/
STRDEF bool str_concat(String *self, StringView sv);

/*
  Frees and zeroes the string. It doesn't do anything if self == NULL
  Double freeing same string (means free(NULL)) is no-op according to the standart (see ISO C11 §7.22.3.3)
*/
STRDEF void str_free(String *self);

/*
  Returns character at that index. (`idx`)
  Returns \0 if idx is out-of-bounds.
*/
STRDEF char str_at(const String *self, size_t idx);

/*
  Compares two string objects. Returns `lhs - rhs` logically.
  Details:
    Returns lhs->count - rhs->count if count fields doesn't equal to each other.
    Returns directly result of memcmp with parameters lhs->data and rhs->data.
    It means the first difference between buffers (left - right usually)
    Doesn't compare capacity fields.
*/
STRDEF int str_cmp(const String *lhs, const String *rhs);

/*
  Returns true if two strings are equal within buffer aspect.
  Wrapper around `str_cmp` see its documentation for further information.
*/
STRDEF bool str_eq(const String *lhs, const String *rhs);

/*
  Creates and returns sub-string out of string `self`.
  Returned string is a new heap object, doesn't refer to self by any chance.
  Range: [start, end); start: inclusive, end: exclusive
  Start and end are indexes to the main string
*/
STRDEF String str_substr(const String *self, size_t start, size_t end);

// String view functions

/*
  Returns sub-StringView out of StringView `sv`.
  Returned string view refers/points to `sv`.
  So make sure that `sv` is pointing to valid (NOT dangling) buffer.
  Range: [start, end); start: inclusive, end: exclusive
  Start and end are indexes to the main string view
*/
STRDEF StringView sv_subsv(StringView sv, size_t start, size_t end);

/*
  Returns sub string view out of string `str`.
  Returned string view refers/points to `str`.
  So make sure that `str` is valid (NOT dangling).
  Range: [start, end); start: inclusive, end: exclusive
  Start and end are indexes to the main string
*/
STRDEF StringView sv_substr(const String *str, size_t start, size_t end);

/*
  Chops string view `self` from left by amount of `count` parameter.
  Returns chopped string view.
*/
STRDEF StringView sv_chop_left(StringView self, size_t count);

// Alias for sv_chop_left
#define sv_remove_prefix sv_chop_left

/*
  Chops the string view `self` from right by amount of `count` parameter.
  Returns chopped string view.
*/
STRDEF StringView sv_chop_right(StringView self, size_t count);

// Alias for sv_chop_right
#define sv_remove_suffix sv_chop_right

/*
  Chops the string view `self` until hitting a certain character or delimiter. (`delim`)
  Chopped view and `self` does NOT include delimiter.
  Edge Cases:
    It counts the first delimiter if there's several delimiters in `self`.
    Chops all of `self` into return value and empties (zeroes) `self`.
    Sets data pointer to NULL if `self` is fully-chopped. (Delimiter was the first character maybe)
*/
STRDEF StringView sv_chop_until_char(StringView *self, char delim);
#define sv_chop_by_delim sv_chop_by_char
#define sv_chop_by_char sv_chop_until_char
#define sv_chop_until_delim sv_chop_until_char

/*
  Chops until the first character that falls into test class checked by function `func`.
  `func` signature is same as functions from `ctype.h`. (e.g.: isspace, isupper islower etc.)
  Edge cases are same with `sv_chop_until_char`
*/
STRDEF StringView sv_chop_by_func(StringView *self, int (*func)(int));

/*
  Trim all whitespaces from left in `self`. (Doesn't mutate underlying buffer, just shrinks window/view)
  Whitespaces are what `isspace` accepts.
  Returns `self` for chain API.
*/
STRDEF StringView *sv_trim_left(StringView *self);

/*
  Trim all whitespaces from right in `self`. See `sv_trim_left` for further information.
*/
STRDEF StringView *sv_trim_right(StringView *self);

/*
  Trim all whitespaces from both sides in `self`. See `sv_trim_left` for further information.
*/
STRDEF StringView *sv_trim(StringView *self);

/*
  If the functions that returns String or StringView
  has returned one of these, it means there's an error.
*/
static const String STR_INVALID = {(char*)0xDEADBEEF, 0, 0};
static const StringView SV_INVALID = {(const char*)0xDEADBEEF, 0};

// You can check errors with these helper macros
#define STR_ISINVALID(str) ((str).data == STR_INVALID.data)
#define SV_ISINVALID(sv) ((sv).data == SV_INVALID.data)

#ifdef STR_IMPLEMENTATION
#include <stdlib.h>
#include <ctype.h>

String str_new(size_t init_capacity)
{
  String res; res.data = NULL; res.count = 0; res.capacity = 0;
  if (!str_reserve(&res, init_capacity)) return STR_INVALID;
  return res;
}

String str_from_sv(StringView sv)
{
  String res; res.data = NULL; res.count = 0; res.capacity = 0;
  if (!str_reserve(&res, sv.count + 1)) return STR_INVALID;
  memcpy(res.data, sv.data, sv.count);
  res.count = sv.count;
  res.data[res.count] = '\0';
  return res;
}

bool str_reserve(String *self, size_t extra)
{
  if (!self) return false;

  if (extra + self->count >= self->capacity) {
    size_t new_cap;
    if (self->capacity == 0) new_cap = extra + self->count;
    else new_cap = self->capacity * 2;
    new_cap += 1; // Space for '\0'

    // realloc(NULL, n) is equivalent to malloc(n), guarenteed in ISO C11 §7.22.3.5
    void *tmp = realloc(self->data, new_cap);
    if (!tmp) return false;
    self->data = (char*)tmp;
    self->capacity = new_cap;
  }
  return true;
}

bool str_append(String *self, char c)
{
  if (!str_reserve(self, 1)) return false;
  self->data[self->count++] = c;
  self->data[self->count] = '\0';
  return true;
}

bool str_concat(String *self, StringView sv)
{
  if (!str_reserve(self, sv.count)) return false;
  memcpy(self->data + self->count, sv.data, sv.count);
  self->count += sv.count;
  self->data[self->count] = '\0';
  return true;
}

void str_free(String *self)
{
  if (!self) return;
  // free(NULL) is no-op according to ISO C11 §7.22.3.3
  if (!STR_ISINVALID(*self)) free(self->data);
  //if (self->data != (char *)0xDEADBEEF) free(self->data);
  self->data = NULL; self->count = 0; self->capacity = 0;
}

char str_at(const String *self, size_t idx)
{
  if (idx >= self->count) return '\0';
  return self->data[idx];
}

int str_cmp(const String *lhs, const String *rhs)
{
  if (lhs->count != rhs->count) return lhs->count - rhs->count;
  return memcmp(lhs->data, rhs->data, lhs->count);
}

bool str_eq(const String *lhs, const String *rhs)
{
  return str_cmp(lhs, rhs) == 0;
}

String str_substr(const String *self, size_t start, size_t end)
{
  if (start > end ||
      start > self->count ||
      end > self->count) return STR_INVALID;
  size_t count = end - start;
  String res = str_new(count + 1);

  memcpy(res.data, self->data + start, count);
  res.data[res.count] = '\0';
  return res;
}

// String View functions implementations

StringView sv_subsv(StringView sv, size_t start, size_t end)
{
  if (start > end ||
      start > sv.count ||
      end > sv.count) return SV_INVALID;
  return (StringView){sv.data + start, end - start};
}

StringView sv_substr(const String *str, size_t start, size_t end)
{
  if (start > end ||
      start > str->count ||
      end > str->count) return SV_INVALID;
  return (StringView){str->data + start, end - start};
}

StringView sv_chop_left(StringView self, size_t count)
{
  if (self.count < count) return SV_INVALID;
  return (StringView){.data=self.data + count, .count=self.count - count};
}

StringView sv_chop_right(StringView self, size_t count)
{
  if (self.count < count) return SV_INVALID;
  return (StringView){.data=self.data, .count=self.count - count};
}

StringView sv_chop_until_char(StringView *self, char delim)
{
  if (!self) return SV_INVALID;

  const char *found = (const char*)memchr(self->data, delim, self->count);
  if (!found) {
    StringView result = *self;
    self->data = NULL;
    self->count = 0;
    return result;
  }

  size_t n = found - self->data;
  StringView result = {.data=self->data, .count=n};

  self->count -= n + 1;
  if (self->count == 0) self->data = NULL;
  else self->data = found + 1;
  return result;
}

StringView sv_chop_by_func(StringView *self, int (*func)(int))
{
  if (!self) return SV_INVALID;

  for (size_t i = 0; i < self->count; i++) {
    char c = self->data[i];
    if (!func((unsigned char)c)) continue;
    // found
    StringView result = {.data=self->data, .count=i};
    self->count -= i + 1;
    if (self->count == 0) self->data = NULL;
    else self->data += i + 1;
    return result;
  }

  // not found
  StringView result = *self;
  self->data = NULL;
  self->count = 0;
  return result;
}

StringView *sv_trim_left(StringView *self)
{
  if (!self || self->count == 0) return self;
  size_t leading_spaces = 0;
  for (const char *c = self->data; c < self->data + self->count; c++) {
    if (isspace((unsigned char)(*c))) leading_spaces += 1;
    else break;
  }

  self->count -= leading_spaces;
  if (self->count == 0) self->data = NULL;
  else self->data += leading_spaces;
  return self;
}

StringView *sv_trim_right(StringView *self)
{
  if (!self || self->count == 0) return self;
  size_t trailing_spaces = 0;
  for (const char *c = self->data + self->count - 1; c >= self->data; c--) {
    if (isspace((unsigned char)(*c))) trailing_spaces += 1;
    else break;
  }
  self->count -= trailing_spaces;
  if (self->count == 0) self->data = NULL;
  return self;
}

StringView *sv_trim(StringView *self)
{
  return sv_trim_right(sv_trim_left(self));
}

#endif // STR_IMPLEMENTATION

#endif // STR_H

/*
  The MIT License
  Copyright (c) 2026 ilpeN

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/
