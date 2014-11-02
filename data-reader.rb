#!/usr/bin/env ruby

# TODO: refactor this to use the stuff in lib/

require 'json'
require 'zlib'
require 'stringio'
require 'base64'
require 'sqlite3'

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

class SqliteWriter
  def initialize(sqlite_file)
    @db = SQLite3::Database.new(sqlite_file)
    schema = "CREATE TABLE IF NOT EXISTS events ("
    schema += fields.map {|name, type| "#{name} #{type}" }.join(",")
    schema += ");"
    @db.execute schema
  end

  def fields
    {
      :eventType => 'TEXT',
      :courseName => 'TEXT',
      :exerciseName => 'TEXT',
      :data => 'BLOB'
      # TODO: more
    }
  end

  def begin_transaction
    @db.transaction
  end

  def commit
    @db.commit
  end

  def write_record(record)
    sql = "INSERT INTO events ("
    sql += fields.keys.map(&:to_s).join(',')
    sql += ") VALUES ("
    sql += (['?'] * fields.size).join(',')
    sql += ");"
    values = fields.keys.map {|key| record[key.to_s].to_s }
    @db.execute(sql, values)
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
  puts "Usage: #{$0} [--sqlite_out=file] data-or-index-files..."
  exit!
end

file_pairs = []
sql_file = nil

while !ARGV.empty?
  arg = ARGV.shift
  case arg
  when /--sqlite_out=(.*)/
    sql_file = $1
  else
    if arg.end_with?('.idx')
      index_file = arg
      data_file = arg[0...-4] + '.dat'
    elsif arg.end_with?('.dat')
      index_file = arg[0...-4] + '.idx'
      data_file = arg
    else
      raise "Unrecognized file extension for #{arg}"
    end
    file_pairs << [index_file, data_file]
  end
end

if sql_file
  sql_writer = SqliteWriter.new(sql_file)
  sql_writer.begin_transaction
end

total_records = 0
total_records_by_type = {}
total_records_by_type.default = 0
total_size_by_record_type = {}
total_size_by_record_type.default = 0
total_snapshot_size_by_course = {}
total_snapshot_size_by_course.default = 0

file_pairs.each do |index_file, data_file|
  DataFilePair.new(index_file, data_file).each_record do |record|
    if sql_writer
      sql_writer.write_record(record)
    end

    total_records += 1
    if record['eventType']
      total_records_by_type[record['eventType']] += 1
      total_size_by_record_type[record['eventType']] += record['data'].size if record['data']
    end

    if record['eventType'] == 'code_snapshot' && record['courseName'] && record['data']
      total_snapshot_size_by_course[record['courseName']] += record['data'].size
    end
  end
end

if sql_writer
  sql_writer.commit
  puts "SQL file #{sql_file} committed."
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

puts "Total 'data' field size in code_snapshot records: #{human_bytes(total_snapshot_size_by_course.values.reduce(0, &:+))}"
total_snapshot_size_by_course.keys.sort.each do |course|
  puts "  #{course}: #{human_bytes(total_snapshot_size_by_course[course])}"
end
