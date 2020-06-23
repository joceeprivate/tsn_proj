/******************************************************************************

  Copyright (c) 2016, Xilinx Corporation
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of the Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/

/* based on simple_talker openAVB */

#define		_DEFAULT_SOURCE
#include	<sys/types.h>	/* basic system data types */
#include	<sys/socket.h>	/* basic socket definitions */
#include	<time.h>
#include	<netinet/in.h>	/* sockaddr_in{} and other Internet defns */
#include	<arpa/inet.h>	/* inet(3) functions */
#include	<errno.h>
#include	<fcntl.h>		/* for nonblocking */
#include	<netdb.h>
#include	<signal.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<unistd.h>

// Common usage with VTAG 0x8100:				./tsn_talker -i eth0 -t 33024 -d 1 -s 1 

#define MAX_NUM_FRAMES 10
#define NANOSECONDS_PER_SECOND (1000000000ULL)
#define TIMESPEC_TO_NSEC(ts) (((uint64_t)ts.tv_sec * (uint64_t)NANOSECONDS_PER_SECOND) + (uint64_t)ts.tv_nsec)
#define MULTI_ADDR "224.0.0.100"

static clockid_t get_clockid(int fd)
{
#define CLOCKFD 3
#define FD_TO_CLOCKID(fd) ((~(clockid_t)(fd) << 3) | CLOCKFD)

	return FD_TO_CLOCKID(fd);
}

void dumpAscii(uint8_t *pFrame, int i, int *j)
{
	char c;

	printf("  ");
	
	while (*j <= i) {
		c = pFrame[*j];
		*j += 1;
		if (!isprint(c) || isspace(c))
			c = '.';
		printf("%c", c);
	}
}

void dumpFrameContent(uint8_t *pFrame, uint32_t len)
{
	int i = 0, j = 0;
	while (i <= len) {
		if (i % 16 == 0) {
			if (i != 0 ) {
				// end of line stuff
				dumpAscii(pFrame, (i < len ? i : len), &j);
				printf("\n");

			}
			if (i+1 < len) {
				// start of line stuff
				printf("0x%4.4d:  ", i);
			}
		}
		else if (i % 2 == 0) {
			printf("  ");
		}

		if (i == len)
			printf("  ");
		else
			printf("%2.2x", pFrame[i]);

		i += 1;
	}
}

/* void dumpFrame(uint8_t *pFrame, uint32_t len, const struct ethhdr *hdr)
{
	printf("Frame received, ethertype=0x%x len=%u\n", hdr->ethertype, len);
	printf("src: %s\n", ether_ntoa(hdr->shost));
	printf("dst: %s\n", ether_ntoa(hdr->dhost));
	if (hdr->vlan) {
		printf("VLAN pcp=%u, vid=%u\n", (unsigned)hdr->vlan_pcp, hdr->vlan_vid); 
	}
	//dumpFrameContent(pFrame, len);
	printf("\n");
} */

int main(int argc, char* argv[])
{
	int sock_fd;
	int phc_fd;
	int clkid;
	struct sockaddr_in server_addr, client_addr;
	struct ether_addr *macaddr;
	struct ip_mreq mreq;
	int ret;
	socklen_t addrlen;

	if (argc != 4)
	{
		fprintf(stderr, "%s: Usage \"%s <port> <len> <multicast_enable>\n",
				argv[0], argv[0]);
		exit(1);
	}

	int len = atoi(argv[2]);

	sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock_fd < 0)
	{
		perror("socket():");
		exit(2);
	}

	int set = 1;
	ret = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set));
	if (ret < 0)
	{
		perror("setsockopt(): SO_REUSEADDR");
		exit(2);
	}
	
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(atoi(argv[1]));

	ret = bind(sock_fd, &server_addr, sizeof(server_addr));
	if (ret < 0)
	{
		perror("bind()");
		exit(2);
	}
	
	int multicast = atoi(argv[3]);

	if (multicast)
	{
		char loop = 1;
		ret = setsockopt(sock_fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
		if (ret < 0)
		{
			perror("setsockopt(): IP_MULTICAST_LOOP");
			exit(2);
		}

		mreq.imr_multiaddr.s_addr = inet_addr(MULTI_ADDR);
		mreq.imr_interface.s_addr = htonl(INADDR_ANY);

		ret = setsockopt(sock_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
		if (ret < 0)
		{
			perror("setsockopt(): IP_ADD_MEMBERSHIP");
			exit(2);
		}		
	}


	char pBuf[len];
	
	struct timespec now;
	static uint64_t lastNSec = 0;
	static uint64_t nowNSec;
	static uint32_t packetCnt = 0;	

	phc_fd = open("/dev/ptp0", O_RDWR);
	if (phc_fd < 0)
		printf("ptp open failed\n");

	clkid = get_clockid(phc_fd);

	printf("Start receiving UDP frames...\n");

	while (packetCnt < 10000) {

		recvfrom(sock_fd, pBuf, len, 0, (struct sockaddr *) &client_addr, &addrlen);

		//printf("Received a frame from: %s\n", inet_ntoa(client_addr.sin_addr));

		clock_gettime(clkid, &now);
		nowNSec = TIMESPEC_TO_NSEC(now);

		printf("%d\n", nowNSec - lastNSec);
		lastNSec = nowNSec;

		packetCnt++;
		
	}

	if (multicast)
	{
		ret = setsockopt(sock_fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
		if (ret < 0)
		{
			perror("setsockopt(): IP_DROP_MEMBERSHIP");
			exit(2);
		}
	}

	close(sock_fd);
	return 0;
}
