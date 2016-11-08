# This makes use of -Wall, -Werror, and -Wmissing-prototypes

CC = gcc
# -Wall允许发出Gcc提供的所有有用的报警信息
# -werror 	把所有的告警信息转化为错误信息，并在告警发生时终止编译过程
CFLAGS = -Wall -Werror -Wmissing-prototypes
OBJS = server.o ioUtils/rio.o memory/list.o thread/thread_pool.o protocol/http_header.o
LDLIBS = -lpthread

.INTERMEDIATE: $(OBJS)

all: bhamWebServer

bhamWebServer: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDLIBS) -o bhamWebServer

server.o: server.h server.c
rio.o: ioUtils/rio.h ioUtils/rio.c
list.o: memory/list.h memory/list.c
thread_pool.o:thread/threadpool.h list.o
http_header.o:http_header.h

clean:
	rm -f *~ *.o bhamWebServer
	
run:
	./bhamWebServer -p 8088
