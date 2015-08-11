#include     <stdio.h>  
#include     <stdlib.h>   
#include     <unistd.h>    
#include     <sys/types.h>  
#include     <sys/stat.h>  
#include     <fcntl.h>   
#include     <termios.h>  
#include     <errno.h>  


#define MAX_MAC_HASH_NUM 61
struct vendor_mac_t{
	struct vendor_mac_t * next;
	char name[32];
	char mac[16];
};

struct vendor_mac_t *mac_hash_table[MAX_MAC_HASH_NUM];

int g_vendor_mac_load_flag = 0;
int vendor_mac_hash(int mac[])
{
	if (mac == NULL){
		return -1;
	}
	return (mac[0] ^mac[1] ^mac[2] ) % MAX_MAC_HASH_NUM;
}

void add_mac_item(char *mac_addr,char *vendor_name)
{
	int buf[3] = {0};
	struct vendor_mac_t *item = NULL;
	struct vendor_mac_t * head = NULL;
	sscanf(mac_addr,"%2x-%2x-%2x",&buf[0],&buf[1],&buf[2]);
	int index = vendor_mac_hash(buf);
	head = mac_hash_table[index];
	item = (struct vendor_mac_t *)malloc(sizeof(struct vendor_mac_t));
	if ( NULL == item )
		return;
	item ->next = NULL;
	strcpy(item->name,vendor_name);
	sprintf(item->mac, "%02x:%02x:%02x",buf[0],buf[1],buf[2]);
	if ( head == NULL ){
		mac_hash_table[index] = item;
	}
	else{
		item->next = head->next;
		head->next = item;
	}
}

int load_vendor_mac(void)
{
	FILE * fp = NULL;
	char vendor_buf[64] = {0};
	char mac_buf[64] = {0};
	memset(mac_hash_table,0x0,sizeof(mac_hash_table));
	fp = fopen("/etc/wlan_client_list/vendor_mac.ini","r");
	if ( NULL == fp ){
		printf("open file error!\n");
		return 0;
	}
	while(fgets(vendor_buf,sizeof(vendor_buf),fp))
	{
		if (!strstr(vendor_buf, "#start"))
			continue;
		strcpy(vendor_buf,vendor_buf+sizeof("#start"));
		while(fgets(mac_buf,sizeof(mac_buf),fp)){
			if (strstr(mac_buf, "#end"))
				break;
			add_mac_item(mac_buf,vendor_buf);
			memset(mac_buf,0x0,sizeof(mac_buf));
		}
		memset(vendor_buf,0x0,sizeof(vendor_buf));
	}
	fclose(fp);
	return 1;
}

int getDevTypeByMac(char *usermac,char *devtype)
{
	int buf[3] = {0};
	int index = 0;
	struct vendor_mac_t *p = NULL,*prev = NULL;
	if ( !g_vendor_mac_load_flag ) {
		if (load_vendor_mac())
			g_vendor_mac_load_flag = 1;
	}
	sscanf(usermac,"%2x:%2x:%2x",&buf[0],&buf[1],&buf[2]);
	index = vendor_mac_hash(buf);
	if ( -1 == index ){
		printf("index error!\n");
		goto NOT_MATCH;
	}
	prev = p = mac_hash_table[index];
	while (p){
		if ( !strncasecmp( usermac, p->mac,8)){
			strcpy(devtype, p->name);
			// 匹配成功的mac放在链表头，提高匹配效率
			if ( p != prev ){
				prev->next = p;
				p->next =  mac_hash_table[index];
				mac_hash_table[index] = p;
			}
			return 1;
		}
		prev = p;
		p = p->next;
	}
NOT_MATCH:
	strcpy(devtype,"unknown");
	return 0;
}

static int checkWlanCli(char *usermac)
{
	FILE *fp;
	char buf[64]={0};
	char mac_str[32]={0};
	int aid, psm, mimops, mcs, sgi, stbc;
	char bw[8]={0};

	system("echo "" > /proc/wl_list");
	system("iwpriv ra0 show stainfo");  //在驱动里把无线客户列表信息写到/proc/wl_list文件
	fp = popen("cat /proc/wl_list", "r");
	if(!fp){
		return -1;
	}
	while(fgets(buf, sizeof(buf), fp)){
		sscanf(buf, "%s %d %d %d %s %d %d %d", mac_str,&aid,&psm,&mimops,bw,&mcs,&sgi,&stbc);
		if(16 > strlen(mac_str))
		{
			pclose(fp);
			return 0;
		}
		if(0 == memcmp(usermac,mac_str,17))
		{
			printf("usermac = %s,mac_str = %s\n", usermac, mac_str);
			pclose(fp);
			return 1;
		}

	}
	pclose(fp);
	return 0;
}

int main()  
{  
	FILE *fp;
	int i = 0, ret = 0;
	char hostname[128]={0}, tmpbuf[128] = { 0 };
	char mac_str[32]={0};
	char ip_str[32]={0}, leaseime_str[32] = { 0 };
	char typenamebuf[32]={0}, devtype[32] = { 0 };

	while(1) {
		fp = fopen("/tmp/dhcp.leases", "r");
		if (NULL == fp)
		{
			sleep(30);
			continue;
		}
		while (!feof(fp) && (4 == fscanf(fp, " %10s %17s %s %s %*s",leaseime_str,mac_str,ip_str,hostname)))
		{
			getDevTypeByMac(mac_str, typenamebuf);
			memcpy(devtype, typenamebuf, strlen(typenamebuf)-1);
			ret = checkWlanCli(mac_str);
			if (-1 == ret )
			{
				sleep(30);
				fclose(fp);
				break;
			}
			else if(0 == ret)
			{
				continue;
			}
			memset(tmpbuf, 0, sizeof(tmpbuf));
			sprintf(tmpbuf, "echo %s %s %s %s >> /tmp/wlanClientList", hostname, mac_str, devtype, leaseime_str);
			system(tmpbuf);
		}
		fclose(fp);
		sleep(57);
		system("rm -fr /tmp/wlanClientList");
	}
	
	return 0;
}  
