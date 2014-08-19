#!/usr/bin/env ruby
# encoding: UTF-8

gem 'minitest'
require 'json'
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
      "HTTP_X_TMC_VERSION" => "1",
      "HTTP_X_TMC_USERNAME" => "the user",
      "HTTP_X_TMC_PASSWORD" => "the+pass",
      "REMOTE_ADDR" => "127.0.0.1"
    }

    Mimic.mimic(:port => @auth_port) do
      get("/auth.text") do
        valid_users = ["the user", "pöllö", "user1", "user2", "user3"]
        if valid_users.include?(params["username"]) && (params["password"] == "the+pass" || params["session_id"] == "the session id")
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
    ]
    assert_equal(expected_index, index.map {|r| r[0..1] })

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

    assert_equal(0, index[0][0])
    assert_equal(5, index[0][1])
    assert_equal("12345", data)
  end

  def test_providing_content_length_and_much_input
    input = "1234567890" * 10000
    env = @basic_env.merge("CONTENT_LENGTH" => "50000")

    out = run_cgi!(input, env)
    assert_starts_with("Status: 200 OK\n", out)

    (index, data) = read_data

    assert_equal(0, index[0][0])
    assert_equal(50000, index[0][1])
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

  def test_error_if_username_has_slash
    env = @basic_env.merge("CONTENT_LENGTH" => "3", "HTTP_X_TMC_USERNAME" => "foo/bar")

    out = run_cgi!("asd", env)

    assert_starts_with("Status: 400 Bad Request\n", out)
  end

  def test_auth_with_session_id_instead_of_password
    env = @basic_env.clone
    env.delete("HTTP_X_TMC_PASSWORD")
    env["HTTP_X_TMC_SESSION_ID"] = "the session id"

    out = run_cgi!("asd", env)

    assert_starts_with("Status: 200 OK\n", out)
  end

  def test_auth_with_umlauted_username
    # Note that unescaped headers are always iso-8859-1
    env = @basic_env.merge("HTTP_X_TMC_USERNAME" => "pöllö".encode("ISO-8859-1"))
    out = run_cgi!("asd", env)
    assert_starts_with("Status: 200 OK\n", out)
  end

  def test_site_index_file
    outs = []
    env = @basic_env.merge("HTTP_X_TMC_USERNAME" => "user3")
    outs << run_cgi!("asd", env)
    outs << run_cgi!("asd", env)
    env = @basic_env.merge("HTTP_X_TMC_USERNAME" => "user2")
    outs << run_cgi!("asd", env)
    env = @basic_env.merge("HTTP_X_TMC_USERNAME" => "user1")
    outs << run_cgi!("asd", env)
    outs << run_cgi!("asd", env)

    outs.each do |out|
      assert_starts_with("Status: 200 OK\n", out)
    end

    expected_lines = ["user1\n", "user2\n", "user3\n"]
    assert_equal(expected_lines, File.readlines(site_index_file))
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

  def read_data(user = 'the user')
    index_file = "#{@test_data_dir}/#{user}.idx"
    data_file = "#{@test_data_dir}/#{user}.dat"
    [parse_index(File.read(index_file)), File.read(data_file)]
  end

  def site_index_file
    "#{@test_data_dir}/index.txt"
  end

  def read_log
    File.read(@test_log_file)
  end

  def assert_starts_with(expected, actual)
    assert_equal(expected, actual[0...(expected.length)])
  end

  def parse_index(index)
    index.lines.map do |line|
      if line =~ /^(\d+) (\d+) (.*)/
        [$1.to_i, $2.to_i, JSON.parse($3)]
      else
        raise "Invalid index line: '#{line}'"
      end
    end
  end
end
