require 'active_support/json'
require 'digest/sha1'
require 'record'

# A reader for an index + data file pair.
class FilePairReader
  def initialize(index_path)
    @index_path = index_path
    @data_path = self.class.index_path_to_data_path(index_path)
    @file = File.open(@index_path, "r+b")
    @file.flock(File::LOCK_EX)
  end

  def self.index_path_to_data_path(index_path)
    index_path[0...-4] + '.dat'
  end

  def self.open(index_path, &block)
    if block
      reader = FilePairReader.new(index_path)
      begin
        block.call(reader)
      ensure
        reader.close
      end
    else
      FilePairReader.new(index_path)
    end
  end

  def close
    begin
      @file.flock(File::LOCK_UN)
    ensure
      @file.close
    end
  end

  def each_record(&block)
    @file.each_line do |line|
      record = parse_index_line(line)
      block.call(record)
    end
  end

  def rewind
    @file.rewind
  end

  def write!(record)
    @file.puts(record.to_index_line)
  end

  private

  def parse_index_line(line)
    parts = line.split(' ', 3)
    raise "Invalid index line: #{line}" if parts.length != 3
    offset = parts[0].to_i
    length = parts[1].to_i
    metadata = ActiveSupport::JSON.decode(parts[2])
    Record.new(@index_path, @data_path, offset, length, metadata)
  end
end
