/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 2002, Ralink Technology, Inc.
 *
 * All rights reserved. Ralink's source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************

    Module Name:
    rbus_main_dev.c

    Abstract:
    Create and register network interface for RBUS based chipsets in linux platform.

    Revision History:
    Who         When            What
    --------    ----------      ----------------------------------------------
*/
#define RTMP_MODULE_OS

/*start of get wireless clients list from proc file to user by tianfy 2014-12-12*/
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include "rt_config.h"

#define PROCNAME "wl_list"
static struct proc_dir_entry *myproc_entry = NULL;
char wl_clients[1024]={0};
/*end of get wireless clients list from proc file to user by tianfy 2014-12-12*/

static struct net_device *rt2880_dev = NULL;


VOID __exit rt2880_module_exit(VOID);
int __init rt2880_module_init(VOID);

MODULE_LICENSE("GPL");
module_init(rt2880_module_init);
module_exit(rt2880_module_exit);

#if defined(CONFIG_RA_CLASSIFIER)&&(!defined(CONFIG_RA_CLASSIFIER_MODULE)) 	 
extern int (*ra_classifier_init_func) (void) ; 	 
extern void (*ra_classifier_release_func) (void) ; 	 
extern struct proc_dir_entry *proc_ptr, *proc_ralink_wl_video;	 
#endif

/*start of get wireless clients list from proc file to user by tianfy 2014-12-12*/
static ssize_t my_file_read(struct file * file,char *data,size_t len,loff_t *off)
{
        if(*off > 0)
                return 0;
        if(copy_to_user(data,wl_clients,strlen(wl_clients)))
                return -EFAULT;
        *off += strlen(wl_clients);
        return strlen(wl_clients);
}

static ssize_t my_file_write(struct file *file, const char *data,size_t len,loff_t *off)
{
        if(copy_from_user(wl_clients,(void*)data,len))
                return -EFAULT;
        wl_clients[0]='\0';
        return len;
}

static struct file_operations my_file_ops = {
       .read = my_file_read,
       .write = my_file_write,
};
/*end of get wireless clients list from proc file to user by tianfy 2014-12-12*/

int rt2880_module_init(VOID)
{
	struct  net_device		*net_dev;
	ULONG				csr_addr;
	INT					rv;
	PVOID				*handle = NULL;
	RTMP_ADAPTER		*pAd;
	unsigned int			dev_irq;
	RTMP_OS_NETDEV_OP_HOOK	netDevHook;
	//struct proc_dir_entry* myproc_entry;

	DBGPRINT(RT_DEBUG_TRACE, ("===> rt2880_probe\n"));	
/*add by tianfy 2014-12-12*/
	myproc_entry = proc_create(PROCNAME, 0666, NULL, &my_file_ops);
	if(!myproc_entry){
	        printk(KERN_ERR "can't create /proc/wl_list n");
	        return -EFAULT;
	}
/*end by tianfy 2014-12-12*/
/*RtmpRaBusInit============================================ */
	/* map physical address to virtual address for accessing register */
	csr_addr = (unsigned long)RTMP_MAC_CSR_ADDR;
	dev_irq = RTMP_MAC_IRQ_NUM;
	

/*RtmpDevInit============================================== */
	/* Allocate RTMP_ADAPTER adapter structure */
/*	handle = kmalloc(sizeof(struct os_cookie) , GFP_KERNEL); */
	os_alloc_mem(NULL, (UCHAR **)&handle, sizeof(struct os_cookie));
	if (!handle)
	{
		DBGPRINT(RT_DEBUG_ERROR, ("Allocate memory for os_cookie failed!\n"));
		goto err_out;
	}
	NdisZeroMemory(handle, sizeof(struct os_cookie));

#ifdef OS_ABL_FUNC_SUPPORT
	/* get DRIVER operations */
	RTMP_DRV_OPS_FUNCTION(pRtmpDrvOps, NULL, NULL, NULL);
#endif /* OS_ABL_FUNC_SUPPORT */

	rv = RTMPAllocAdapterBlock(handle, (VOID **)&pAd);
	if (rv != NDIS_STATUS_SUCCESS)
	{
		DBGPRINT(RT_DEBUG_ERROR, (" RTMPAllocAdapterBlock !=  NDIS_STATUS_SUCCESS\n"));
/*		kfree(handle); */
		os_free_mem(NULL, handle);
		
		goto err_out;
	}
	/* Here are the RTMP_ADAPTER structure with rbus-bus specific parameters. */
	pAd->CSRBaseAddress = (PUCHAR)csr_addr;

	RtmpRaDevCtrlInit(pAd, RTMP_DEV_INF_RBUS);


/*NetDevInit============================================== */
	net_dev = RtmpPhyNetDevInit(pAd, &netDevHook);
	if (net_dev == NULL)
		goto err_out_free_radev;

	/* Here are the net_device structure with pci-bus specific parameters. */
	net_dev->irq = dev_irq;			/* Interrupt IRQ number */
	net_dev->base_addr = csr_addr;		/* Save CSR virtual address and irq to device structure */
	((POS_COOKIE)handle)->pci_dev = net_dev;

#ifdef CONFIG_STA_SUPPORT
    pAd->StaCfg.OriDevType = net_dev->type;
#endif /* CONFIG_STA_SUPPORT */


	
#ifdef RT_CFG80211_SUPPORT
	/*
		In 2.6.32, cfg80211 register must be before register_netdevice();
		We can not put the register in rt28xx_open();
		Or you will suffer NULL pointer in list_add of
		cfg80211_netdev_notifier_call().
	*/
	CFG80211_Register(pAd, pAd->pCfgDev, pNetDev);
#endif /* RT_CFG80211_SUPPORT */

/*All done, it's time to register the net device to kernel. */
	/* Register this device */
	rv = RtmpOSNetDevAttach(pAd->OpMode, net_dev, &netDevHook);
	if (rv)
	{
		DBGPRINT(RT_DEBUG_ERROR, ("failed to call RtmpOSNetDevAttach(), rv=%d!\n", rv));
		goto err_out_free_netdev;
	}

	/* due to we didn't have any hook point when do module remove, we use this static as our hook point. */
	rt2880_dev = net_dev;
	
	wl_proc_init();

	DBGPRINT(RT_DEBUG_TRACE, ("%s: at CSR addr 0x%lx, IRQ %ld. \n", net_dev->name, (ULONG)csr_addr, net_dev->irq));

	DBGPRINT(RT_DEBUG_TRACE, ("<=== rt2880_probe\n"));

#if defined(CONFIG_RA_CLASSIFIER)&&(!defined(CONFIG_RA_CLASSIFIER_MODULE)) 	 
    proc_ptr = proc_ralink_wl_video; 	 
    if(ra_classifier_init_func!=NULL) 	 
	    ra_classifier_init_func(); 	 
#endif

	return 0;

err_out_free_netdev:
	RtmpOSNetDevFree(net_dev);

err_out_free_radev:
	/* free RTMP_ADAPTER strcuture and os_cookie*/
	RTMPFreeAdapter(pAd);
		
err_out:
	return -ENODEV;
	
}


VOID rt2880_module_exit(VOID)
{
	struct net_device   *net_dev = rt2880_dev;
	RTMP_ADAPTER *pAd;


	if (net_dev == NULL)
		return;
	
	/* pAd = net_dev->priv; */
	GET_PAD_FROM_NET_DEV(pAd, net_dev);

	if (pAd != NULL)
	{
		RtmpPhyNetDevExit(pAd, net_dev);
		RtmpRaDevCtrlExit(pAd);
	}
	else
	{
		RtmpOSNetDevDetach(net_dev);
	}
	
	/* Free the root net_device. */
	RtmpOSNetDevFree(net_dev);
	
#if defined(CONFIG_RA_CLASSIFIER)&&(!defined(CONFIG_RA_CLASSIFIER_MODULE))
    proc_ptr = proc_ralink_wl_video; 	 
    if(ra_classifier_release_func!=NULL) 	 
	    ra_classifier_release_func(); 	 
#endif

	wl_proc_exit();
}

