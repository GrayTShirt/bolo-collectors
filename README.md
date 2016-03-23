bolo-collectors - Collectors for Bolo
=====================================

[![Travis](https://img.shields.io/travis/bolo/bolo-collectors.svg)](https://travis-ci.org/bolo/bolo-collectors)

[bolo][bolo] is a toolkit for building distributed, scalable
monitoring systems.

**bolo-collectors** is a set of utilities that gather raw
telemetry and format it for consumption by bolo utilities like
dbolo and send_bolo.

Getting Started
---------------

To compile the software, use the standard incantation:

    $ ./bootstrap
    $ ./configure
    $ make
    $ sudo make install

This will compile and install all of the bolo components in
standard systems places.  bolo-collectors requires libcurl, libpq,
librrd, libip4tc + libip6tc (iptables) and [libvigor][libvigor].

Collectors will be installed into /usr/local/lib/bolo/collectors.

The Collectors
--------------

The following collectors exist:

  1. **linux**    - Collect metrics from /proc about host health
  2. **hostinfo** - Reports host details as KEYs (FQDN, IP, etc.)
  3. **process**  - Gather data about a set of processes
  4. **postgres** - Run arbitrary queries against a PG database
  4. **mysql**    - Run arbitrary queryes against a MySQL database
  4. **netstat**  - Get network recv/send queue sizes for
                    arbitrary in-flight and listening connections.
  5. **files**    - Count files according to age, time, name, etc.
  6. **fw**       - Get hit counters (packets/bytes) from iptables
                    firewalls
  7. **cogd**     - Gather metrics about [clockwork][clockwork]
                    cogd runs (exec time, parse time, etc.)
  7. **httpd**    - Read scoreboard data from nginx
  8. **rrdcache** - Retrieve statistics from RRDCached
  9. **tcp**      - Connect to arbitrary TCP ports and record
                    response times (IPv4 only)


[libvigor]:   https://github.com/jhunt/libvigor
[bolo]:       https://github.com/bolo/bolo
[clockwork]:  https://github.com/jhunt/clockwork
