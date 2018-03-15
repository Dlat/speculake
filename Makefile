
all: inject measure

inject: inject.c indirect.S
	$(CC) -Wl,-Tlinker.ld $^ -o $@

measure: target_fn.c measure.c indirect.S
	$(CC) -Wl,-Tlinker.ld $^ -o $@

measure_noasm: measure.c common.c
	$(CC) -Wl,-Tlinker.ld $^ -o $@


single: target_fn.S common.c measure.c link-single.ld
	$(CC) -m64 -fPIC -pie -mcmodel=large -c measure.c
	$(CC) -m64 -fPIC -pie -mcmodel=large -c common.c
	$(CC) -m64 -fPIC -pie -mcmodel=large -c target_fn.S
	$(CC) -m64 -mcmodel=large -Wl,-Tlink-single.ld target_fn.o common.o measure.o -shared -o libsingle.so
	$(CC) -m64 main.c -lsingle -L./ -o single

aes: aes.S
	$(CC) aes.S -o aes

clean:
	rm inject measure *.o
