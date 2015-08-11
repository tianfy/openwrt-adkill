/***********************************************************
	Copyright (C), 1998-2013, Tenda Tech. Co., Ltd.
	FileName: tpi_ewarn.c
	Description:portal set
	Author: tianfengyang;
	Version : 1.0
	Date: 2013.10.28
	Function List:
	1.clear_portal_rules
	2.load_portal_rules
	3.tpi_portal_set
	History:
	<author>   <time>     <version >   <desc>
	tianfengyang        2013-10.28   1.0        new
************************************************************/
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ether.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>

//#include <semaphore.h>
#include <linux/if_ether.h>
#include <stdint.h>

#include "netprobe_netlink.h"

//static sem_t g_ptnl_sem;
struct netprobe_handle *netprobehandle = NULL;


void inline *netprobenlmsg_tail(const struct nlmsghdr *nlh)
{
	return (unsigned char *)nlh + NLMSG_ALIGN(nlh->nlmsg_len);
}

int inline netprobenla_pad_length(int payload)
{
	return NLA_ALIGN(NLA_HDRLEN + payload) - (NLA_HDRLEN + payload);
}

void inline *netprobenla_data(const struct nlattr *nla)
{
	return (unsigned char *)nla + NLA_HDRLEN;
}

struct netprobe_handle *netprobenl_alloc_handle(int payload)
{
	struct netprobe_handle *handle;

	handle = calloc(1, sizeof(*handle));
	
	if (NULL == handle) {
		printf("The handle calloc failed\n");
		return NULL;
	}

	handle->h_fd = -1;
	handle->h_peer.nl_family = AF_NETLINK;
	handle->h_peer.nl_pid = 0;
	handle->h_peer.nl_groups = 0;
	//handle->h_seq_expect = handle->h_seq_next = time(0);
	

	handle->h_tx_nlh = calloc(1, NLMSG_SPACE(payload));
	if (NULL == handle->h_tx_nlh){
		printf("The rx nlmsghdr calloc failed\n");
		free(handle);
		return NULL;
	}
	handle->h_tx_nlh->nlmsg_len= NLMSG_ALIGN(NLMSG_HDRLEN);
	handle->h_tx_payload_size = NLMSG_ALIGN(payload);

	return handle;
}

int netprobenl_send_msg(struct netprobe_handle *handle)
{
	struct msghdr msg;
	struct iovec iov;
	
	memset(&msg, 0x0, sizeof(msg));
	memset(&iov, 0x0, sizeof(struct iovec));
	
	//handle->h_seq_expect = handle->h_seq_next;
	//handle->h_tx_nlh->nlmsg_seq = handle->h_seq_next++;
	handle->h_tx_nlh->nlmsg_pid = 0;//getpid();
	handle->h_tx_nlh->nlmsg_flags |= NLM_F_REQUEST;
	handle->h_tx_nlh->nlmsg_type = NLMSG_MIN_TYPE;	
	iov.iov_base = (void *)handle->h_tx_nlh;
	iov.iov_len = NLMSG_ALIGN(handle->h_tx_nlh->nlmsg_len);
	
	msg.msg_name = (void *)&handle->h_peer;
	msg.msg_namelen = sizeof(struct sockaddr_nl);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	printf("send msg to kernel\n");
	return sendmsg(handle->h_fd, &msg, 0);
}



/**************************************************************
handle: 句柄
attrtype: 属性类型
attrlen：数据的负载大小，不包过nlattr的头部大小
data: 存储的数据
返回值：-1空间不够，0成功
***************************************************************/
int netprobenla_put(struct netprobe_handle *handle, int attrtype, int attrlen, void *data)
{
	struct nlattr *nla = NULL;
	int total_len = 0;

	handle->h_tx_nlh->nlmsg_len = NLMSG_ALIGN(NLMSG_HDRLEN);	//由于每次只设置一个属性，每次都将其初始化为nlmsghdr头部的长度，不带数据
	memset((unsigned char *)handle->h_tx_nlh + NLMSG_ALIGN(handle->h_tx_nlh->nlmsg_len), 0x0, handle->h_tx_payload_size);
	total_len = NLMSG_ALIGN(handle->h_tx_nlh->nlmsg_len) + NLA_ALIGN(NLA_HDRLEN + attrlen);
	if (total_len - NLMSG_HDRLEN > handle->h_tx_payload_size){
		printf("the room of nlmsghdr is full\n");
		return -1;
	}
	nla = (struct nlattr *)netprobenlmsg_tail(handle->h_tx_nlh);
	nla->nla_type = attrtype;
	nla->nla_len = NLA_HDRLEN + attrlen;
	memcpy((void *)netprobenla_data(nla), data, attrlen);
	//由于nlmsghdr是四字节对齐的，所以当传递的数据时字符串时，最后并没有'\0'结尾，要拷贝该字符串必须通过nla->nla_len
	//计算出来，当通过strlen计算字符串长度会出现错误
	memset((unsigned char *)nla + nla->nla_len, 0x0, netprobenla_pad_length(attrlen));	//由于nlattr是四字节对齐，填充后面空字段
	handle->h_tx_nlh->nlmsg_len = total_len;
	
	return 0;
}


int  adddata_tokernel(char *data, int datalen)
{
	int err = 0;
	
	//sem_wait(&g_ptnl_sem);
	err = netprobenla_put(netprobehandle, NETPROBE_CMD_SET, datalen, data);
	if (err < 0){
	     // sem_post(&g_ptnl_sem);
		printf("portal add error\n");
		return -1;
	}
	
	if ((err = netprobenl_send_msg(netprobehandle)) < 0) {
	      //sem_post(&g_ptnl_sem);
		printf("send msg error\n");
		return -1;
	}
      //sem_post(&g_ptnl_sem);
	return 0;
}

int deldata_tokernel(char *data, int datalen)
{
	int err = 0;
	
	//sem_wait(&g_ptnl_sem);
	err = netprobenla_put(netprobehandle, NETPROBE_CMD_DEL, datalen, data);
	if (err < 0){
	      //sem_post(&g_ptnl_sem);
		printf("portal add error\n");
		return -1;
	}
	
	if ((err = netprobenl_send_msg(netprobehandle)) < 0) {
	      //sem_post(&g_ptnl_sem);
		printf("send msg error\n");
		return -1;
	}
      //sem_post(&g_ptnl_sem);
	return 0;
}

int netprobenl_connect(struct netprobe_handle *handle, int protocol)
{
	int err = -1;

	handle->h_fd = socket(AF_NETLINK, SOCK_RAW, protocol);
	if (handle->h_fd < 0) {
		printf("socket(AF_NETLINK, ...) failed\n");
		handle->h_fd = -1;
		return err;
	}

	err = bind(handle->h_fd, (struct sockaddr*)&(handle->h_peer),
		   sizeof(handle->h_peer));
	if (err < 0) {
		printf("bind() failed\n");
		close(handle->h_fd);
		handle->h_fd = -1;
		return err;
	}
	//handle->h_proto = protocol;

	return 0;
}


int init_netprobe_netlink()
{
	int err = 0;
    	//sem_init(&g_ptnl_sem, 0, 1); /*信号量初始化*/
	netprobehandle = netprobenl_alloc_handle(MAX_PAYLOAD);
	if (NULL == netprobehandle){
		printf("the handle alloc failed\n");
		return -1;
	}
	
	err = netprobenl_connect(netprobehandle, NETLINK_NETPROBE);
	if (err < 0){
		printf("the socket connect failed\n");
		return err;
	}
	return err;
}

