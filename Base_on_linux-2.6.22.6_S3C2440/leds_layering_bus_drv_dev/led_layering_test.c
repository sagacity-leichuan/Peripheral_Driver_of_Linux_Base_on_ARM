
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

/* s3c24xx_leds_test on
 * s3c24xx_leds_test off
 */
int main(int argc, char **argv)
{
	int fd;
	int val = 1;
	fd = open("/dev/s3c24xx_leds", O_RDWR);
	if (fd < 0)
	{
		printf("can't open!\n");
	}
	if (argc != 2)
	{
		printf("Usage :\n");
		printf("%s <on|off>\n", argv[0]);
		return 0;
	}

	if (strcmp(argv[1], "on") == 0)
	{
		val  = 1;
	}
	else
	{
		val = 0;
	}
	
	write(fd, &val, 4);
	return 0;
}
