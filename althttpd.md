The Althttpd Webserver
======================

Althttpd is a simple webserver that has run the <https://sqlite.org/> website
since 2004.  Althttpd strives for simplicity, security, and low resource
usage.

As of 2024, the althttpd instance for sqlite.org answers
more than 500,000 HTTP requests per day (about 5 or 6 per second)
delivering about 200GB of content per day (about 18 megabits/second) 
on a $40/month [Linode](https://www.linode.com/pricing).  The load 
average on this machine normally stays around 0.5.  About 19%
of the HTTP requests are CGI to various [Fossil](https://fossil-scm.org/)
source-code repositories.

Design Philosophy
----------------

Althttpd is usually launched from 
[xinetd](https://en.wikipedia.org/wiki/Xinetd) or
[systemd](https://systemd.io) or
similar. A separate process is started for each incoming
connection, and that process is wholly focused on serving that
one connection.  A single althttpd
process will handle one or more HTTP requests over the same connection.
When the connection closes, the althttpd process exits.

Althttpd can also operate stand-alone. Althttpd itself listens on port
80 for incoming HTTP requests (or 443 for incoming HTTPS requests),
then forks a copy of itself to handle each inbound connection.  Each
connection is still handled using a separate process.  The only
difference is that the connection-handler process is now started by a
master althttpd instance rather than by xinetd or systemd.

Althttpd has no configuration file. All configuration is handled
using a few command-line arguments. This helps to keep the
configuration simple and mitigates worries about about introducing
a security vulnerability through a misconfigured web server.

Because each althttpd process only needs to service a single
connection, althttpd is single threaded.  Furthermore, each process
only lives for the duration of a single connection, which means that
althttpd does not need to worry too much about memory leaks.
These design factors help keep the althttpd source code simple,
which facilitates security auditing and analysis.

For serving TLS connections there are two options:

1. althttpd can be built with the `ENABLE_TLS` macro defined and linked to
`-lssl -lcrypto`, then started with the `--cert fullchain.pem` and
`--pkey privkey.pem` flags.

2. althttpd can be started via an external connection service such as
stunnel4, passing the `-https 1` flag to althttpd to tell it that it is
"indirectly" operating in HTTPS mode via that service.

The first option (using the built-in TLS) is preferred.


Source Code
-----------

The complete source code for althttpd is contained within [a single
C-code file](/file/althttpd.c) with no dependences outside of the
standard C library plus OpenSSL if the ENABLE_TLS option is
chosen. Additionally, the build process requires `VERSION.h`, which is
generated by [the included Makefile](/file/Makefile).

The althttpd source code is heavily commented and accessible.
It should be relatively easy to customize for specialized needs.

To build and install althttpd, select one of the build targets
listed at the top of [the Makefile](/file/Makefile), then run:

>
     make THAT_TARGET

If no target is provided it will assume that libssl is available and
will attempt to build both build HTTP-only and HTTPS-aware binaries
named `althttpd` and `althttpsd`, respectively. To install them,
simply move them to the directory of your choice.

The corresponding build rules are trivial, so can be easily
ported into other build infrastructure.

The [SQLite website](https://sqlite.org) uses a
[static build](./static-build.md) so that there is no need
to install OpenSSL on the server.

Setup
-----

There are many wasy of running Althttpd on your system.  Here
are a few variations:

  1.  [Running Althttpd Using Systemd](./linode-systemd.md)
  2.  [Running Althttpd Using Xinetd](./xinetd.md)
  3.  [Running Althttpd As Its Own Standalone Server](./standalone-mode.md)
  4.  [Running Althttpd Using Stunnel4](./stunnel4.md)

The above is not an exhaustive list.
The basic idea is that every time a new socket connection appears on
your webserver port (usually port 80 or 443), you launch a new copy
of the althttpd process to deal with that connection.

A complete setup spec, including a list of all command-line options
and configuration options is in a big header comment in the
[althttpd.c source code file](./althttpd.c).

Hosting Multiple Domains
------------------------

Althttpd uses the HTTP_HOST header of each HTTP request to determine
from where content should be served.
The HTTP_HOST header is the domain name of the URL that prompted the
web-browser to make the HTTP request.
Althttpd makes a copy of this name, converts all 
ASCII alphabetic characters into lower-case and changes
all other characters to "_" and then appends ".website".  So for example,
if the HTTP_HOST is "www.SQLite.org", the converted name will be
"www_sqlite_org.website".  Althttpd then looks for a directory with
that name in its "-root" directory and delivers content out of that directory.
If no such directory is found or if the HTTP request omits the HTTP_HOST
header, then the "default.website" directory is used.  Hence you can
serve multiple websites from the same machine simply by having
multiple *.website folders.  On the Linode that serves the SQLite website,
there are (at last count) 42 *.website folders and symbolic links, including:

  *  [www_sqlite_org.website](https://sqlite.org/)
  *  [default.website](https://sqlite.org/) &larr; *symlink to the previous*
  *  [fossil_scm_org.website](https://fossil-scm.org/)
  *  [pikchr_org.website](https://pikchr.org/)
  *  [www_cvstrac_org.website](http://www.cvstrac.org)
  *  [androwish_org.website](http://androwish.org)

Website Content
---------------

Within each *.website folder, ordinary files are delivered as static
content.  Executable files are run as CGI.  Althttpd will not usually deliver
files whose names begin with "." or "-".  This is a security feature - see
below.

<a id="gzip"></a>
GZip Content Compression
------------------------

Althttpd has basic support for server-side content compression, which
often reduces the over-the-wire cost of files by more than half.
Rather than add a dependency on a compression library to althttpd, it
relies on the website developer to provide content in both compressed
and uncompressed forms.

When serving a file, if the client expresses support for gzip
compression and a file with the same name plus a `.gz` extension is
found, the gzipped copy of the file is served to the client with a
response header indicating that it is gzipped. To the user, it appears
as if the originally-requested file is served compressed. Under the
hood, however, a different file is served.

Note that this feature only works for static files, not CGI.

Security Features
-----------------

To defend against mischief, there are restrictions on names of files that
althttpd will serve.  Within the request URI, all characters other than
alphanumerics and ",-./:~" are converted into a single "_".  Furthermore,
if any path element of the request URI begins with "." or "-" then
althttpd always returns a 404 Not Found error.  Thus it is safe to put
auxiliary files (databases or other content used by CGI, for example)
in the document hierarchy as long as the filenames being with "." or "-".

When althttpd returns a 404, it tries to determine whether the request
is malicous and, if it believes so, it may optionally [temporarily
block the client's IP](#ipshun).

An exception:  Though althttpd normally returns 404 Not Found for any
request with a path element beginning with ".", it does allow requests
where the URI begins with "/.well-known/".  File or directory names
below "/.well-known/" are allowed to begin with "." or "-" (but not
with "..").  This exception is necessary to allow LetsEncrypt to validate
ownership of the website.

Basic Authentication
--------------------

If a file named "-auth" appears anywhere within the content hierarchy,
then access to files in that directory requires
[HTTP basic authentication](https://en.wikipedia.org/wiki/Basic_access_authentication),
as defined by the content of the "-auth" file. The "-auth" file
applies only to the given directory, not recursively into
subdirectories.  The "-auth" file is plain text and line oriented.
Blank lines and lines that begin with "#" are ignored.  Other lines
have meaning as follows:

  *  <b>http-redirect</b>

     The http-redirect line, if present, causes all HTTP requests to
     redirect into an HTTPS request.  The "-auth" file is read and
     processed sequentially, so lines below the "http-redirect" line
     are never seen or processed for http requests.

  *  <b>https-only</b>

     The https-only line, if present, means that only HTTPS requests
     are allowed.  Any HTTP request results in a 404 Not Found error.
     The https-only line normally occurs after an http-redirect line.

  *  <b>realm</b> <i>NAME</i>

     A single line of this form establishes the "realm" for basic
     authentication.  Web browsers will normally display the realm name
     as a title on the dialog box that asks for username and password.

  *  <b>user</b> <i>NAME LOGIN:PASSWORD</i>

     There are multiple user lines, one for each valid user.  The
     LOGIN:PASSWORD argument defines the username and password that
     the user must type to gain access to the website.  The password
     is clear-text - HTTP Basic Authentication is not the most secure
     authentication mechanism.  Upon successful login, the NAME is
     stored in the REMOTE_USER environment variable so that it can be
     accessed by CGI scripts.  NAME and LOGIN are usually the same,
     but can be different.

  *  <b>anyone</b>

     If the "anyone" line is encountered, it means that any request is
     allowed, even if there is no username and password provided.
     This line is useful in combination with "http-redirect" to cause
     all ordinary HTTP requests to redirect to HTTPS without requiring
     login credentials.

Basic Authentication Examples
-----------------------------

The <http://www.sqlite.org/> website contains a "-auth" file in the
toplevel directory as follows:

>
     http-redirect
     anyone

That -auth file causes all HTTP requests to be redirected to HTTPS, without
requiring any further login.  (Try it: visit http://sqlite.org/ and
verify that you are redirected to https://sqlite.org/.)

There is a "-auth" file at <https://fossil-scm.org/private/> that looks
like this:

>
     realm Access To All Fossil Repositories
     http-redirect
     user drh drh:xxxxxxxxxxxxxxxx

Except, of course, the password is not a row of "x" characters.  This
demonstrates the typical use for a -auth file.  Access is granted for
a single user to the content in the "private" subdirectory, provided that
the user enters with HTTPS instead of HTTP.  The "http-redirect" line
is strongly recommended for all basic authentication since the password
is contained within the request header and can be intercepted and
stolen by bad guys if the request is sent via HTTP.

Log File
--------

If the -logfile option is given on the althttpd command-line, then a single
line is appended to the named file for each HTTP request.
The log file is in the Comma-Separated Value or CSV format specified
by [RFC4180](https://tools.ietf.org/html/rfc4180).
There is a comment in the source code that explains what each of the fields
in this output line mean.

The fact that the log file is CSV makes it easy to import into
SQLite for analysis, using a script like this:

>
    CREATE TABLE log(
      date TEXT,             /* Timestamp */
      ip TEXT,               /* Source IP address */
      url TEXT,              /* Request URI */
      ref TEXT,              /* Referer */
      code INT,              /* Result code.  ex: 200, 404 */
      nIn INT,               /* Bytes in request */
      nOut INT,              /* Bytes in reply */
      t1 INT, t2 INT,        /* Process time (user, system) milliseconds */
      t3 INT, t4 INT,        /* CGI script time (user, system) milliseconds */
      t5 INT,                /* Wall-clock time, milliseconds */
      nreq INT,              /* Sequence number of this request */
      agent TEXT,            /* User agent */
      user TEXT,             /* Remote user */
      n INT,                 /* Bytes of url that are in SCRIPT_NAME */
      lineno INT             /* Source code line that generated log entry */
    );
    .mode csv
    .import httplog.csv log
    

The filename on the -logfile option may contain time-based characters 
that are expanded by [strftime()](https://linux.die.net/man/3/strftime).
Thus, to cause a new logfile to be used for each day, you might use
something like:

>
     -logfile /var/logs/althttpd/httplog-%Y%m%d.csv


<a id="ipshun"></a>
Client IP Blocking
------------------

If the `--ipshun DIRECTORY` option is included to althttpd and
DIRECTORY is an absolute pathname (begins with "/") accessible from
within the chroot jail, and if the IP address of the client appears as
a file within that directory, then althttpd might return 503 Service
Unavailable rather than process the request.

*  If the file is zero bytes in size, then 503 is always returned.
   Thus you can "touch" a file that is an IP address name to
   permanently banish that client.

*  If the file is N bytes in size, then 503 is returned if the mtime
   of the file is less than 300*N seconds ago.  In other words, the
   client is banished for five minutes per byte in the file.

Banishment files are automatically created if althttpd gets a request
that would have resulted in a 404 Not Found, and upon examining the
REQUEST_URI the request looks suspicious. Any request that include
/../ is considered a hack attempt, for example. There are other common
vulnerability probes that are also checked. Probably this list of
vulnerability probes will grow with experience.

The banishment files are automatically unlinked after 5 minutes/byte.

Banishment files are initially 1 byte in size. But if a banishment
expires and then a new request arrives prior to 5 minutes per byte of
block-file size, then the file grows by one byte and the mtime is
reset.

The 5-minute banishment time is configurable at build-time by passing
`-DBANISH_TIME=N`, where N is a number of seconds defaulting to 300.
