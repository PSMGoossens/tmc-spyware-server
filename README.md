 
# TMC spyware server #

[![Build Status](https://travis-ci.org/testmycode/tmc-spyware-server.svg?branch=travis)](https://travis-ci.org/testmycode/tmc-spyware-server)

This CGI program receives data from
[TMC's NetBeans plugin](https://github.com/testmycode/tmc-netbeans)
about how the student works.

The University of Helsinki uses this software to gather data for research
purposes. The data will not be used for grading nor will any excerpts of it
be published without thorough anonymization.

## Setup ##

Dependencies: `libcurl-dev`.

Compile with `make`.

The program is configured via envvars. Here is a configuration example for Apache 2.4:

    <VirtualHost *:80>
        ServerName spyware.example.com

        DocumentRoot /path/to/cgi-bin

        RewriteEngine on
        RewriteRule ^.*$ /tmc-spyware-server-cgi [L]
        <Directory />
            SetEnv TMC_SPYWARE_AUTH_URL "http://localhost:3000/auth.text"
            SetEnv TMC_SPYWARE_DATA_DIR "/path/to/data"
            Require all granted
            Options ExecCGI
            SetHandler cgi-script
        </Directory>
    </VirtualHost>

## Running tests ##

Note: tests must be run with an empty `config.h`!

    mv config.h config.h.bak
    bundle install
    test/test-cgi.rb

## Protocol ##

Accepts a POST request of raw data with the following headers:

    X-Tmc-Version: 1
    X-Tmc-Username: username
    X-Tmc-Password: password
    X-Tmc-Session-Id: session-id   (as alternative to password)

(Note: these are passed to the CGI-script through envvars, which may be readable by local users.
This is a security issue if the server has local users that cannot be fully trusted.)

## File format ##

Each student has two files:

- The *data file*, named `<username>.dat`, contains a sequence
  of data records, the format of which is free-form.
- The *index file*, named `<username>.idx` is a text file where each line
  starts with two ascii integers: the absolute offset of the record
  and the length of the record. The record offsets must be in ascending order.
  The line may continue with additional metadata.

Writes to the index and data file are protected by an exclusive `flock`
on the index file. The writing protocol is as follows:

- The index file is locked.
- The data is written to the data file.
- The data file is `fdatasync()`ed.
- The index entry is appended to the index (and the index `fdatasync()`ed).
- The index file lock is released.

If a writer crashes, the data file will contain data that is not
part of any indexed record. This is an acceptable and hopefully rare event.

Non-battery-backed write caches and faulty disks remain a concern.
This is addressed by checksums in the index files. Use `verify.rb` to verify them.

The server can be load-balanced easily since the data files are easy to merge later.

## data-reader.rb ##

`data-reader.rb` is a script that reads through an index+data file.
It prints rudimentary statistics about the file optionally copies the data to an sqlite3 file for easier analysis.

Example:

    ruby data-reader.rb --sqlite_out=data.sqlite3 username.idx
    # Now we can e.g. query the largest code snapshot on a specific course:
    echo "SELECT data FROM events WHERE courseName LIKE '%somecourse%' AND eventType = 'code_snapshot' ORDER BY length(data) DESC LIMIT 1;" | sqlite3 data.sqlite3 | base64 --decode > tmp.zip

## License ##

[GPLv2](http://www.gnu.org/licenses/gpl-2.0.html)
