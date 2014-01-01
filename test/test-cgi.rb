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
    @program = @project_dir + '/tmc-spyware-server-cgi'
    @test_data_dir = @test_dir + '/data'

    FileUtils.rm_rf(@test_data_dir)
  end

  def test_three_sequential_operations
    in1 = "foo\nbar"
    in2 = "asd" * 10000
    in3 = "baz"
    env = {
      "REQUEST_METHOD" => "POST",
      "QUERY_STRING" => "username=theuser&password=thepass&course_name=the-course"
    }

    Mimic.mimic(:port => 3000) do
      get("/auth.text") do
        if params["username"] == "theuser" && params["password"] == "thepass"
          [200, {}, "OK"]
        else
          [200, {}, "FAIL"]
        end
      end
    end

    out1 = run_cgi!(in1, env)
    out2 = run_cgi!(in2, env)
    out3 = run_cgi!(in3, env)

    [out1, out2, out3].each do |out|
      assert_equal("Status: 200 OK\n", out)
    end

    (index, data) = read_data('the-course', 'theuser')

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
  end

  private

  def run_cgi!(stdin, envvars)
    output = run_cgi(stdin, envvars)
    raise "Failed: #{$?}" if !$?.success?
    output
  end

  def run_cgi(stdin, envvars)
    IO.popen([envvars, @program,  :chdir => @test_dir], "r+") do |io|
      io.print stdin
      io.close_write
      io.read
    end
  end

  def read_data(course, user)
    index_file = "#{@test_data_dir}/#{course}/#{user}.idx"
    data_file = "#{@test_data_dir}/#{course}/#{user}.dat"
    [File.read(index_file), File.read(data_file)]
  end
end
