#!/usr/bin/env ruby

$LOAD_PATH << File.dirname(File.realpath(__FILE__)) + '/lib'

require 'file_pair_reader'
require 'digest/sha1'

if ARGV.include?('-h') || ARGV.include?('--help') || ARGV.empty?
  puts "Adds checksums to records that don't have them."
  puts "Usage: #{$0} [--dry-run] index-file.idx [...]"
  exit(0)
end

dry_run = false
index_files = []

until ARGV.empty?
  arg = ARGV.shift
  if arg == '--dry-run'
    dry_run = true
  elsif arg.end_with?('.idx')
    index_files << arg
  else
    raise "Unrecognized file extension for #{arg}"
  end
end


total_records = 0
missing_checksums = 0

index_files.each do |index_file|
  puts "Processing #{index_file}"
  FilePairReader.open(index_file) do |reader|
    records = []
    reader.each_record { |r| records << r }

    reader.rewind_and_truncate!

    records.each do |record|
      total_records += 1
      unless record.sha1_checksum
        missing_checksums += 1
        record.sha1_checksum = record.calculate_data_checksum
      end

      unless dry_run
        reader.write!(record)
      end
    end
  end
end

puts "Checksums added to #{missing_checksums} of #{total_records} records"
