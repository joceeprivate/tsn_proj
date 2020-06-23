/*
 * TSN Configuration utility
 *
 * (C) Copyright 2017, Xilinx, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/in.h>
#include <syscall.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <libconfig.h>
#include <errno.h>

enum axienet_tsn_ioctl {
	SIOCCHIOCTL = SIOCDEVPRIVATE,
	SIOC_GET_SCHED,
	SIOC_PREEMPTION_CFG,
	SIOC_PREEMPTION_CTRL,
	SIOC_PREEMPTION_STS,
	SIOC_PREEMPTION_COUNTER,
	SIOC_QBU_USER_OVERRIDE,
	SIOC_QBU_STS,
	SIOC_TADMA_STR_ADD,
	SIOC_TADMA_PROG_ALL,
	SIOC_TADMA_STR_FLUSH,
	SIOC_PREEMPTION_RECEIVE,
	SIOC_TADMA_OFF,
};

struct tadma_stream {
	unsigned char is_trdp;
	unsigned char dmac[6];
	unsigned short vid;
	unsigned char ip[4];
	unsigned int comid;
	unsigned int trigger;
	unsigned int count;
	unsigned char start;
};

int change_to_continuous(char *ifname)
{
	struct ifreq s;
	int ret;

	if(!ifname)
		return -1;

	int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

	strcpy(s.ifr_name, ifname);

	ret = ioctl(fd, SIOC_TADMA_OFF, &s);

	if(ret < 0)
	{
		perror("Cannot be changed to continuous mode");
	}
	close(fd);
}

int flush_stream(char *ifname)
{
	struct ifreq s;
	int ret;

	if(!ifname)
		return -1;

	int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

	strcpy(s.ifr_name, ifname);

	ret = ioctl(fd, SIOC_TADMA_STR_FLUSH, &s);

	if(ret < 0)
	{
		perror("TADMA stream flush failed");
	}
	close(fd);
}
int add_stream(struct tadma_stream *stream, char *ifname)
{
	struct ifreq s;
	int ret;

	if(!ifname)
		return -1;

	int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

	s.ifr_data = (void *)stream;

	strcpy(s.ifr_name, ifname);

	ret = ioctl(fd, SIOC_TADMA_STR_ADD, &s);

	if(ret < 0)
	{
		perror("TADMA stream add failed");
	}
	close(fd);
}

int program_all_streams(char *ifname)
{
	struct ifreq s;
	int ret;

	if(!ifname)
		return -1;

	int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

	s.ifr_data = (void *)NULL;

	strcpy(s.ifr_name, ifname);

	ret = ioctl(fd, SIOC_TADMA_PROG_ALL, &s);

	if(ret < 0)
	{
		perror("TADMA stream program failed");
	}
	close(fd);
}

int sortbytrigger(const void *i1, const void *i2)  {
    struct tadma_stream **a = (struct tadma_stream **)i1;
    struct tadma_stream **b = (struct tadma_stream **)i2;
    return ((*a)->trigger - (*b)->trigger);
}
int get_interface(char *argv)
{	
        if (!strcmp(argv, "eth1"))
                return 1;
        else if (!strcmp(argv, "ep"))
                return 0;
        else
		return -1;
}
void usage()
{
	printf("Usage-1 :\n tadma_prog <interface>\n");
	printf("	Default configuration of streams is in /etc/streams.cfg\n");
	printf("Usage-2 :\n tadma_prog -c <interface> [file name]\n");
	printf("	file name : user defined streams's configuration. Ex: streams_01.cfg\n");
	printf("Usage-3 :\n tadma_prog <interface> off\n");
	printf("	Programs TADMA back to continuous mode\n");
	printf("interface : [ep/eth0/eth1]\n");
	printf("-c : To use the configuration of streams in the file at mentioned path\n");
	exit(1);
}

int main(int argc, char **argv)
{
	config_t cfg;
	config_setting_t *setting;
	char str[120];
	char ifname[IFNAMSIZ];
	int i;
	int ch;
	struct tadma_stream **stream;
	char *path;
	int len;

	config_init(&cfg);
	if((argc < 2) || (argc > 4)) 
		usage();
	if(argc == 4 && (strcmp(argv[1],"-c") == 0)) {
		len = strlen(argv[3]);
		path = (char*)malloc((len+1)*sizeof(char));
		strcpy(path,argv[3]);
		if (get_interface(argv[2]) < 0)
			usage();	
		strncpy(ifname, argv[2], IFNAMSIZ);

	} else if (argc ==3 && (strcmp(argv[2], "off") == 0)) {
		if (get_interface(argv[1]) < 0)
			usage();	
		strncpy(ifname, argv[1], IFNAMSIZ);
		flush_stream(ifname);
		change_to_continuous(ifname);
		return;
	} else {
		if(argc == 3 || argc == 4) 
			usage();
		len = strlen("/etc/streams.cfg");
		path = (char*)malloc((len+1)*sizeof(char));
		strcpy(path,"/etc/streams.cfg");
		if (get_interface(argv[1]) < 0)
			usage();	
		strncpy(ifname, argv[1], IFNAMSIZ);
	}
	if( !config_read_file(&cfg, path) )
	{		
		printf("Can't read %s, Error: %s\n", path, config_error_text(&cfg));
		config_destroy(&cfg);
		return(EXIT_FAILURE);
	}

	sprintf(str, "streams");

	setting = config_lookup(&cfg, str);
	if(setting != NULL)
	{
		int loop = config_setting_length(setting);
		if (loop == 0) {
			printf("Number of stream entries cannot be zero\n");
			usage();	
		}
		
		stream = malloc(sizeof(struct tadma_stream*) * loop);
		for(i = 0; i < loop; i++)
		stream[i] = malloc(sizeof(struct tadma_stream));
		/* TODO */
		/* if( count > stream_count) */
		for(i = 0; i < loop; i++)
		{
			const char *mac_buf;
			const char *ip_buf;
			unsigned short vid;
			unsigned int comid;
			unsigned int trigger, count;
			unsigned char mac[6];
			unsigned char ip[4];
			unsigned char is_trdp;

			config_setting_t *cs =
				config_setting_get_elem(setting, i);
			
			if(!config_setting_lookup_bool(cs, "is_trdp", &is_trdp))
				continue;

			stream[i]->is_trdp = is_trdp;
			if(is_trdp)
			{
				if(!config_setting_lookup_string(cs, "ip", &ip_buf))
					continue;

				sscanf(ip_buf, "%d.%d.%d.%d", &ip[0],
				&ip[1], &ip[2], &ip[3]);
				config_setting_lookup_int(cs, "comid", &comid);
				
				memcpy(stream[i]->ip, ip, 4);
				stream[i]->comid = comid;

				printf("ip: %s comid: %d ", ip_buf, comid);
			}
			else
			{
				if(!config_setting_lookup_string(cs, "mac", &mac_buf))
					continue;

				sscanf(mac_buf, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0],
				&mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);				
				config_setting_lookup_int(cs, "vid", &vid);

				memcpy(stream[i]->dmac, mac, 6);
				stream[i]->vid = vid;

				printf("mac: %s vid: %d ", mac_buf, vid);
			}

			config_setting_lookup_int(cs, "trigger", &trigger);
			config_setting_lookup_int(cs, "count", &count);
			
			stream[i]->trigger = (unsigned int)trigger;
			stream[i]->count = (unsigned int)count;

			printf("trigger: %d, count: %d\n", trigger, count);

			if(count > 4) {
				printf("Error: count value should not be greater than 4\n");
				goto end;
			}
			
			stream[i]->start = (unsigned char)i?0:1;
			
		}
		qsort(stream, loop, sizeof(struct tadma_stream*), sortbytrigger);
		flush_stream(ifname);
		for(i = 0; i < loop; i++)
		{
			//printf("vid: %d trigger: %d\n",
			//		stream[i]->vid, stream[i]->trigger);
			add_stream(stream[i], ifname);
		}
		program_all_streams(ifname);
end:
		for(i = 0; i < loop; i++)
		free(stream[i]);
		
		free(stream);
	}
}
