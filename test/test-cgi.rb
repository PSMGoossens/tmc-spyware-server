#!/usr/bin/env ruby

gem 'minitest'
require 'socket' # work around https://github.com/lukeredpath/mimic/pull/12 
require 'minitest/autorun'
require 'mimic'

# This test assumes the default configuration

class TestCgi < Minitest::Test
  def setup
    @test_dir = File.realpath(File.dirname(__FILE__))
    @project_dir = File.realpath(@test_dir + '/../')
    @program = @project_dir + '/cgi-bin/tmc-spyware-server-cgi'
    @test_data_dir = @test_dir + '/data'
    @test_log_file = @test_dir + '/tmc-spyware-server.log'

    @auth_port = 3038

    FileUtils.rm_rf(@test_data_dir)
    FileUtils.rm_f(@test_log_file)

    @basic_env = {
      "TMC_SPYWARE_DATA_DIR" => @test_data_dir,
      "TMC_SPYWARE_AUTH_URL" => "http://localhost:#{@auth_port}/auth.text",
      "REQUEST_METHOD" => "POST",
      "QUERY_STRING" => "username=theuser&password=thepass",
      "REMOTE_ADDR" => "127.0.0.1"
    }

    Mimic.mimic(:port => @auth_port) do
      get("/auth.text") do
        if params["username"] == "theuser" && params["password"] == "thepass"
          [200, {}, "OK"]
        else
          [200, {}, "FAIL"]
        end
      end
    end
  end

  def test_three_sequential_operations
    in1 = "foo\nbar"
    in2 = "asd" * 50000
    in3 = "baz"
    env = @basic_env

    out1 = run_cgi!(in1, env)
    out2 = run_cgi!(in2, env)
    out3 = run_cgi!(in3, env)

    [out1, out2, out3].each do |out|
      assert_starts_with("Status: 200 OK\n", out)
    end

    (index, data) = read_data

    sz1 = in1.size
    sz2 = sz1 + in2.size
    expected_index = [
      [0, sz1],
      [sz1, in2.size],
      [sz2, in3.size]
    ].map {|x| x.join(" ") }.join("\n") + "\n"
    assert_equal(expected_index, index)

    expected_data = in1 + in2 + in3
    assert_equal(expected_data, data)

    log = read_log
    assert(log.include?(" 200 127.0.0.1"))
  end

  def test_providing_content_length
    input = "1234567890" * 10000
    env = @basic_env.merge("CONTENT_LENGTH" => "5")

    out = run_cgi!(input, env)
    assert_starts_with("Status: 200 OK\n", out)

    (index, data) = read_data

    assert_equal("0 5\n", index)
    assert_equal("12345", data)
  end

  def test_providing_content_length_and_much_input
    input = "1234567890" * 10000
    env = @basic_env.merge("CONTENT_LENGTH" => "50000")

    out = run_cgi!(input, env)
    assert_starts_with("Status: 200 OK\n", out)

    (index, data) = read_data

    assert_equal("0 50000\n", index)
    assert_equal(input[0...50000], data)
  end

  def test_error_on_short_input
    input = "123"
    env = @basic_env.merge("CONTENT_LENGTH" => "5")

    out = run_cgi!(input, env)
    assert_starts_with("Status: 500 Internal Server Error\n", out)

    log = read_log
    assert(log.include?("Input was 2 bytes shorter than expected.\n"))
    assert(log.include?(" 500 127.0.0.1"))
    assert(!log.include?(" 200 127.0.0.1"))
  end

  private

  def run_cgi!(stdin, envvars)
    output = run_cgi(stdin, envvars)
    raise "Failed: #{$?}" if !$?.success?
    output
  end

  def run_cgi(stdin, envvars)
    IO.popen([envvars, @program, :chdir => @test_dir, :err => @test_log_file], "r+") do |io|
      begin
        io.print stdin
      rescue Errno::EPIPE
        # ignore - some tests expect the program not to read all of stdin
      end
      io.close_write
      io.read
    end
  end

  def read_data(user = 'theuser')
    index_file = "#{@test_data_dir}/#{user}.idx"
    data_file = "#{@test_data_dir}/#{user}.dat"
    [File.read(index_file), File.read(data_file)]
  end

  def read_log
    File.read(@test_log_file)
  end

  def assert_starts_with(expected, actual)
    assert_equal(expected, actual[0...(expected.length)])
  end
end
