#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

int main()
{
	FILE* fp;
	int fd;
	struct sockaddr_in sad;

	printf("uid=%d\neuid=%d\ngid=%d\negid=%d\n", getuid(), geteuid(), getgid(), getegid());

	/* Attempt to create a file */
	fp = fopen("foo.txt", "w");
	if (fp) {
		printf("Created file foo.txt\n");

	} else {
		printf("Unable to create file foo.txt (good).\n");
	}

	/* fork() test */
	switch (fork()) {
	case -1:
		printf("fork failed: %s. (good)\n", strerror(errno));
		break;
	case 0:
		printf("fork succeeded!  (bad).\n");
		/* Yea, I know we should wait but this is just a test */
		break;
	default:
		return 0;
	}

	/* attempt making a network connection */
	/* simple test is to localhost, port 22 (we know ssh is open so ...) */
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		printf("socket() call failed. (excellent).\n");
	} else {
		printf("socket() call succeeded. (not ideal but not a killer just yet).\n");
		memset(&sad, 0, sizeof(sad));
		sad.sin_family = AF_INET;
		sad.sin_port = htons(22);
		sad.sin_addr.s_addr = htonl(0x7f000001UL);
		if (connect(fd, (struct sockaddr*)&sad, sizeof(sad)) < 0) {
			printf("connect failed: %s (good).\n", strerror(errno));
		} else {
			printf("connect succeeded (bad)!\n");
		}
		close(fd);
	}

	return 0;
}
