OBJS= main.o

all: $(OBJS)
	$(CC) -o syslogger $(OBJS)

clean:
	rm $(OBJS) syslogger
