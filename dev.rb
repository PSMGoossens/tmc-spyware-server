#!/usr/bin/env ruby
# Runs the CGI script at http://localhost:3101/
# with datadir at thisdir/data, and log to stderr.

require 'webrick'
Dir.chdir(File.dirname($0))
project_dir = Dir.pwd

port = 3101
cgi_program = project_dir + '/cgi-bin/tmc-spyware-server-cgi'
data_dir = project_dir + '/data'
auth_url = 'http://localhost:3000/auth.text'

$extra_env_vars = {
  'TMC_SPYWARE_DATA_DIR' => data_dir,
  'TMC_SPYWARE_AUTH_URL' => auth_url
}

# We need terrible hax to smuggle our envvars through to WEBrick's CGI handler :(
module MyCGIHandlerBuilder
  def self.get_instance(server, *options)
    handler = WEBrick::HTTPServlet::CGIHandler.get_instance(server, *options)
    class << handler
      def do_GET(req, res)
        class << req
          def meta_vars
            mv = super
            mv.merge($extra_env_vars)
          end
        end
        super(req, res)
      end

      alias do_POST do_GET # Need to realias
    end
    handler
  end
end

server = WEBrick::HTTPServer.new(:Port => port, :DocumentRoot => 'cgi-bin', :AccessLog => [])
server.mount('/', MyCGIHandlerBuilder, File.expand_path(cgi_program))

trap("INT") do
  server.shutdown
end
server.start
