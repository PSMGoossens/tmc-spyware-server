#!/usr/bin/env ruby

require 'json'
require 'zlib'
require 'stringio'
require 'base64'

class DataFilePair
  def initialize(index_file, data_file)
    @index_file = index_file
    @data_file = data_file
  end

  def each_record(&block)
    readbuf = ''
    File.open(@data_file, "rb") do |f|
      File.readlines(@index_file).map do |index_line|
        if index_line =~ /^(\d+) (\d+)(?: (.*))?$/
          offset = $1.to_i
          length = $2.to_i
          metadata = $3

          readbuf.force_encoding('ASCII-8BIT')
          f.seek(offset)
          f.read(length, readbuf)

          if readbuf.length < length
            $stderr.puts "Not enough data in data file for index: #{index_line}"
            next
          end

          unzipped = StringIO.open(readbuf) do |sf|
            zsf = Zlib::GzipReader.new(sf)
            begin
              zsf.read
            ensure
              zsf.close
            end
          end
          unzipped.force_encoding('UTF-8')

          begin
            record_array = JSON.parse(unzipped)
          rescue JSON::ParserError
            $stderr.puts "Failed to parse JSON for index: #{index_line}"
            next
          end

          record_array.each do |record|
            block.call(record)
          end

        else
          $stderr.puts "Invalid index line: #{index_line}"
          exit!
        end
      end
    end
  end
end

def human_bytes(amt)
  suffixes = ['kB', 'MB', 'GB', 'TB']
  suffix = 'B'
  amt = amt.to_f
  while amt > 1024 && !suffixes.empty?
    amt /= 1024
    suffix = suffixes.shift
  end
  sprintf("%.2f %s", amt, suffix)
end

if ARGV.include?('-h') || ARGV.include?('--help') || ARGV.empty?
  puts "Usage: #{$0} data-or-index-files..."
  exit!
end

file_pairs = ARGV.map do |input_file|
  if input_file.end_with?('.idx')
    index_file = input_file
    data_file = input_file[0...-4] + '.dat'
  elsif input_file.end_with?('.dat')
    index_file = input_file[0...-4] + '.idx'
    data_file = input_file
  else
    raise "Unrecognized file extension for #{input_file}"
  end
  [index_file, data_file]
end

total_records = 0
total_records_by_type = {}
total_records_by_type.default = 0
total_size_by_record_type = {}
total_size_by_record_type.default = 0

file_pairs.each do |index_file, data_file|
  DataFilePair.new(index_file, data_file).each_record do |record|
    total_records += 1
    if record['eventType']
      total_records_by_type[record['eventType']] += 1
      total_size_by_record_type[record['eventType']] += record['data'].size if record['data']
    end

#     if record['eventType'] == 'text_insert'
#       puts JSON.parse(Base64.decode64(record['data']))['file']
#       puts JSON.parse(Base64.decode64(record['data']))['patches']
#     end
  end
end

puts "Total records: #{total_records}"
total_records_by_type.keys.sort.each do |ty|
  puts "  #{ty}: #{total_records_by_type[ty]}"
end

puts "Total 'data' field size: #{human_bytes(total_size_by_record_type.values.reduce(0, &:+))}"
total_size_by_record_type.keys.sort.each do |ty|
  total_size = total_size_by_record_type[ty].to_f
  avg_size = total_size / total_records_by_type[ty].to_f
  puts "  #{ty}: #{human_bytes(total_size)}  (average size of record: #{human_bytes(avg_size)})"
end
