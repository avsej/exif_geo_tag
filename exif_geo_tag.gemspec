# coding: utf-8

lib = File.expand_path('../lib', __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)
require 'exif/version'

Gem::Specification.new do |spec|
  spec.name          = 'exif_geo_tag'
  spec.version       = ExifGeoTag::VERSION
  spec.authors       = 'Sergey Avseyev'
  spec.email         = 'sergey.avseyev@gmail.com'
  spec.summary       = 'Ruby EXIF geo tagger.'
  spec.description   = 'Ruby EXIF geo tagger based on libexif.'
  spec.homepage      = 'https://github.com/avsej/exif_geo_tag'
  spec.license       = 'MIT'
  spec.files         = Dir['lib/**/*.rb', 'ext/**/*.{h,c}']
  spec.extensions    = ['ext/extconf.rb']
  spec.add_development_dependency 'rake', '~> 10.0'
  spec.add_development_dependency 'rake-compiler'
end
