require 'mkmf'

have_library('exif')
have_header('libexif/exif-data.h')
create_header('config.h')
create_makefile('exif_geo_tag_ext')
