all:	sim

sim:	simxcom.c
	gcc -Wall -Werror -o $@  -lX11 -lcairo -lm $<

clean:
	rm -f sim
