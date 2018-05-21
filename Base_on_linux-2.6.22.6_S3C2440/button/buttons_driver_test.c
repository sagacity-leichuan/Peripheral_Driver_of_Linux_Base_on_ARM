#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <signal.h>


//用于测试按键中断+fasync
int fd;

void my_signal_fun(int signum)
{
	unsigned char key_val;
	read(fd, &key_val, 1);
	printf("key_val: 0x%x\n", key_val);
}

int main(int argc, char **argv)
{
	 
	//中断+fasync+ 非阻塞方式（驱动里面有信号量）测试
	/*int fd = 0;
	unsigned char key_val = 0;
	int ret = 0;
	
	fd = open("/dev/buttons", O_RDWR | O_NONBLOCK);    //添加了非阻塞标记
	if (fd < 0)
	{
		printf("can't open!\n");
		return -1;
	}

	while (1)
	{
		ret = read(fd, &key_val, 1);
		printf("key_val: 0x%x, ret = %d\n", key_val, ret);
		sleep(5);
	}
	
	return 0;*/


	//中断+fasync 方式测试
	unsigned char key_val;
	int ret;
	int Oflags;

	//SIGIO : io信号
	signal(SIGIO, my_signal_fun);
	
	fd = open("/dev/buttons", O_RDWR | O_NONBLOCK);
	if (fd < 0)
	{
		printf("can't open!\n");
		return -1;
	}

	fcntl(fd, F_SETOWN, getpid());  //告诉驱动程序此应用程序的PID
	
	Oflags = fcntl(fd, F_GETFL);    //获得fd的flags
	
	fcntl(fd, F_SETFL, Oflags | FASYNC);  //给fd的flags加上异步机制标志，调用驱动的.fasync函数

	while (1)
	{
		sleep(1000);
	}
	
	return 0;

	
	//中断+poll方式测试
	/*int fd;
	unsigned char key_val;
	int ret;

	struct pollfd fds[1];
	
	fd = open("/dev/buttons", O_RDWR);
	if (fd < 0)
	{
		printf("can't open!\n");
	}

	fds[0].fd     = fd;
	fds[0].events = POLLIN; //期待的事件是有数据可以读取
	while (1)
	{
		ret = poll(fds, 1, 5000);
		if (ret == 0)
		{
			printf("time out\n");
		}
		else
		{
			read(fd, &key_val, 1);
			printf("key_val = 0x%x\n", key_val);
		}
	}
	
	return 0;*/


	//中断方式测试
	/*int fd;
	unsigned char key_val = 0;
	
	fd = open("/dev/buttons", O_RDWR);
	if (fd < 0)
	{
		printf("can't open!\n");
		return 0;
	}

	while (1)
	{
		read(fd, &key_val, 1);
		printf("key_val = 0x%x\n", key_val);
	}	
	return 0;*/

	//查询方式测试
	/*int fd;
	unsigned char key_vals[4];
	int cnt = 0;
	
	fd = open("/dev/buttons", O_RDWR);
	if (fd < 0)
	{
		printf("can't open!\n");
	}

	while (1)
	{
		read(fd, key_vals, sizeof(key_vals));
		if (!key_vals[0] || !key_vals[1] || !key_vals[2] || !key_vals[3])
		{
			printf("%04d key pressed: %d %d %d %d\n", cnt++, key_vals[0], key_vals[1], key_vals[2], key_vals[3]);
		}
	}
	return 0;*/

}





