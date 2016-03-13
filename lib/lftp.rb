
require 'tempfile'
require 'shellwords'

# Wrapper for the lftp command
class LFTP
  def initialize(remote, local)
    @remote = remote
    @local = local
    raise "The lftp command is not installed." if `which lftp`.empty?
  end

  def list_directory(path)
    output = do_commands(['cd', path], ['find', '-d', '2'])
    output.force_encoding('ASCII-8BIT')
    lines = output.split("\n")
    lines.map do |line|
      line.sub(/^\.*\/?/, '').sub(/\/$/, '')
    end.reject(&:empty?)
  end

  def cat(path)
    do_commands(['cat', path])
  end

  def continue_copying_remote_file(path, local_path)
    do_commands(['get1', '-c', '-o', local_path, '--', path])
  end

  private

  def do_commands(*commands)
    Tempfile.open('lftp-setderr') do |stderr_file|
      commands = commands.clone
      commands.unshift(['lcd', @local])
      commands.unshift(['open', @remote])
      commands.map! {|command| command.map {|word| quote_file_name(word) }.join(' ') }

      shell_command = Shellwords.join ['lftp', '-vvv', '-c', commands.join(' ; ')]
      output = `#{shell_command} 2> #{Shellwords.escape(stderr_file.path)}`
      status = $?
      unless status.success?
        begin
          stderr = File.read(stderr_file)
        rescue
          stderr = "<failed to read temp LFTP stderr file>"
        end
        raise "LFTP command failed with #{status}. stderr:\n#{stderr}"
      end
      output
    end
  end

  def quote_file_name(name)
    # In the 2nd gsub, the extra backslash escapes the special backreference meaning of "\'".
    "'" + name.gsub("\\", "\\\\").gsub("'", "\\\\'") + "'"
  end
end
