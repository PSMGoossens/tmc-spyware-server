#!/usr/bin/env ruby

$LOAD_PATH << File.dirname(File.realpath(__FILE__)) + '/lib'

require 'file_pair_reader'
require 'digest/sha1'

if ARGV.include?('-h') || ARGV.include?('--help') || ARGV.empty?
  puts "Verifies the checksums of recorded data."
  puts "Usage: #{$0} index-file.idx [...]"
  exit(0)
end

index_files = []
until ARGV.empty?
  arg = ARGV.shift
  if arg.end_with?('.idx')
    index_files << arg
  else
    raise "Unrecognized file extension for #{arg}"
  end
end

record_count = 0
no_checksum_count = 0
error_count = 0

index_files.each do |file|
  puts "Verifying #{file}"
  FilePairReader.open(file) do |reader|
    reader.each_record do |record|
      begin
        if record.sha1_checksum
          record_count += 1
          record.verify_checksum!
        else
          no_checksum_count += 1
        end
      rescue Record::ChecksumError => e
        puts e.message
        error_count += 1
      end
    end
  end
end

puts "#{record_count} records verified in #{index_files.size} files."
if no_checksum_count > 0
  puts "#{no_checksum_count} records had no checksum."
end
if error_count == 0
  puts "All OK."
  exit 0
else
  puts "*** #{error_count} errors!"
  exit 1
end
