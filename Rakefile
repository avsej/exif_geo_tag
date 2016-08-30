require "rake/extensiontask"
require 'rspec/core/rake_task'

Rake::ExtensionTask.new("exif_geo_tag_ext") do |ext|
  ext.ext_dir = "ext"
end

desc 'Start an irb session and load the library.'
task :console do
  exec "irb -I lib -rexif_geo_tag"
end

task default: [:compile, :console]
