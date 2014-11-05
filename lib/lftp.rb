
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
    lines = output.split("\n")
    lines.map do |line|
      line.sub(/^\.*\/?/, '').sub(/\/$/, '')
    end.reject(&:empty?)
  end

  def cat(path)
    do_commands(['cat', path])
  end

  def continue_copying_remote_file(path, local_path)
    do_commands(['get1', '-c', '-o', local_path, path])
  end

  def copy_remote_file(path, local_path, options = {})
    options = {
      :remote_range => nil,
      :local_offset => nil
    }.merge(options)
    command = ['get1', '-o', local_path]
    if options[:remote_range]
      from = options[:remote_range].first
      to = options[:remote_range].last
      command << "--source-region=#{from}-#{to}"
    end
    if options[:local_offset]
      command << "--target-position=#{pos}"
    end
    command << path
    do_commands(command)
  end

  private

  def do_commands(*commands)
    commands = commands.clone
    commands.unshift(['lcd', @local])
    commands.unshift(['open', @remote])
    commands.map! {|command| command.map {|word| quote_file_name(word) }.join(' ') }

    shell_command = Shellwords.join ['lftp', '-vvv', '-c', commands.join(' ; ')]
    output = `#{shell_command}`
    raise "LFTP command failed." unless $?.success?
    output
  end

  def quote_file_name(name)
    # In the 2nd gsub, the extra backslash escapes the special backreference meaning of "\'".
    "'" + name.gsub("\\", "\\\\").gsub("'", "\\\\'") + "'"
  end
end
