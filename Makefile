Comp := gcc
CFLAGS := -O3 -g -Wall

all: cache_sim

cache_sim: cache_sim.c
	$(Comp) $@.c $(CFLAGS) -o sim_cache -lm
