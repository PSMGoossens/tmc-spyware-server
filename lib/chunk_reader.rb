module ChunkReader
  extend self

  def read_chunks(path, offset, length, options = {}, &block)
    options = {
      chunk_size: 16 * 1024
    }.merge(options)

    File.open(path, "rb") do |f|
      f.sysseek(offset, IO::SEEK_SET)
      amt_read = 0
      begin
        while amt_read < length
          amt_to_read = [options[:chunk_size], length - amt_read].min
          data = f.sysread(amt_to_read)
          amt_read += data.bytesize
          block.call(data)
        end
      rescue EOFError
        # loop done
      end
    end
  end
end
