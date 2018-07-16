require 'exif_geo_tag/version'
require 'exif_geo_tag_ext'

module ExifGeoTag
end

unless 42.respond_to?(:negative?)
  class Numeric
    def negative?
      self < 0
    end
  end
end

unless 4.2.respond_to?(:negative?)
  class Float
    def negative?
      self < 0.0
    end
  end
end
