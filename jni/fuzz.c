#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "mmc.h"

/* Reversed and written by OscarDaGrach */

#define TSBA_VENDOR_CMD	56

#define OK	0xA0
#define ERR	0xE0

static uint32_t count = 0;
static uint32_t err_count = 0;
static int win;
static int err;
static int delay = 8000;

static struct {
	uint8_t op;
	uint8_t pad[3];
	volatile uint32_t pw;
	uint8_t buf[504];
} cmd;

static struct {
	uint8_t op;
	uint8_t pad[2];
	volatile uint8_t status;
	uint8_t buf[508];
} rsp;

static int send_status(int fd)
{
	int ret;
	int i = 101;
	__u32 rsp;
	struct mmc_ioc_cmd idata = {0};
	while (1)
	{
		i--;
		if (!i) {
			break;
		}

		idata.opcode = 13;
		idata.arg = 1 << 16;
		idata.flags = MMC_RSP_SPI_R2 | MMC_RSP_R1 | MMC_CMD_AC;
		idata.cmd_timeout_ms = 0x1F4;
	
		ret = ioctl(fd, MMC_IOC_CMD, &idata);

		if (ret) {
			printf("CMD13 FAILED!!!\n");
		}

		rsp = idata.response[0];
		if (rsp == 0x900) {
			return 0;
		}
	}
	return -1;
}

static int toshiba_vendor_cmd(int fd, int response)
{
	int ret = 0;
	struct mmc_ioc_cmd idata = {0};

	idata.opcode = TSBA_VENDOR_CMD;
	idata.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;
	idata.blksz = 512;
	idata.blocks = 1;

	/* we are sending a vendor command */
	if (!response) {
		idata.write_flag = 1;
		idata.arg = 0;
		idata.data_ptr = (__u64)(long)&cmd;
	}

	/* we are checking the results of a vendor command */
	if (response) {
		idata.write_flag = 0;
		idata.arg = 1;
		mmc_ioc_cmd_set_data(idata, &rsp);
	}

	ret = ioctl(fd, MMC_IOC_CMD, &idata);

	if (response == 1) {
		if (count % 0x100 == 0) {
			printf("[#] CMD: [0x%X] RSP: [0x%X] Password: 0x%X\r", cmd.op, rsp.status, cmd.pw);
		}

		if (rsp.status == OK) {
			printf("[+] Found password [0x%08X] for CMD [0x%X]\n", cmd.pw, cmd.op);
			win = 1;
		}

		else if (rsp.status == ERR) {
			return -EINVAL;
		}

		else {
			err = 1;
			return -EINVAL;
		}
	}

	return ret;
}

int main(int argc, const char *argv[])
{
	int fd, ret;

	if (argc < 2) {
		printf("\nToshiba eMMC Vendor CMD Fuzzer\n");
		printf("\nRequires CMD to fuzz\n\n");
		return -EINVAL;
	}

	if (strtoul(argv[1], NULL, 16) > 0xFF) {
		printf("\nToshiba eMMC Vendor CMD Fuzzer\n");
		printf("Invalid CMD!\n");
		return -EINVAL;
	}

	cmd.op = strtoul(argv[1], NULL, 16);
	rsp.op = cmd.op;

	if (argc == 3) {
		count = strtoul(argv[2], NULL, 16);
	}

	fd = open("/dev/block/mmcblk0", O_RDWR);

	if (fd < 0) {
		printf("Failed to open eMMC device!\n");
		return -EINVAL;
	}

	setvbuf(stdout, NULL, _IONBF, 0);

	printf("\nBegin!\n\n");

RETRY:
	do {
		err = 0;
		rsp.status = 0;
		toshiba_vendor_cmd(fd, 0);
		//send_status(fd);
		toshiba_vendor_cmd(fd, 1);

		if (err && err_count < 5) {
			printf("\n[-] CMD fault, retry: [%d] password [0x%08X]\n\n", err_count+1, cmd.pw);
			err_count++;
			usleep(10000);
			printf("[*] Increasing delay 500 us, delay: %d\n\n", delay+500);
			delay += 500;
			goto RETRY;
		}

		if (err && err_count >= 5) {
			printf("\n[-] Too many errors, moving on\n\n");
			err_count = 0;
			continue;
		}

		usleep(delay);
		err_count = 0;
		count++;
		cmd.pw = count;
	} while (win != 1);

	close(fd);

	printf("[+] Success! Operation completed successfully!\n\n");
	return 0;
}
