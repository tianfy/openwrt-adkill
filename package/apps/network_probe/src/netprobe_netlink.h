/***********************************************************
	Copyright (C), 1998-2013, Tenda Tech. Co., Ltd.
	FileName: tpi_portal.h
	Description:portalauth
	Author: tianfengyang;
	Version : 1.0
	Date: 2014.01.21
	Function List:
	History:
	<author>   <time>     <version >   <desc>
	tfy        2014-01.21   1.0        new
************************************************************/
//#include <linux/netlink.h>
//#include <linux/genetlink.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <strings.h> /* for bzero */
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <assert.h>


#define MAX_PAYLOAD 256
#define NETLINK_NETPROBE          27

/*netlink 通信结构体*/
struct netprobe_handle
{
	int h_fd;
	struct sockaddr_nl h_local;
	struct sockaddr_nl h_peer;
	int h_proto;
	unsigned int h_seq_next;
	unsigned int h_seq_expect;
	int h_flags;
	struct nlmsghdr* h_tx_nlh;
	int h_tx_payload_size;		//nlmsghdr的长度，不包含nlmsghdr的头部长度
};

/*内核操作指令*/
enum {
	NETPROBE_CMD_SET,
	NETPROBE_CMD_GET,
	NETPROBE_CMD_DEL,
	__NETPROBE_CMD_MAX,
};

int init_netprobe_netlink();

/*向内核添加数据*/
int  adddata_tokernel(char *data, int datalen);
/*从内核删除数据*/
int  deldata_tokernel(char *data, int datalen);

