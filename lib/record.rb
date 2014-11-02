require 'chunk_reader'

class Record
  class ChecksumError < StandardError
  end

  def initialize(index_path, data_path, offset, length, metadata)
    @index_path = index_path
    @data_path = data_path
    @offset = offset
    @length = length
    @metadata = metadata
  end

  attr_accessor :index_path, :data_path, :offset, :length, :metadata

  def stream_data(&block)
    ChunkReader.read_chunks(@data_path, @offset, @length, &block)
  end

  def sha1_checksum
    @metadata['sha1']
  end

  def sha1_checksum=(new_checksum)
    @metadata['sha1'] = new_checksum
  end

  def verify_checksum
    expected = sha1_checksum
    if expected
      actual = calculate_data_checksum
      if actual != expected
        raise ChecksumError.new("#{self} checksum mismatch. Expected #{expected}, was #{actual}.")
      end
    end
  end

  def calculate_data_checksum
    digest = Digest::SHA1.new
    stream_data do |chunk|
      digest.update(chunk)
    end
    digest.hexdigest
  end

  def to_index_line
    metadata_json = ActiveSupport::JSON.encode(@metadata)
    raise "Invalid metadata - has linebreak" if metadata_json.include?("\n")
    "#{@offset}, #{@length} #{metadata_json}"
  end

  def to_s
    "(Record: #{@index_path} #{@offset} #{@length} #{@metadata})"
  end
end
