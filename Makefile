# althttpd和实用程序的Makefile。目标摘要：
#
#    make althttpd                 <--  不带OpenSSL的althttpd
#    make althttpsd                <--  包含OpenSSL以支持TLS的althttpd
#    make static-althttpd          <--  静态链接版本的althttpd
#    make static-althttpsd         <--  静态链接版本的althttpsd
#    make logtodb                  <--  从日志文件构建SQLite数据库的程序
#    make static-logtodb           <--  静态链接版本的相同程序
#
default: althttpd althttpsd
CC=cc
CFLAGS=-Os -Wall -Wextra -I.

VERSION.h:	VERSION manifest manifest.uuid mkversion.c
	$(CC) -o mkversion mkversion.c
	rm -r VERSION.h
	./mkversion manifest.uuid manifest VERSION >VERSION.h

althttpd:	althttpd.c VERSION.h
	$(CC) $(CFLAGS) -o althttpd althttpd.c

althttpsd:	althttpd.c VERSION.h
	$(CC) $(CFLAGS) -fPIC -o althttpsd -DENABLE_TLS althttpd.c -lssl -lcrypto

static-althttpd:	althttpd.c VERSION.h
	$(CC) $(CFLAGS) -static -o static-althttpd althttpd.c

static-althttpsd:	althttpd.c VERSION.h
	$(CC) $(CFLAGS) -static -fPIC -o static-althttpsd -DENABLE_TLS althttpd.c -lssl -lcrypto -lpthread -ldl

sqlite3.o:	sqlite3.c
	$(CC) $(CFLAGS) -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_THREADSAFE=0 -c -o sqlite3.o sqlite3.c

static-logtodb:	logtodb.c sqlite3.o VERSION.h
	$(CC) $(CFLAGS) -static -o static-logtodb logtodb.c sqlite3.o

logtodb:	logtodb.c VERSION.h
	$(CC) $(CFLAGS) -o logtodb logtodb.c -lsqlite3 -lm -ldl -lpthread

clean:
	rm -f althttpd althttpsd VERSION.h sqlite3.o static-althttpd \
              static-althttpsd logtodb static-logtodb
