#include <strings.h>

#include "ruby.h"
#include "ruby/encoding.h"

RUBY_EXTERN VALUE rb_cRational;
RUBY_EXTERN VALUE rb_cTime;
RUBY_EXTERN VALUE rb_cNumeric;

#include <libexif/exif-data.h>
#include <libexif/exif-entry.h>
#include <libexif/exif-loader.h>

#include "jpeg-data.h"

ExifLog *logger;
#ifndef DEBUG
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define exif_log(...)                                                                                                  \
    do {                                                                                                               \
    } while (0)
#elif defined(__GNUC__)
#define exif_log(x...)                                                                                                 \
    do {                                                                                                               \
    } while (0)
#else
#define exif_log (void)
#endif
#endif

VALUE egt_mExifGeoTag;

#define TAG_MAPPING(X)                                                                                                 \
    X(EXIF_TAG_GPS_VERSION_ID, version_id)                                                                             \
    X(EXIF_TAG_GPS_LATITUDE_REF, latitude_ref)                                                                         \
    X(EXIF_TAG_GPS_LATITUDE, latitude)                                                                                 \
    X(EXIF_TAG_GPS_LONGITUDE_REF, longitude_ref)                                                                       \
    X(EXIF_TAG_GPS_LONGITUDE, longitude)                                                                               \
    X(EXIF_TAG_GPS_ALTITUDE_REF, altitude_ref)                                                                         \
    X(EXIF_TAG_GPS_ALTITUDE, altitude)                                                                                 \
    X(EXIF_TAG_GPS_TIME_STAMP, time_stamp)                                                                             \
    X(EXIF_TAG_GPS_DATE_STAMP, date_stamp)                                                                             \
    X(EXIF_TAG_GPS_SATELLITES, satellites)                                                                             \
    X(EXIF_TAG_GPS_STATUS, status)                                                                                     \
    X(EXIF_TAG_GPS_MEASURE_MODE, measure_mode)                                                                         \
    X(EXIF_TAG_GPS_DOP, dop)                                                                                           \
    X(EXIF_TAG_GPS_SPEED_REF, speed_ref)                                                                               \
    X(EXIF_TAG_GPS_SPEED, speed)                                                                                       \
    X(EXIF_TAG_GPS_TRACK_REF, track_ref)                                                                               \
    X(EXIF_TAG_GPS_TRACK, track)                                                                                       \
    X(EXIF_TAG_GPS_IMG_DIRECTION_REF, img_direction_ref)                                                               \
    X(EXIF_TAG_GPS_IMG_DIRECTION, img_direction)                                                                       \
    X(EXIF_TAG_GPS_MAP_DATUM, map_datum)                                                                               \
    X(EXIF_TAG_GPS_DEST_LATITUDE_REF, dest_latitude_ref)                                                               \
    X(EXIF_TAG_GPS_DEST_LATITUDE, dest_latitude)                                                                       \
    X(EXIF_TAG_GPS_DEST_LONGITUDE_REF, dest_longitude_ref)                                                             \
    X(EXIF_TAG_GPS_DEST_LONGITUDE, dest_longitude)                                                                     \
    X(EXIF_TAG_GPS_DEST_BEARING_REF, dest_bearing_ref)                                                                 \
    X(EXIF_TAG_GPS_DEST_BEARING, dest_bearing)                                                                         \
    X(EXIF_TAG_GPS_DEST_DISTANCE_REF, dest_distance_ref)                                                               \
    X(EXIF_TAG_GPS_DEST_DISTANCE, dest_distance)                                                                       \
    X(EXIF_TAG_GPS_PROCESSING_METHOD, processing_method)                                                               \
    X(EXIF_TAG_GPS_AREA_INFORMATION, area_information)                                                                 \
    X(EXIF_TAG_GPS_DIFFERENTIAL, differential)

#define X(e, i) ID egt_sym_##i;
TAG_MAPPING(X)
#undef X

ID egt_sym__latitude;
ID egt_sym__longitude;
ID egt_sym__altitude;
ID egt_sym__timestamp;

ID egt_id_Rational;
ID egt_id_add;
ID egt_id_div;
ID egt_id_sub;
ID egt_id_mul;
ID egt_id_to_f;
ID egt_id_to_i;
ID egt_id_rationalize;
ID egt_id_round;
ID egt_id_split;
ID egt_id_utc;
ID egt_id_numerator;
ID egt_id_denominator;
ID egt_id_strftime;
ID egt_id_hour;
ID egt_id_min;
ID egt_id_sec;
ID egt_id_truncate;
ID egt_id_negative_p;

VALUE egt_str_colon;
VALUE egt_str_period;
VALUE egt_str_date_format;
VALUE egt_str_south;
VALUE egt_str_north;
VALUE egt_str_west;
VALUE egt_str_east;

VALUE egt_flt_min;
VALUE egt_flt_sec;

static void *egt_exif_entry_alloc(ExifMem *mem, ExifEntry *entry, unsigned int size)
{
    void *data;

    if (!entry || !size) {
        return NULL;
    }

    data = exif_mem_alloc(mem, size);
    if (data) {
        return data;
    }

    EXIF_LOG_NO_MEMORY(logger, "RubyExt", size);
    return NULL;
}

static void egt_exif_entry_initialize(ExifMem *mem, ExifEntry *entry, ExifTag tag)
{
    /* We need the byte order */
    if (!entry || !entry->parent || entry->data) {
        return;
    }

    entry->tag = tag;

    switch ((int)tag) {
    case EXIF_TAG_GPS_VERSION_ID:
        /*
         * Indicates the version of GPSInfoIFD. The version is given as 2.2.0.0.
         * This tag is mandatory when GPSInfo tag is present. Note that the
         * GPSVersionID tag is written as a different byte than the Exif Version
         * tag. Default 2.2.0.0
         */
        entry->format = EXIF_FORMAT_BYTE;
        entry->components = 4;
        entry->size = exif_format_get_size(entry->format) * entry->components;
        entry->data = egt_exif_entry_alloc(mem, entry, entry->size);
        entry->data[0] = 2;
        entry->data[1] = 2;
        entry->data[2] = 0;
        entry->data[3] = 0;
        break;
    case EXIF_TAG_GPS_LATITUDE_REF:
        /*
         * Indicates whether the latitude is north or south latitude. The ASCII
         * value 'N' indicates north latitude, and 'S' is south latitude.
         */
        entry->format = EXIF_FORMAT_ASCII;
        entry->components = 2;
        break;
    case EXIF_TAG_GPS_LATITUDE:
        /*
         * Indicates the latitude. The latitude is expressed as three RATIONAL
         * values giving the degrees, minutes, and seconds, respectively. If
         * latitude is expressed as degrees, minutes and seconds, a typical
         * format would be dd/1,mm/1,ss/1. When degrees and minutes are used
         * and, for example, fractions of minutes are given up to two decimal
         * places, the format would be dd/1,mmmm/100,0/1.
         */
        entry->format = EXIF_FORMAT_RATIONAL;
        entry->components = 3;
        break;
    case EXIF_TAG_GPS_LONGITUDE_REF:
        /*
         * Indicates whether the longitude is east or west longitude. ASCII 'E'
         * indicates east longitude, and 'W' is west longitude.
         */
        entry->format = EXIF_FORMAT_ASCII;
        entry->components = 2;
        break;
    case EXIF_TAG_GPS_LONGITUDE:
        /*
         * Indicates the longitude. The longitude is expressed as three RATIONAL
         * values giving the degrees, minutes, and seconds, respectively. If
         * longitude is expressed as degrees, minutes and seconds, a typical
         * format would be ddd/1,mm/1,ss/1. When degrees and minutes are used
         * and, for example, fractions of minutes are given up to two decimal
         * places, the format would be ddd/1,mmmm/100,0/1.
         */
        entry->format = EXIF_FORMAT_RATIONAL;
        entry->components = 3;
        break;

    case EXIF_TAG_GPS_ALTITUDE_REF:
        /*
         * Indicates the altitude used as the reference altitude. If the
         * reference is sea level and the altitude is above sea level, 0 is
         * given. If the altitude is below sea level, a value of 1 is given and
         * the altitude is indicated as an absolute value in the GPSAltitude
         * tag. The reference unit is meters. Note that this tag is BYTE type,
         * unlike other reference tags. Default 0.
         */
        entry->format = EXIF_FORMAT_BYTE;
        entry->components = 1;
        entry->size = exif_format_get_size(entry->format) * entry->components;
        entry->data = egt_exif_entry_alloc(mem, entry, entry->size);
        entry->data[0] = 0;
        break;
    case EXIF_TAG_GPS_ALTITUDE:
        /*
         * Indicates the altitude based on the reference in GPSAltitudeRef.
         * Altitude is expressed as one RATIONAL value. The reference unit is
         * meters.
         */
        entry->format = EXIF_FORMAT_RATIONAL;
        entry->components = 1;
        break;
    case EXIF_TAG_GPS_TIME_STAMP:
        /*
         * Indicates the time as UTC (Coordinated Universal Time). TimeStamp is
         * expressed as three RATIONAL values giving the hour, minute, and
         * second.
         */
        entry->format = EXIF_FORMAT_RATIONAL;
        entry->components = 3;
        break;
    case EXIF_TAG_GPS_SATELLITES:
        /*
         * Indicates the GPS satellites used for measurements. This tag can be
         * used to describe the number of satellites, their ID number, angle of
         * elevation, azimuth, SNR and other information in ASCII notation. The
         * format is not specified. If the GPS receiver is incapable of taking
         * measurements, value of the tag shall be set to NULL.
         */
        entry->format = EXIF_FORMAT_ASCII;
        entry->components = 0;
        break;
    case EXIF_TAG_GPS_STATUS:
        /*
         * Indicates the status of the GPS receiver when the image is recorded.
         * 'A' means measurement is in progress, and 'V' means the measurement
         * is Interoperability.
         */
        entry->format = EXIF_FORMAT_ASCII;
        entry->components = 2;
        break;
    case EXIF_TAG_GPS_MEASURE_MODE:
        /*
         * Indicates the GPS measurement mode. '2' means two-dimensional
         * measurement and '3' means three-dimensional measurement is in
         * progress.
         */
        entry->format = EXIF_FORMAT_ASCII;
        entry->components = 2;
        break;
    case EXIF_TAG_GPS_DOP:
        /*
         * Indicates the GPS DOP (data degree of precision). An HDOP value is
         * written during two-dimensional measurement, and PDOP during
         * three-dimensional measurement.
         */
        entry->format = EXIF_FORMAT_ASCII;
        entry->components = 2;
        break;
    case EXIF_TAG_GPS_SPEED_REF:
        /*
         * Indicates the unit used to express the GPS receiver speed of
         * movement. 'K' 'M' and 'N' represents kilometers per hour, miles per
         * hour, and knots. Default 'K'.
         */
        entry->format = EXIF_FORMAT_ASCII;
        entry->components = 2;
        entry->size = exif_format_get_size(entry->format) * entry->components;
        entry->data = egt_exif_entry_alloc(mem, entry, entry->size);
        entry->data[0] = 'K';
        entry->data[1] = 0;
        break;
    case EXIF_TAG_GPS_SPEED:
        /*
         * Indicates the speed of GPS receiver movement.
         */
        entry->format = EXIF_FORMAT_RATIONAL;
        entry->components = 1;
        break;
    case EXIF_TAG_GPS_TRACK_REF:
        /*
         * Indicates the reference for giving the direction of GPS receiver
         * movement. 'T' denotes true direction and 'M' is magnetic direction.
         * Default 'T'.
         */
        entry->format = EXIF_FORMAT_ASCII;
        entry->components = 2;
        entry->size = exif_format_get_size(entry->format) * entry->components;
        entry->data = egt_exif_entry_alloc(mem, entry, entry->size);
        entry->data[0] = 'T';
        entry->data[1] = 0;
        break;
    case EXIF_TAG_GPS_TRACK:
        /*
         * Indicates the direction of GPS receiver movement. The range of values
         * is from 0.00 to 359.99.
         */
        entry->format = EXIF_FORMAT_RATIONAL;
        entry->components = 1;
        break;
    case EXIF_TAG_GPS_IMG_DIRECTION_REF:
        /*
         * Indicates the reference for giving the direction of the image when it
         * is captured. 'T' denotes true direction and 'M' is magnetic
         * direction. Default 'T'.
         */
        entry->format = EXIF_FORMAT_ASCII;
        entry->components = 2;
        entry->size = exif_format_get_size(entry->format) * entry->components;
        entry->data = egt_exif_entry_alloc(mem, entry, entry->size);
        entry->data[0] = 'T';
        entry->data[1] = 0;
        break;
    case EXIF_TAG_GPS_IMG_DIRECTION:
        /*
         * Indicates the direction of the image when it was captured. The range
         * of values is from 0.00 to 359.99.
         */
        entry->format = EXIF_FORMAT_RATIONAL;
        entry->components = 1;
        break;
    case EXIF_TAG_GPS_MAP_DATUM:
        /*
         * Indicates the geodetic survey data used by the GPS receiver. If the
         * survey data is restricted to Japan, the value of this tag is 'TOKYO'
         * or 'WGS-84'. If a GPS Info tag is recorded, it is strongly
         * recommended that this tag be recorded.
         */
        entry->format = EXIF_FORMAT_ASCII;
        entry->components = 0;
        break;
    case EXIF_TAG_GPS_DEST_LATITUDE_REF:
        /*
         * Indicates whether the latitude of the destination point is north or
         * south latitude. The ASCII value 'N' indicates north latitude, and 'S'
         * is south latitude.
         */
        entry->format = EXIF_FORMAT_ASCII;
        entry->components = 2;
        break;
    case EXIF_TAG_GPS_DEST_LATITUDE:
        /*
         * Indicates the latitude of the destination point. The latitude is
         * expressed as three RATIONAL values giving the degrees, minutes, and
         * seconds, respectively. If latitude is expressed as degrees, minutes
         * and seconds, a typical format would be dd/1,mm/1,ss/1. When degrees
         * and minutes are used and, for example, fractions of minutes are given
         * up to two decimal places, the format would be dd/1,mmmm/100,0/1.
         */
        entry->format = EXIF_FORMAT_RATIONAL;
        entry->components = 3;
        break;
    case EXIF_TAG_GPS_DEST_LONGITUDE_REF:
        /*
         * Indicates whether the longitude of the destination point is east or
         * west longitude. ASCII 'E' indicates east longitude, and 'W' is west
         * longitude.
         */
        entry->format = EXIF_FORMAT_ASCII;
        entry->components = 2;
        break;
    case EXIF_TAG_GPS_DEST_LONGITUDE:
        /*
         * Indicates the longitude of the destination point. The longitude is
         * expressed as three RATIONAL values giving the degrees, minutes, and
         * seconds, respectively. If longitude is expressed as degrees, minutes
         * and seconds, a typical format would be ddd/1,mm/1,ss/1. When degrees
         * and minutes are used and, for example, fractions of minutes are given
         * up to two decimal places, the format would be ddd/1,mmmm/100,0/1.
         */
        entry->format = EXIF_FORMAT_RATIONAL;
        entry->components = 3;
        break;
    case EXIF_TAG_GPS_DEST_BEARING_REF:
        /*
         * Indicates the reference used for giving the bearing to the
         * destination point. 'T' denotes true direction and 'M' is magnetic
         * direction. Default 'T'.
         */
        entry->format = EXIF_FORMAT_ASCII;
        entry->components = 2;
        entry->size = exif_format_get_size(entry->format) * entry->components;
        entry->data = egt_exif_entry_alloc(mem, entry, entry->size);
        entry->data[0] = 'T';
        entry->data[1] = 0;
        break;
    case EXIF_TAG_GPS_DEST_BEARING:
        /*
         * Indicates the bearing to the destination point. The range of values
         * is from 0.00 to 359.99.
         */
        entry->format = EXIF_FORMAT_RATIONAL;
        entry->components = 1;
        break;
    case EXIF_TAG_GPS_DEST_DISTANCE_REF:
        /*
         * Indicates the unit used to express the distance to the destination
         * point. 'K', 'M' and 'N' represent kilometers, miles and knots.
         * Default 'K'.
         */
        entry->format = EXIF_FORMAT_ASCII;
        entry->components = 2;
        entry->size = exif_format_get_size(entry->format) * entry->components;
        entry->data = egt_exif_entry_alloc(mem, entry, entry->size);
        entry->data[0] = 'K';
        entry->data[1] = 0;
        break;
    case EXIF_TAG_GPS_DEST_DISTANCE:
        /*
         * Indicates the distance to the destination point.
         */
        entry->format = EXIF_FORMAT_RATIONAL;
        entry->components = 1;
        break;
    case EXIF_TAG_GPS_PROCESSING_METHOD:
        /*
         * A character string recording the name of the method used for location
         * finding. The first byte indicates the character code used (Table
         * 6,Table 7), and this is followed by the name of the method. Since
         * the Type is not ASCII, NULL termination is not necessary.
         */
        entry->format = EXIF_FORMAT_UNDEFINED;
        entry->components = 0;
        break;
    case EXIF_TAG_GPS_AREA_INFORMATION:
        /*
         * A character string recording the name of the GPS area. The first byte
         * indicates the character code used (Table 6, Table 7), and this is
         * followed by the name of the GPS area. Since the Type is not ASCII,
         * NULL termination is not necessary.
         */
        entry->format = EXIF_FORMAT_UNDEFINED;
        entry->components = 0;
        break;
    case EXIF_TAG_GPS_DATE_STAMP:
        /*
         * A character string recording date and time information relative to
         * UTC (Coordinated Universal Time). The format is "YYYY:MM:DD." The
         * length of the string is 11 bytes including NULL.
         */
        entry->format = EXIF_FORMAT_ASCII;
        entry->components = 11;
        break;
    case EXIF_TAG_GPS_DIFFERENTIAL:
        /*
         * Indicates whether differential correction is applied to the GPS
         * receiver. 0 = Measurement without differential correction. 1 =
         * Differential correction applied.
         */
        entry->format = EXIF_FORMAT_SHORT;
        entry->components = 1;
        break;
    default:
        exif_log(logger, EXIF_LOG_CODE_DEBUG, "RubyExt", "Unexpected tag %d", (int)tag);
        return;
    }
    if (entry->data == NULL) {
        entry->size = exif_format_get_size(entry->format) * entry->components;
        if (entry->size > 0) {
            entry->data = egt_exif_entry_alloc(mem, entry, entry->size);
        }
    }
}

#define CF(entry, expected)                                                                                            \
    {                                                                                                                  \
        if (entry->format != expected) {                                                                               \
            exif_log(logger, EXIF_LOG_CODE_DEBUG, "RubyExt",                                                           \
                     "Invalid format '%s' for tag '%s' (0x%04x), expected '%s'.", exif_format_get_name(entry->format), \
                     exif_tag_get_name_in_ifd(entry->tag, EXIF_IFD_GPS), (int)entry->tag,                              \
                     exif_format_get_name(expected));                                                                  \
            break;                                                                                                     \
        }                                                                                                              \
    }

#define CC(entry, expected)                                                                                            \
    {                                                                                                                  \
        if (entry->components != expected) {                                                                           \
            exif_log(logger, EXIF_LOG_CODE_DEBUG, "RubyExt", "Invalid number of components %i for tag '%s' (0x%04x), " \
                                                             "expected %i.",                                           \
                     (int)entry->components, exif_tag_get_name_in_ifd(entry->tag, EXIF_IFD_GPS), (int)entry->tag,      \
                     (int)expected);                                                                                   \
            break;                                                                                                     \
        }                                                                                                              \
    }

static VALUE egt_exif_entry_get_value(ExifEntry *entry)
{
    ExifByteOrder byte_order;
    ExifRational rat;
    VALUE val;
    unsigned long i;

    byte_order = exif_data_get_byte_order(entry->parent->parent);

    /* Sanity check */
    if (entry->size != entry->components * exif_format_get_size(entry->format)) {
        exif_log(logger, EXIF_LOG_CODE_DEBUG, "RubyExt", "Invalid size of entry with tag %d (%i, expected %li x %i).",
                 (int)entry->tag, entry->size, entry->components, exif_format_get_size(entry->format));
        return Qnil;
    }

#define READ_AND_RETURN_RATIONAL()                                                                                     \
    val = rb_ary_new();                                                                                                \
    if (entry->components == 1) {                                                                                      \
        rat = exif_get_rational(entry->data, byte_order);                                                              \
        if (rat.numerator == 0 && rat.denominator == 0) {                                                              \
            rat.denominator = 1;                                                                                       \
        }                                                                                                              \
        return rb_funcall(rb_mKernel, egt_id_Rational, 2, INT2FIX(rat.numerator), INT2FIX(rat.denominator));           \
    } else {                                                                                                           \
        for (i = 0; i < entry->components; i++) {                                                                      \
            rat = exif_get_rational(entry->data + exif_format_get_size(entry->format) * i, byte_order);                \
            if (rat.numerator == 0 && rat.denominator == 0) {                                                          \
                rat.denominator = 1;                                                                                   \
            }                                                                                                          \
            rb_ary_push(val,                                                                                           \
                        rb_funcall(rb_mKernel, egt_id_Rational, 2, INT2FIX(rat.numerator), INT2FIX(rat.denominator))); \
        }                                                                                                              \
        return val;                                                                                                    \
    }

    switch ((int)entry->tag) {
    case EXIF_TAG_GPS_VERSION_ID:
        CF(entry, EXIF_FORMAT_BYTE);
        CC(entry, 4);
        return rb_sprintf("%u.%u.%u.%u", entry->data[0], entry->data[1], entry->data[2], entry->data[3]);
    case EXIF_TAG_GPS_LONGITUDE_REF:
    case EXIF_TAG_GPS_STATUS:
    case EXIF_TAG_GPS_MEASURE_MODE:
    case EXIF_TAG_GPS_DOP:
    case EXIF_TAG_GPS_SPEED_REF:
    case EXIF_TAG_GPS_LATITUDE_REF:
    case EXIF_TAG_GPS_TRACK_REF:
    case EXIF_TAG_GPS_IMG_DIRECTION_REF:
    case EXIF_TAG_GPS_DEST_LATITUDE_REF:
    case EXIF_TAG_GPS_DEST_LONGITUDE_REF:
    case EXIF_TAG_GPS_DEST_BEARING_REF:
    case EXIF_TAG_GPS_DEST_DISTANCE_REF:
        CF(entry, EXIF_FORMAT_ASCII);
        CC(entry, 2);
        return rb_str_new_cstr((char *)entry->data);
    case EXIF_TAG_GPS_ALTITUDE_REF:
        CF(entry, EXIF_FORMAT_BYTE);
        CC(entry, 1);
        return INT2FIX(entry->data[0]);
    case EXIF_TAG_GPS_ALTITUDE:
    case EXIF_TAG_GPS_SPEED:
    case EXIF_TAG_GPS_TRACK:
    case EXIF_TAG_GPS_IMG_DIRECTION:
    case EXIF_TAG_GPS_DEST_BEARING:
    case EXIF_TAG_GPS_DEST_DISTANCE:
        CF(entry, EXIF_FORMAT_RATIONAL);
        CC(entry, 1);
        READ_AND_RETURN_RATIONAL();
    case EXIF_TAG_GPS_TIME_STAMP:
    case EXIF_TAG_GPS_LATITUDE:
    case EXIF_TAG_GPS_LONGITUDE:
    case EXIF_TAG_GPS_DEST_LATITUDE:
    case EXIF_TAG_GPS_DEST_LONGITUDE:
        CF(entry, EXIF_FORMAT_RATIONAL);
        CC(entry, 3);
        READ_AND_RETURN_RATIONAL();
    case EXIF_TAG_GPS_SATELLITES:
    case EXIF_TAG_GPS_MAP_DATUM:
        CF(entry, EXIF_FORMAT_ASCII);
        return rb_str_new_cstr((char *)entry->data);
    case EXIF_TAG_GPS_PROCESSING_METHOD:
    case EXIF_TAG_GPS_AREA_INFORMATION:
        return rb_str_new((char *)entry->data, entry->size);
    case EXIF_TAG_GPS_DATE_STAMP:
        CF(entry, EXIF_FORMAT_ASCII);
        CC(entry, 11);
        return rb_str_new_cstr((char *)entry->data);
    case EXIF_TAG_GPS_DIFFERENTIAL:
        CF(entry, EXIF_FORMAT_SHORT);
        CC(entry, 1);
        return INT2FIX(exif_get_short(entry->data, byte_order));
    default:
        exif_log(logger, EXIF_LOG_CODE_DEBUG, "RubyExt", "Unexpected tag %d", (int)entry->tag);
    }
    return Qnil;
}

static void egt_generate_virtual_fields(VALUE values)
{
    VALUE val;

#define CONVERT_COORDINATES(name, from, to)                                                                            \
    val = rb_hash_aref(values, from);                                                                                  \
    if (val != Qnil) {                                                                                                 \
        Check_Type(val, T_ARRAY);                                                                                      \
        if (RARRAY_LEN(val) == 3) {                                                                                    \
            VALUE deg = rb_ary_entry(val, 0), min = rb_funcall(rb_ary_entry(val, 1), egt_id_div, 1, egt_flt_min),      \
                  sec = rb_funcall(rb_ary_entry(val, 2), egt_id_div, 1, egt_flt_sec);                                  \
            rb_hash_aset(values, to, rb_funcall(rb_funcall(deg, egt_id_add, 1, rb_funcall(min, egt_id_add, 1, sec)),   \
                                                egt_id_to_f, 0));                                                      \
        } else {                                                                                                       \
            exif_log(logger, EXIF_LOG_CODE_DEBUG, "RubyExt", "Expected " name " to have 3 items, but got %ld",         \
                     RARRAY_LEN(val));                                                                                 \
        }                                                                                                              \
    }

    CONVERT_COORDINATES(":latitude", egt_sym_latitude, egt_sym__latitude);
    CONVERT_COORDINATES(":longitude", egt_sym_longitude, egt_sym__longitude);
    val = rb_hash_aref(values, egt_sym_altitude);
    if (val != Qnil) {
        rb_hash_aset(values, egt_sym__altitude, rb_funcall(val, egt_id_to_f, 0));
    }
    {
        VALUE time = rb_hash_aref(values, egt_sym_time_stamp);
        VALUE date = rb_hash_aref(values, egt_sym_date_stamp);

        if (time != Qnil && date != Qnil) {
            VALUE sdate;
            Check_Type(time, T_ARRAY);
            Check_Type(date, T_STRING);
            sdate = rb_funcall(date, egt_id_split, 1, egt_str_colon);
            if (RARRAY_LEN(time) == 3) {
                if (RARRAY_LEN(sdate) == 3) {
                    VALUE hour = rb_funcall(rb_ary_entry(time, 0), egt_id_to_i, 0),
                          min = rb_funcall(rb_ary_entry(time, 1), egt_id_to_i, 0),
                          sec = rb_funcall(rb_ary_entry(time, 2), egt_id_to_i, 0);
                    VALUE year = rb_funcall(rb_ary_entry(sdate, 0), egt_id_to_i, 0),
                          month = rb_funcall(rb_ary_entry(sdate, 1), egt_id_to_i, 0),
                          day = rb_funcall(rb_ary_entry(sdate, 2), egt_id_to_i, 0);
                    rb_hash_aset(values, egt_sym__timestamp,
                                 rb_funcall(rb_cTime, egt_id_utc, 6, year, month, day, hour, min, sec));
                } else {
                    exif_log(logger, EXIF_LOG_CODE_DEBUG, "RubyExt",
                             "Expected :date_stamp to have 3 sections separated by ':', but got %ld",
                             RARRAY_LEN(sdate));
                }
            } else {
                exif_log(logger, EXIF_LOG_CODE_DEBUG, "RubyExt", "Expected :time_stamp to have 3 items, but got %ld",
                         RARRAY_LEN(time));
            }
        }
    }
#undef CONVERT_COORDINATES
}

static void egt_exif_entry_set_value(ExifMem *mem, ExifEntry *entry, VALUE values, ID key)
{
    VALUE val = rb_hash_aref(values, key);
    ExifByteOrder byte_order = exif_data_get_byte_order(entry->parent->parent);

    switch ((int)entry->tag) {
    case EXIF_TAG_GPS_VERSION_ID:
        if (val != Qnil) {
            VALUE sval;

            Check_Type(val, T_STRING);
            sval = rb_funcall(val, egt_id_split, 1, egt_str_period);
            if (RARRAY_LEN(sval) == 3) {
                entry->data[0] = (ExifByte)FIX2INT(rb_funcall(rb_ary_entry(sval, 0), egt_id_to_i, 0));
                entry->data[1] = (ExifByte)FIX2INT(rb_funcall(rb_ary_entry(sval, 1), egt_id_to_i, 0));
                entry->data[2] = (ExifByte)FIX2INT(rb_funcall(rb_ary_entry(sval, 2), egt_id_to_i, 0));
                entry->data[3] = (ExifByte)FIX2INT(rb_funcall(rb_ary_entry(sval, 3), egt_id_to_i, 0));
            } else {
                exif_log(logger, EXIF_LOG_CODE_DEBUG, "RubyExt",
                         "Expected :gps_version_id to have 4 sections separated by '.', but got %ld", RARRAY_LEN(sval));
            }
        }
        return;
    case EXIF_TAG_GPS_LONGITUDE_REF:
    case EXIF_TAG_GPS_STATUS:
    case EXIF_TAG_GPS_MEASURE_MODE:
    case EXIF_TAG_GPS_DOP:
    case EXIF_TAG_GPS_SPEED_REF:
    case EXIF_TAG_GPS_LATITUDE_REF:
    case EXIF_TAG_GPS_TRACK_REF:
    case EXIF_TAG_GPS_IMG_DIRECTION_REF:
    case EXIF_TAG_GPS_DEST_LATITUDE_REF:
    case EXIF_TAG_GPS_DEST_LONGITUDE_REF:
    case EXIF_TAG_GPS_DEST_BEARING_REF:
    case EXIF_TAG_GPS_DEST_DISTANCE_REF:
        if (val != Qnil) {
            Check_Type(val, T_STRING);
            if (entry->data) {
                exif_mem_free(mem, entry->data);
            }
            entry->components = 2;
            entry->size = entry->components * sizeof(char);
            entry->data = exif_mem_alloc(mem, entry->components * sizeof(char));
            entry->data[0] = RSTRING_PTR(val)[0];
            entry->data[1] = 0;
        }
        return;
    case EXIF_TAG_GPS_ALTITUDE_REF:
        if (val != Qnil) {
            Check_Type(val, T_FIXNUM);
            if (entry->data) {
                exif_mem_free(mem, entry->data);
            }
            entry->components = 1;
            entry->size = entry->components * sizeof(ExifByte);
            entry->data = exif_mem_alloc(mem, entry->components * sizeof(ExifByte));
            entry->data[0] = (ExifByte)FIX2INT(val);
        }
        return;
    case EXIF_TAG_GPS_ALTITUDE:
    case EXIF_TAG_GPS_SPEED:
    case EXIF_TAG_GPS_TRACK:
    case EXIF_TAG_GPS_IMG_DIRECTION:
    case EXIF_TAG_GPS_DEST_BEARING:
    case EXIF_TAG_GPS_DEST_DISTANCE:
        if (val != Qnil) {
            ExifRational rat;

            val = rb_funcall(val, egt_id_rationalize, 0), rat.numerator = FIX2INT(rb_funcall(val, egt_id_numerator, 0));
            rat.denominator = FIX2INT(rb_funcall(val, egt_id_denominator, 0));
            if (entry->data) {
                exif_mem_free(mem, entry->data);
            }
            entry->components = 1;
            entry->size = entry->components * sizeof(ExifRational);
            entry->data = exif_mem_alloc(mem, entry->components * sizeof(ExifRational));
            exif_set_rational(entry->data, byte_order, rat);
        }
        return;
    case EXIF_TAG_GPS_TIME_STAMP:
    case EXIF_TAG_GPS_LATITUDE:
    case EXIF_TAG_GPS_LONGITUDE:
    case EXIF_TAG_GPS_DEST_LATITUDE:
    case EXIF_TAG_GPS_DEST_LONGITUDE:
        if (val != Qnil) {
            Check_Type(val, T_ARRAY);

            if (RARRAY_LEN(val) == 3) {
                unsigned long i;

                entry->components = 3;
                entry->size = entry->components * sizeof(ExifRational);
                entry->data = exif_mem_alloc(mem, entry->components * sizeof(ExifRational));

                for (i = 0; i < entry->components; i++) {
                    ExifRational rat;
                    VALUE vrat;
                    vrat = rb_funcall(rb_ary_entry(val, i), egt_id_rationalize, 0);
                    rat.numerator = FIX2INT(rb_funcall(vrat, egt_id_numerator, 0));
                    rat.denominator = FIX2INT(rb_funcall(vrat, egt_id_denominator, 0));
                    exif_set_rational(entry->data + exif_format_get_size(entry->format) * i, byte_order, rat);
                }
            } else {
                exif_log(logger, EXIF_LOG_CODE_DEBUG, "RubyExt",
                         "Expected %s to have 4 sections separated by '.', but got %ld", StringValueCStr(key),
                         RARRAY_LEN(val));
            }
        }
        return;
    case EXIF_TAG_GPS_SATELLITES:
    case EXIF_TAG_GPS_MAP_DATUM:
    case EXIF_TAG_GPS_PROCESSING_METHOD:
    case EXIF_TAG_GPS_AREA_INFORMATION:
        if (val != Qnil) {
            Check_Type(val, T_STRING);
            if (entry->data) {
                exif_mem_free(mem, entry->data);
            }
            entry->components = RSTRING_LEN(val) + 1;
            entry->size = entry->components * sizeof(char);
            entry->data = exif_mem_alloc(mem, entry->components * sizeof(char));
            memcpy(entry->data, RSTRING_PTR(val), RSTRING_LEN(val));
            entry->data[RSTRING_LEN(val)] = 0;
        }
        return;
    case EXIF_TAG_GPS_DATE_STAMP:
        if (val != Qnil) {
            Check_Type(val, T_STRING);
            if (RSTRING_LEN(val) == 10) {
                if (entry->data) {
                    exif_mem_free(mem, entry->data);
                }
                entry->components = 11;
                entry->size = entry->components * sizeof(char);
                entry->data = exif_mem_alloc(mem, entry->components * sizeof(char));
                memcpy(entry->data, RSTRING_PTR(val), 10);
                entry->data[10] = 0;
            }
        }
        return;
    case EXIF_TAG_GPS_DIFFERENTIAL:
        if (val != Qnil) {
            Check_Type(val, T_FIXNUM);
            if (entry->data) {
                exif_mem_free(mem, entry->data);
            }
            entry->components = 1;
            entry->size = entry->components * sizeof(ExifShort);
            entry->data = exif_mem_alloc(mem, entry->components * sizeof(ExifShort));
            exif_set_short(entry->data, byte_order, (ExifShort)FIX2INT(val));
        }
        return;
    default:
        exif_log(logger, EXIF_LOG_CODE_DEBUG, "RubyExt", "Unexpected tag %d", (int)entry->tag);
    }
}

static void egt_parse_virtual_fields(VALUE values)
{

    VALUE val;

    val = rb_hash_aref(values, egt_sym__altitude);
    if (val != Qnil) {
        rb_hash_aset(values, egt_sym_altitude, rb_funcall(val, egt_id_rationalize, 0));
    }

    val = rb_hash_aref(values, egt_sym__timestamp);
    if (val != Qnil) {
        VALUE time;
        if (!rb_obj_is_kind_of(val, rb_cTime)) {
            rb_raise(rb_eTypeError, "wrong argument (%" PRIsVALUE ")! (Expected kind of %" PRIsVALUE ")",
                     rb_obj_class(val), rb_cTime);
        }
        rb_hash_aset(values, egt_sym_date_stamp, rb_funcall(val, egt_id_strftime, 1, egt_str_date_format));
        time = rb_ary_new();
        rb_ary_push(time, rb_funcall(rb_funcall(val, egt_id_hour, 0), egt_id_rationalize, 0));
        rb_ary_push(time, rb_funcall(rb_funcall(val, egt_id_min, 0), egt_id_rationalize, 0));
        rb_ary_push(time, rb_funcall(rb_funcall(val, egt_id_sec, 0), egt_id_rationalize, 0));
        rb_hash_aset(values, egt_sym_time_stamp, time);
    }

#define CONVERT_COORDINATES(from, to, ref, negative, positive)                                                         \
    val = rb_hash_aref(values, from);                                                                                  \
    if (val != Qnil) {                                                                                                 \
        VALUE deg, min, sec, tmp;                                                                                      \
                                                                                                                       \
        if (!rb_obj_is_kind_of(val, rb_cNumeric)) {                                                                    \
            rb_raise(rb_eTypeError, "wrong argument (%" PRIsVALUE ")! (Expected kind of %" PRIsVALUE ")",              \
                     rb_obj_class(val), rb_cNumeric);                                                                  \
        }                                                                                                              \
        deg = rb_funcall(val, egt_id_truncate, 0);                                                                     \
        min = rb_funcall(rb_funcall(rb_funcall(val, egt_id_sub, 1, deg), egt_id_mul, 1, egt_flt_min), egt_id_truncate, \
                         0);                                                                                           \
        tmp = rb_funcall(rb_funcall(rb_funcall(val, egt_id_sub, 1, deg), egt_id_sub, 1,                                \
                                    rb_funcall(min, egt_id_div, 1, egt_flt_min)),                                      \
                         egt_id_mul, 1, egt_flt_sec);                                                                  \
        sec = rb_funcall(tmp, egt_id_round, 1, INT2FIX(3));                                                            \
        rb_hash_aset(values, to, rb_ary_new_from_args(3, rb_funcall(deg, egt_id_rationalize, 0),                       \
                                                      rb_funcall(min, egt_id_rationalize, 0),                          \
                                                      rb_funcall(sec, egt_id_rationalize, 0)));                        \
        if (RTEST(rb_funcall(val, egt_id_negative_p, 0))) {                                                            \
            rb_hash_aset(values, ref, negative);                                                                       \
        } else {                                                                                                       \
            rb_hash_aset(values, ref, positive);                                                                       \
        }                                                                                                              \
    }

    CONVERT_COORDINATES(egt_sym__latitude, egt_sym_latitude, egt_sym_latitude_ref, egt_str_south, egt_str_north);
    CONVERT_COORDINATES(egt_sym__longitude, egt_sym_longitude, egt_sym_longitude_ref, egt_str_west, egt_str_east);
#undef CONVERT_COORDINATES
}

static void egt_handle_tag(ExifMem *mem, ExifData *exif_data, ExifTag tag, VALUE prev_values, VALUE new_values, ID key)
{
    ExifEntry *exif_entry;
    VALUE val;

    if (rb_hash_aref(new_values, key) != Qnil) {
        exif_entry = exif_content_get_entry(exif_data->ifd[EXIF_IFD_GPS], tag);
        if (!exif_entry) {
            exif_entry = exif_entry_new();
            exif_entry->tag = tag;
            exif_content_add_entry(exif_data->ifd[EXIF_IFD_GPS], exif_entry);
            egt_exif_entry_initialize(mem, exif_entry, tag);
            /* the entry has been added to the IFD, so we can unref it */
            exif_entry_unref(exif_entry);
        } else {
            val = egt_exif_entry_get_value(exif_entry);
            rb_hash_aset(prev_values, key, val);
        }

        egt_exif_entry_set_value(mem, exif_entry, new_values, key);
    }
}

static ExifData *egt_exif_data_new_from_file(ExifMem *mem, const char *path)
{
    ExifData *edata;
    ExifLoader *loader;

    loader = exif_loader_new_mem(mem);
    exif_loader_log(loader, logger);
    exif_loader_write_file(loader, path);
    edata = exif_loader_get_data(loader);
    exif_loader_unref(loader);

    return (edata);
}

static void egt_save_exif_to_file(ExifData *exif_data, char *file_path)
{
    JPEGData *jdata;
    unsigned char *exif_blob = NULL;
    unsigned int exif_blob_len;

    /* Parse the JPEG file. */
    jdata = jpeg_data_new();
    jpeg_data_log(jdata, logger);
    jpeg_data_load_file(jdata, file_path);

    /* Make sure the EXIF data is not too big. */
    exif_data_save_data(exif_data, &exif_blob, &exif_blob_len);
    if (exif_blob_len) {
        free(exif_blob);
        if (exif_blob_len > 0xffff) {
            rb_raise(rb_eArgError, "too much EXIF data (%i bytes). Only %i bytes are allowed.", exif_blob_len, 0xffff);
        }
    };

    jpeg_data_set_exif_data(jdata, exif_data);

    if (!jpeg_data_save_file(jdata, file_path)) {
        exif_log(logger, EXIF_LOG_CODE_DEBUG, "RubyExt", "failed to write updated EXIF to %s", file_path);
    }
    jpeg_data_unref(jdata);
}

static VALUE egt_write_tag(VALUE self, VALUE file_path, VALUE new_values)
{
    ExifData *exif_data;
    ExifMem *mem;
    VALUE prev_values;
    (void)self;

    Check_Type(file_path, T_STRING);
    Check_Type(new_values, T_HASH);

    mem = exif_mem_new_default();
    exif_data = egt_exif_data_new_from_file(mem, RSTRING_PTR(file_path));
    if (!exif_data) {
        rb_raise(rb_eArgError, "file not readable or no EXIF data in file");
    }

    prev_values = rb_hash_new();

    egt_parse_virtual_fields(new_values);
#define X(e, i) egt_handle_tag(mem, exif_data, e, prev_values, new_values, egt_sym_##i);
    TAG_MAPPING(X)
#undef X
    egt_generate_virtual_fields(prev_values);

    if (rb_hash_size(new_values) > 0) {
        egt_save_exif_to_file(exif_data, RSTRING_PTR(file_path));
    }

    exif_data_free(exif_data);
    exif_mem_unref(mem);
    return prev_values;
}

#ifdef DEBUG
/* ANSI escape codes for output colors */
#define COL_BLUE "\033[34m"
#define COL_GREEN "\033[32m"
#define COL_RED "\033[31m"
#define COL_BOLD "\033[1m"
#define COL_UNDERLINE "\033[4m"
#define COL_NORMAL "\033[m"

#define put_colorstring(file, colorstring)                                                                             \
    do {                                                                                                               \
        if (isatty(fileno(file))) {                                                                                    \
            fputs(colorstring, file);                                                                                  \
        }                                                                                                              \
    } while (0)

static void log_func(ExifLog *log, ExifLogCode code, const char *domain, const char *format, va_list args, void *data)
{
    (void)log;
    (void)data;

    switch (code) {
    case EXIF_LOG_CODE_DEBUG:
        put_colorstring(stdout, COL_GREEN);
        fprintf(stdout, "%s: ", domain);
        vfprintf(stdout, format, args);
        put_colorstring(stdout, COL_NORMAL);
        printf("\n");
        break;
    case EXIF_LOG_CODE_CORRUPT_DATA:
    case EXIF_LOG_CODE_NO_MEMORY:
        put_colorstring(stderr, COL_RED COL_BOLD COL_UNDERLINE);
        fprintf(stderr, "%s\n", exif_log_code_get_title(code));
        put_colorstring(stderr, COL_NORMAL COL_RED);
        fprintf(stderr, "%s\n", exif_log_code_get_message(code));
        fprintf(stderr, "%s: ", domain);
        vfprintf(stderr, format, args);
        put_colorstring(stderr, COL_NORMAL);
        fprintf(stderr, "\n");

        /*
         * EXIF_LOG_CODE_NO_MEMORY is always a fatal error, so exit.
         * EXIF_LOG_CODE_CORRUPT_DATA is only fatal if debug mode
         * is off.
         *
         * Exiting the program due to a log message is really a bad
         * idea to begin with. This should be removed once the libexif
         * API is fixed to properly return error codes everywhere.
         */
        if ((code == EXIF_LOG_CODE_NO_MEMORY))
            exit(1);
        break;
    default:
        put_colorstring(stdout, COL_BLUE);
        printf("%s: ", domain);
        vprintf(format, args);
        put_colorstring(stdout, COL_NORMAL);
        printf("\n");
        break;
    }
}
#endif

void Init_exif_geo_tag_ext(void)
{
    VALUE interned;

    egt_mExifGeoTag = rb_define_module("ExifGeoTag");

    rb_define_singleton_method(egt_mExifGeoTag, "write_tag", egt_write_tag, 2);

#define X(e, i) egt_sym_##i = ID2SYM(rb_intern(#i));
    TAG_MAPPING(X)
#undef X

    egt_sym__latitude = ID2SYM(rb_intern("_latitude"));
    egt_sym__longitude = ID2SYM(rb_intern("_longitude"));
    egt_sym__altitude = ID2SYM(rb_intern("_altitude"));
    egt_sym__timestamp = ID2SYM(rb_intern("_timestamp"));

    egt_id_Rational = rb_intern("Rational");
    egt_id_add = rb_intern("+");
    egt_id_div = rb_intern("/");
    egt_id_sub = rb_intern("-");
    egt_id_mul = rb_intern("*");
    egt_id_to_f = rb_intern("to_f");
    egt_id_to_i = rb_intern("to_i");
    egt_id_rationalize = rb_intern("rationalize");
    egt_id_round = rb_intern("round");
    egt_id_split = rb_intern("split");
    egt_id_utc = rb_intern("utc");
    egt_id_numerator = rb_intern("numerator");
    egt_id_denominator = rb_intern("denominator");
    egt_id_strftime = rb_intern("strftime");
    egt_id_hour = rb_intern("hour");
    egt_id_min = rb_intern("min");
    egt_id_sec = rb_intern("sec");
    egt_id_truncate = rb_intern("truncate");
    egt_id_negative_p = rb_intern("negative?");

    interned = rb_ary_new();
    rb_const_set(egt_mExifGeoTag, rb_intern("_INTERNED"), interned);
    egt_str_colon = rb_str_freeze(rb_str_new_cstr(":"));
    rb_ary_push(interned, egt_str_colon);
    egt_str_period = rb_str_freeze(rb_str_new_cstr("."));
    rb_ary_push(interned, egt_str_period);
    egt_str_date_format = rb_str_freeze(rb_str_new_cstr("%Y:%m:%d"));
    rb_ary_push(interned, egt_str_date_format);
    egt_str_south = rb_str_freeze(rb_str_new_cstr("S"));
    rb_ary_push(interned, egt_str_south);
    egt_str_north = rb_str_freeze(rb_str_new_cstr("N"));
    rb_ary_push(interned, egt_str_north);
    egt_str_west = rb_str_freeze(rb_str_new_cstr("W"));
    rb_ary_push(interned, egt_str_west);
    egt_str_east = rb_str_freeze(rb_str_new_cstr("E"));
    rb_ary_push(interned, egt_str_east);

    egt_flt_min = rb_float_new(60);
    egt_flt_sec = rb_float_new(3600);

#ifdef DEBUG
    logger = exif_log_new();
    exif_log_set_func(logger, log_func, NULL);
#else
    logger = NULL;
#endif
}
