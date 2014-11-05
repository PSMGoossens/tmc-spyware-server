#!/usr/bin/env ruby

$LOAD_PATH << File.dirname(File.realpath(__FILE__)) + '/lib'

require 'file_pair_reader'
require 'lftp'
require 'thread/pool'
require 'tmpdir'

if ARGV.include?('-h') || ARGV.include?('--help') || ARGV.empty?
  puts "Copies index and data files from a remote server to a local mirror."
  puts "Only copies appended data. Assumes all existing data is identical."
  puts "It is advisable to run verify.rb on both copies occasionally."
  puts
  puts "Usage: #{$0} [--threads=n] <remote> <local>"
  puts "Where <remote> is an URL accepted by lftp and <local> is a local directory."
  exit(0)
end

remote = nil
local = nil
num_threads = 1

until ARGV.empty?
  arg = ARGV.shift
  if arg =~ /--threads=(.*)/
    num_threads = $1.to_i
  elsif remote == nil
    remote = arg
  elsif local == nil
    local = arg
  else
    raise "Too many arguments"
  end
end

index_files = LFTP.new(remote, local).list_directory('.').select {|f| f.end_with?('.idx') }


thread_pool = Thread.pool(num_threads)
errors = 0

index_files.each do |index_file|
  thread_pool.process do
    begin
      lftp = LFTP.new(remote, local)

      data_file = FilePairReader.index_path_to_data_path(index_file)
      local_index_file = File.join(local, index_file)
      local_data_file = File.join(local, data_file)

      # The server writes to the index file only after all data has been written,
      # so if we read the index file first, the data we will read will cover at least
      # all records in our index file. We accept that we may get some some extra data
      # at the end.
      Thread.exclusive { puts "Syncing #{index_file}" }
      lftp.continue_copying_remote_file(index_file, local_index_file)
      Thread.exclusive { puts "Syncing #{data_file}" }
      lftp.continue_copying_remote_file(data_file, local_data_file)
    rescue Exception => e
      Thread.exclusive {
        errors += 1
        puts "#{e.class}: #{e.message}\n  #{e.backtrace.join("\n  ")}"
      }
    end
  end
end

thread_pool.shutdown

if errors == 0
  puts "Sync successful"
  exit(0)
else
  puts "*** Sync errors: #{errors}"
  exit(1)
end
