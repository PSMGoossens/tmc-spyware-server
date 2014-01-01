 
# TMC spyware server #

This CGI program receives data from
[TMC's NetBeans plugin](https://github.com/testmycode/tmc-netbeans)
about how the student works.

The University of Helsinki uses this software to gather data for research
purposes. The data will not be used for grading nor will any excerpts of it
be published without thorough anonymization.

## Setup ##

Dependencies: `libcurl-dev`.

Configure by writing a `config.h` that defines the stuff mentioned in `config-defaults.h`.
Then compile the program with `make`.

## Running tests

Note: tests must be run with an empty `config.h`!

    rm -f config.h
    bundle install
    cd test
    ./test-cgi.rb

## File format ##

Each course has a directory with two files for each student.

- The *data file*, named `<username>.dat`, contains a sequence
  of data records, the format of which is free-form.
- The *index file*, named `<username>.idx` is a text file where each line
  starts with two ascii integers: the absolute offset of the record
  and the length of the record. The record offsets must be in ascending order.

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
Checksums may be added later to address these.

## License ##

[GPLv2](http://www.gnu.org/licenses/gpl-2.0.html)
