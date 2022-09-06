
all: echo_client echo_server perf_client perf_server

%: %.c
	gcc -o $@ $^ unp.c $(CFLAGS) $(LIBS)

