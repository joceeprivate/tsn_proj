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

static clockid_t get_clockid(int fd)
{
#define CLOCKFD 3
#define FD_TO_CLOCKID(fd) ((~(clockid_t)(fd) << 3) | CLOCKFD)

	return FD_TO_CLOCKID(fd);
}

#define MAX_NUM_FRAMES 100
#define NANOSECONDS_PER_SECOND (1000000000ULL)
#define TIMESPEC_TO_NSEC(ts) (((uint64_t)ts.tv_sec * (uint64_t)NANOSECONDS_PER_SECOND) + (uint64_t)ts.tv_nsec)

//#define DEBUG 1

#ifdef DEBUG
#define debug_print(...) printf(__VA_ARGS__)
#else
#define debug_print(...)
#endif

static clockid_t clkid;

int send_n_packets(int sockfd, const void *buff, size_t *nbytes, int flags, const struct sockaddr *to, socklen_t *addrlen, 
						int pkt_limit)
{
	int sent = 0;
	do
	{
		sendto(sockfd, buff, nbytes, flags, to, addrlen);

		sent++;

	} while (sent < pkt_limit);

	return sent;
}

int send_n_packets_rtlmt(int sockfd, const void *buff, size_t *nbytes, int flags, const struct sockaddr *to, socklen_t *addrlen, 
						int pkt_limit, struct timespec *start, uint64_t duration)
{
	uint64_t time_elapsed;
	struct timespec now;
	struct timespec rem;
	int ret;
	int sent = 0;	

	do
	{
		ret = sendto(sockfd, buff, nbytes, flags, to, addrlen);

		if (ret < 0)		
			printf("Frame sent failed!! Error code = %d\n", errno);

		sent++;

		clock_gettime(clkid, &now);

		time_elapsed = (now.tv_sec - start->tv_sec) * NANOSECONDS_PER_SECOND + (now.tv_nsec - start->tv_nsec);

	} while ((time_elapsed < duration) && (sent < pkt_limit));

	if (time_elapsed < duration)
	{
		rem.tv_sec = (duration - time_elapsed) / NANOSECONDS_PER_SECOND;
		rem.tv_nsec = (duration - time_elapsed) % NANOSECONDS_PER_SECOND;
		nanosleep(&rem, NULL);
		start->tv_sec += (start->tv_nsec + duration) / NANOSECONDS_PER_SECOND;
		start->tv_nsec = (start->tv_nsec + duration) % NANOSECONDS_PER_SECOND;
	}
	else if (time_elapsed == duration)
	{
		start->tv_sec += (start->tv_nsec + duration) / NANOSECONDS_PER_SECOND;
		start->tv_nsec = (start->tv_nsec + duration) % NANOSECONDS_PER_SECOND;
	}
	else
	{
		debug_print(" %ld ##############LATE !!!!#############", time_elapsed);
		rem.tv_sec = (duration - (time_elapsed % duration)) / NANOSECONDS_PER_SECOND;
		rem.tv_nsec = (duration - (time_elapsed % duration)) % NANOSECONDS_PER_SECOND;
		nanosleep(&rem, NULL);
		start->tv_sec = now.tv_sec + rem.tv_sec + (now.tv_nsec + rem.tv_nsec) / NANOSECONDS_PER_SECOND;
		start->tv_nsec = (now.tv_nsec + rem.tv_nsec) % NANOSECONDS_PER_SECOND;
	}

	if (sent < pkt_limit)
	{
		printf("Error: Insufficient time to send %d packets in %ld duration\n",
			   pkt_limit, duration);
	}

	return sent;
}

int main(int argc, char *argv[])
{
	struct sockaddr_in server_addr;
	struct timespec rem;
	struct timespec start;

	int sock_fd;
	int phc_fd;
	int txlen;
	int pkt_limit;
	uint64_t duration;
	int ratelimit;
	int ret;
	  
	if (argc != 7)
	{
		fprintf(stderr, "%s: Usage \"%s <ip> <port> <txlen> <pkt_cnt> <duration> <ratelimit>\n",
				argv[0], argv[0]);
		fprintf(stderr, "where seq_offset is the offset after eth_hdr"
						"where sequence 32 bit number is inserted\n");
		fprintf(stderr, "where ratelimit is 0 or 1 \n");
		fprintf(stderr, "if ratelimit is 0, <pkt_cnt> number of packets "
						"are sent full speed and stop\n");
		fprintf(stderr, "if ratelimit is 0, and <pkt_cnt> is -1; packets "
						"are sent full speed continuously\n");
		fprintf(stderr, "if ratelimit is 1 <pkt_cnt> number of packets "
						"are sent in <duration>; and this cycle repeats "
						"continuously\n");
		fprintf(stderr, "if ratelimit is 1 <pkt_cnt> value -1 is "
						"illegal\n");
		exit(1);
	}

	sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock_fd < 0)
	{
		printf("Socket create error\n");
		exit(2);
	}

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(argv[2]));
	ret = inet_pton(AF_INET, argv[1], &server_addr.sin_addr);
	if (!ret)
	{
		printf("IP addr wrong\n");
		exit(2);
	}

	txlen = atoi(argv[3]);
	pkt_limit = atoi(argv[4]);
	duration = atoi(argv[5]);
	ratelimit = atoi(argv[6]);

	if (ratelimit && (pkt_limit < 0))
	{
		printf("Invalid args\n");
		exit(2);
	}
	
	char pBuf[txlen];
	memset(pBuf, 'A', txlen);

	phc_fd = open("/dev/ptp0", O_RDWR);
	if (phc_fd < 0)
		printf("ptp open failed\n");

	clkid = get_clockid(phc_fd);
	clock_gettime(clkid, &start);

	rem.tv_sec = (NANOSECONDS_PER_SECOND - start.tv_nsec + duration) / NANOSECONDS_PER_SECOND;
	rem.tv_nsec = (NANOSECONDS_PER_SECOND - start.tv_nsec + duration) % NANOSECONDS_PER_SECOND;

	nanosleep(&rem, NULL);

	start.tv_sec = start.tv_sec + 1;
	start.tv_nsec = duration;

	printf("UDP sender start time: sec: %ld ns: %ld, dest IP: %s, port: %d, length: %d\n ", 
			start.tv_sec, start.tv_nsec, inet_ntoa(server_addr.sin_addr), atoi(argv[2]), txlen);

	while (1)
	{
		if (ratelimit)
		{
			send_n_packets_rtlmt(sock_fd, pBuf, txlen, 0, (struct sockaddr *) &server_addr, sizeof(server_addr), pkt_limit, &start, duration);
		}
		else
		{
			if (pkt_limit < 0)
				send_n_packets(sock_fd, pBuf, txlen, 0, (struct sockaddr *) &server_addr, sizeof(server_addr), 1);
			else
			{
				/*send pkt_limit packets and exit */
				send_n_packets(sock_fd, pBuf, txlen, 0, (struct sockaddr *) &server_addr, sizeof(server_addr), pkt_limit);
				break;
			}
		}
	}

	close(sock_fd);
	return 0;
}
