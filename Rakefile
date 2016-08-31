require 'rake/extensiontask'
require 'rubygems/package_task'

def gemspec
  @clean_gemspec ||= begin
                       eval(File.read(File.expand_path('../exif_geo_tag.gemspec', __FILE__)))
                     end
end

Gem::PackageTask.new(gemspec) do |pkg|
  pkg.need_tar = pkg.need_zip = false
end

Rake::ExtensionTask.new('exif_geo_tag_ext') do |ext|
  ext.ext_dir = 'ext'
end

desc 'Start an irb session and load the library.'
task :console do
  exec 'irb -I lib -rexif_geo_tag'
end

task default: [:compile, :console]
