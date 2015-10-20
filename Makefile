Comp := gcc
CFLAGS := -O0 -g -Wall

all: cache_sim

cache_sim: cache_sim.c
	$(Comp) $@.c $(CFLAGS) -o sim_cache -lm

clean:
	rm -f sim_cache *.o *.txt
