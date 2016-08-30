#include "ruby.h"

#include <libexif/exif-data.h>

VALUE rb_mExifGeoTag;

static VALUE rb_write_tag(VALUE self, VALUE file_path, VALUE geo_data)
{
    Check_Type(file_path, T_STRING);
    Check_Type(geo_data, T_HASH);

    return Qnil;
}

void Init_exif_geo_tag_ext(void)
{
    rb_mExifGeoTag = rb_define_module("ExifGeoTag");

    rb_define_singleton_method(rb_mExifGeoTag, "write_tag", rb_write_tag, 2);
}
