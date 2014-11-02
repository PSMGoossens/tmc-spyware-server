require 'active_support/json'
require 'digest/sha1'
require 'record'

# A reader for an index + data file pair.
class FilePairReader
  def initialize(index_path)
    @index_path = index_path
    @data_path = index_path[0...-4] + '.dat'
  end

  def each(&block)
    file = File.open(@index_path, "rb")
    begin
      file.flock(File::LOCK_EX)

      file.each_line do |line|
        record = parse_index_line(line)
        block.call(record)
      end

      file.flock(File::LOCK_UN)
    ensure
      file.close
    end
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
