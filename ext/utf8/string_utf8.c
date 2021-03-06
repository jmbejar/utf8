#include "ext.h"
#include "utf8.h"

extern VALUE intern_as_utf8;

/*
 * Document-class: String::UTF8
 */

/*
 * call-seq: length
 *
 * Returns the number of UTF8 characters in this string
 */
static VALUE rb_cString_UTF8_length(VALUE self) {
  unsigned char *str = (unsigned char *)RSTRING_PTR(self);
  size_t len = RSTRING_LEN(self);
  int64_t utf8_len = 0;

  utf8_len = utf8CharCount(str, len);
  if (utf8_len < 0) {
    rb_raise(rb_eArgError, "invalid utf-8 byte sequence");
  }

  return INT2FIX(utf8_len);
}

/*
 * call-seq: each_char {|utf8_char| ...}
 *
 * Iterates over the string, yielding one UTF8 character at a time
 */
static VALUE rb_cString_UTF8_each_char(int argc, VALUE *argv, VALUE self) {
  unsigned char *str = (unsigned char *)RSTRING_PTR(self);
  size_t len = RSTRING_LEN(self), i=0;
  int8_t lastCharLen=0;
  VALUE utf8Str;

  // this will return an Enumerator wrapping this string, yielding this method
  // when Enumerator#each is called
  if (!rb_block_given_p()) {
    return rb_funcall(self, rb_intern("to_enum"), 1, ID2SYM(rb_intern("each_char")));
  }

  for(; i<len; i+=lastCharLen) {
    lastCharLen = utf8CharLen(str, len);
    if (lastCharLen < 0) {
      rb_raise(rb_eArgError, "invalid utf-8 byte sequence");
    }
    utf8Str = rb_str_new((char *)str+i, lastCharLen);
    AS_UTF8(utf8Str);
    rb_yield(utf8Str);
  }

  return self;
}

/*
 * Works like String#[] but taking into account UTF8 character boundaries
 *
 * This method doesn't currently (and may never) support Regexp parameters
 * It also doesn't support a String parameter (yet)
 */
static VALUE rb_cString_UTF8_slice(int argc, VALUE *argv, VALUE self) {
  unsigned char *str = (unsigned char *)RSTRING_PTR(self), *start = str;
  size_t len = RSTRING_LEN(self);
  VALUE utf8Str;

  if (len == 0) return Qnil;

  if (argc == 2) {
    if (TYPE(argv[0]) == T_REGEXP) {
      rb_raise(rb_eArgError, "Regular Expressions aren't supported yet");
    }

    // [offset, length] syntax
    long wantPos = NUM2LONG(argv[0]), curPos = 0, wantLen = NUM2LONG(argv[1]);
    int8_t curCharLen = 0;
    unsigned char *offset = str;

    if (wantLen < 0) {
      return Qnil;
    } else if (wantLen == 0) {
      utf8Str = rb_str_new("", 0);
      AS_UTF8(utf8Str);
      return utf8Str;
    }

    if (wantPos < 0) {
      int64_t char_cnt = utf8CharCount(str, len);
      if (char_cnt < 0) {
        rb_raise(rb_eArgError, "invalid utf-8 byte sequence");
      }
      if ((wantPos * -1) > char_cnt) {
        return Qnil;
      }
      wantPos = char_cnt + wantPos;
    }

    // scan until starting position
    curCharLen = utf8CharLen(str, len);
    if (curCharLen < 0) {
      rb_raise(rb_eArgError, "invalid utf-8 byte sequence");
    }
    while (curPos < wantPos) {
      // if we're about to step out of bounds, return nil
      if ((size_t)(str-start) >= len) {
        return Qnil;
      }

      str += curCharLen;
      curCharLen = utf8CharLen(str, len);
      if (curCharLen < 0) {
        rb_raise(rb_eArgError, "invalid utf-8 byte sequence");
      }
      curPos++;
    }

    // now scan until we have the number of chars asked for
    curPos = 1;
    offset = str;
    str += curCharLen;
    curCharLen = utf8CharLen(str, len);
    if (curCharLen < 0) {
      rb_raise(rb_eArgError, "invalid utf-8 byte sequence");
    }
    while (curPos < wantLen) {
      // if we're about to step out of bounds, stop
      if ((size_t)(str-start) >= len) {
        break;
      }

      str += curCharLen;
      curCharLen = utf8CharLen(str, len);
      if (curCharLen < 0) {
        rb_raise(rb_eArgError, "invalid utf-8 byte sequence");
      }
      curPos++;
    }

    utf8Str = rb_str_new((char *)offset, str-offset);
    AS_UTF8(utf8Str);
    return utf8Str;
  }

  if (argc != 1) {
    rb_raise(rb_eArgError, "wrong number of arguments (%d for 1)", argc);
  }

  // [Fixnum] syntax
  if (TYPE(argv[0]) == T_FIXNUM) {
    long wantPos = NUM2LONG(argv[0]), curPos = 0;
    int8_t curCharLen = 0;

    if (wantPos < 0) {
      int64_t char_cnt = utf8CharCount(str, len);
      if ((wantPos * -1) > char_cnt) {
        return Qnil;
      }
      wantPos = char_cnt + wantPos;
    }

    curCharLen = utf8CharLen(str, len);
    if (curCharLen < 0) {
      rb_raise(rb_eArgError, "invalid utf-8 byte sequence");
    }
    while (curPos < wantPos) {
      // if we're about to step out of bounds, return nil
      if ((size_t)(str-start) >= len) {
        return Qnil;
      }

      str += curCharLen;
      curCharLen = utf8CharLen(str, len);
      if (curCharLen < 0) {
        rb_raise(rb_eArgError, "invalid utf-8 byte sequence");
      }
      curPos++;
    }

    utf8Str = rb_str_new((char *)str, curCharLen);
    AS_UTF8(utf8Str);
    return utf8Str;
  } else {
    if (TYPE(argv[0]) == T_REGEXP) {
      rb_raise(rb_eArgError, "Regular Expressions aren't supported yet");
    }

    // [Range] syntax
    long wantPos, curPos = 0, wantLen;
    int64_t char_cnt = 0;
    int8_t curCharLen = 0;
    unsigned char *offset = str;
    VALUE ret;

    char_cnt = utf8CharCount(str, len);
    ret = rb_range_beg_len(argv[0], &wantPos, &wantLen, char_cnt, 0);

    if (ret == Qnil) {
      return Qnil;
    } else if (ret == Qfalse) {
      // TODO: wtf do we do :P
    }

    if (wantLen == 0) {
      utf8Str = rb_str_new("", 0);
      AS_UTF8(utf8Str);
      return utf8Str;
    }

    // scan until starting position
    curCharLen = utf8CharLen(str, len);
    if (curCharLen < 0) {
      rb_raise(rb_eArgError, "invalid utf-8 byte sequence");
    }
    while (curPos < wantPos) {
      // if we're about to step out of bounds, return ""
      if ((size_t)(str-start) >= len) {
        utf8Str = rb_str_new("", 0);
        AS_UTF8(utf8Str);
        return utf8Str;
      }

      str += curCharLen;
      curCharLen = utf8CharLen(str, len);
      if (curCharLen < 0) {
        rb_raise(rb_eArgError, "invalid utf-8 byte sequence");
      }
      curPos++;
    }

    // now scan until we have the number of chars asked for
    curPos = 1;
    offset = str;
    str += curCharLen;
    curCharLen = utf8CharLen(str, len);
    if (curCharLen < 0) {
      rb_raise(rb_eArgError, "invalid utf-8 byte sequence");
    }
    while (curPos < wantLen) {
      // if we're about to step out of bounds, stop
      if ((size_t)(str-start) >= len) {
        break;
      }

      str += curCharLen;
      curCharLen = utf8CharLen(str, len);
      if (curCharLen < 0) {
        rb_raise(rb_eArgError, "invalid utf-8 byte sequence");
      }
      curPos++;
    }

    utf8Str = rb_str_new((char *)offset, str-offset);
    AS_UTF8(utf8Str);
    return utf8Str;
  }
}

void init_String_UTF8() {
  VALUE rb_cString_UTF8 = rb_define_class_under(rb_cString, "UTF8", rb_cString);

  rb_define_method(rb_cString_UTF8, "length",    rb_cString_UTF8_length, 0);
  rb_define_method(rb_cString_UTF8, "each_char", rb_cString_UTF8_each_char, -1);
  rb_define_method(rb_cString_UTF8, "[]",        rb_cString_UTF8_slice, -1);
}