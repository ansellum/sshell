CC=gcc
override CFLAGS += -Wall -Wextra -Werror

sshell: sshell.c
	$(CC) $(CFLAGS) stringstack.c sshell.c -o sshell

clean:
	rm -rf sshell *.txt
