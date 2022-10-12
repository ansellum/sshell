CC=gcc
override CFLAGS += -Wall -Wextra -Werror

sshell: sshell.c
	$(CC) $(CFLAGS) -o sshell sshell.c

clean:
	rm -rf sshell *.txt
