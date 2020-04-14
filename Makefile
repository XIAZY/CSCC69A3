CFLAGS=-Wall -g


all: ext2_cp ext2_mkdir ext2_rm ext2_ls ext2_ln ext2_rm_bonus


ext2_cp: ext2_cp.o utils.o
	cc $(CFLAGS) -o $@ $^

ext2_mkdir: ext2_mkdir.o utils.o
	cc $(CFLAGS) -o $@ $^

ext2_rm: ext2_rm.o utils.o
	cc $(CFLAGS) -o $@ $^

ext2_rm_bonus: ext2_rm_bonus.o utils.o
	cc $(CFLAGS) -o $@ $^

ext2_ls: ext2_ls.o utils.o
	cc $(CFLAGS) -o $@ $^

ext2_ln: ext2_ln.o utils.o
	cc $(CFLAGS) -o $@ $^

utils: utils.c utils.h
	cc $(CFLAGS) -c utils.c

readimage:
	cc $(CFLAGS) readimage.c -o readimage

clean:
	rm ext2_cp ext2_ln ext2_ls ext2_mkdir ext2_rm ext2_ln *.o
