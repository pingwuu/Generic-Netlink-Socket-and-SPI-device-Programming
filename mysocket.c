#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <linux/timer.h>
#include <net/genetlink.h>
#include <linux/export.h>
#include <linux/delay.h>

#include "header.h"
#include "spihead.h"
#include "hcsrhead.h"

//extern Response *r1;

struct data_dotmat ddm_info;
struct data_pins dattconfig;
struct configurepins hcsrconfig;
extern struct measd_dist hcsrdist;

static struct genl_family genlink_family;
//static int messg_kernel_to_user(struct genl_info* info);
//static int messg_kernel_to_user0(struct genl_info* info);
static int messg_kernel_to_user1(struct genl_info* info);

//funtion to receive message from user to kernel
static int genlink_rcv_rp_msg(struct sk_buff* skb, struct genl_info* info){

	//printk(KERN_ALERT "--------nothing-----------");

	if (info->attrs[GENL_TEST_ATTR_MSG]){			//checking if pattern message

		int a=0,i=0, j=0;

		Response *r1 = (Response*)kmalloc(sizeof(Response), GFP_KERNEL);
		r1 =(Response*)nla_data(info->attrs[GENL_TEST_ATTR_MSG]);

		//printk(KERN_NOTICE "%u says id is %d \n", info->snd_portid,r1->flagmssg);
		//printk(KERN_NOTICE "%u says patternBuffer is 0x%x \n", info->snd_portid,r1->patternBuffer[0][4]);

		//memcpy(ddm_info.ledpatt, r1->patternBuffer, sizeof(r1->patternBuffer));			//comm

		for(i=0; i<4; i++){
			for(j=0; j<8; j++){
				ddm_info.ledpatt[i][j] = r1->patternBuffer[i][j];
			}
		}

		//printk("ddm_info value is 0x%x \n", ddm_info.ledpatt[0][4]);

		send_pattern(a);		//comm

		//messg_kernel_to_user(info);

	}
	else if (info->attrs[GENL_TEST_ATTR_MSG0]){			//checking if pin configuration message

		ConfPin *cp1 = (ConfPin*)kmalloc(sizeof(ConfPin), GFP_KERNEL);
		//cp1 =(ConfPin*)nla_data(info->attrs[GENL_TEST_ATTR_MSG0]);

		//printk(KERN_NOTICE "%u says echo pin is %d \n", info->snd_portid,cp1->spics);



		memcpy(cp1, nla_data(info->attrs[GENL_TEST_ATTR_MSG0]),sizeof(ConfPin));
		//memcpy(&dattconfig, cp1, sizeof(dattconfig));

		dattconfig.cspin = cp1->spics;
		dattconfig.trigg = cp1->trigg;
		dattconfig.ech = cp1->ech;
		
		hcsrconfig.echo_pin = cp1->ech;
		hcsrconfig.trigger_pin = cp1->trigg;
		hcsrconfig.cspin = cp1->spics;

		//printk("The CS pin value is %d\n", dattconfig.cspin);
		//printk("The Echo pin value is %d\n", hcsrconfig.echo_pin);
		//printk("The Trigger pin value is %d\n", hcsrconfig.trigger_pin);

		//messg_kernel_to_user0(info);

	}
	else if (info->attrs[GENL_TEST_ATTR_MSG1]){			//checking if distance measurement request message

		int a=0;

		Distmsr *dm1 = (Distmsr*)kmalloc(sizeof(Distmsr), GFP_KERNEL);
		dm1 =(Distmsr*)nla_data(info->attrs[GENL_TEST_ATTR_MSG1]);

		//printk(KERN_NOTICE "%u says %s \n", info->snd_portid,dm1->rqmsgs);

		measure_the_distance(a);

		while(1){

			if(hcsrdist.dist_flag == 1){
				printk("The distance is calculated \n");
				break;
			}
			msleep(100);

		}

		//printk("The distance measured by HCSR sensor is %d", hcsrdist.distancesensed);

		messg_kernel_to_user1(info);

	}
	else {
		printk(KERN_ERR "Empty or no message from %d!!\n",
			info->snd_portid);
		//printk(KERN_ERR "%p\n", info->attrs[GENL_TEST_ATTR_MSG]);
		return -EINVAL;
	}

	return 0;
}

//funtion to reply for pattern message
/*
static int messg_kernel_to_user(struct genl_info* info)
{	
	//printk("==================start message to user from kernel================");
	void *hdr;
	int res, flags = GFP_ATOMIC;
	char msg[GENL_TEST_ATTR_MSG_MAX];
	struct sk_buff* skb = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);

	if (!skb) {
		printk(KERN_ERR "%d: OOM!!", __LINE__);
		return 0;
	}

	hdr = genlmsg_put(skb, 0, 0, &genlink_family, flags, GENL_TEST_C_MSG);
	if (!hdr) {
		printk(KERN_ERR "%d: Unknown err !", __LINE__);
		goto nlmsg_fail;
	}
	printk("==================Test line 1================");

	snprintf(msg, GENL_TEST_ATTR_MSG_MAX, "Hello all. Your message is received!\n");

	res = nla_put_string(skb, GENL_TEST_ATTR_MSG, msg);
	if (res) {
		printk(KERN_ERR "%d: err %d ", __LINE__, res);
		goto nlmsg_fail;
	}
	printk("==================Test line 2================");

	genlmsg_end(skb, hdr);

	printk("==================Test line 3================");

	genlmsg_unicast(genl_info_net(info), skb, info->snd_portid);
	printk(KERN_ALERT "==================Sent message to user from kernel================");
	return 0;

nlmsg_fail:
	genlmsg_cancel(skb, hdr);
	nlmsg_free(skb);
	return 0;
}
*/

//funtion to reply for pin configuration message
/*
static int messg_kernel_to_user0(struct genl_info* info)
{	
	//printk("==================start message to user from kernel================");
	void *hdr;
	int res, flags = GFP_ATOMIC;
	char msg[GENL_TEST_ATTR_MSG_MAX];
	struct sk_buff* skb = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);

	if (!skb) {
		printk(KERN_ERR "%d: OOM!!", __LINE__);
		return 0;
	}

	hdr = genlmsg_put(skb, 0, 0, &genlink_family, flags, GENL_TEST_C_MSG);
	if (!hdr) {
		printk(KERN_ERR "%d: Unknown err !", __LINE__);
		goto nlmsg_fail;
	}
	printk("==================Test line 1================");

	snprintf(msg, GENL_TEST_ATTR_MSG_MAX, "Hello all. Your message0 is received!\n");

	res = nla_put_string(skb, GENL_TEST_ATTR_MSG0, msg);
	if (res) {
		printk(KERN_ERR "%d: err %d ", __LINE__, res);
		goto nlmsg_fail;
	}
	printk("==================Test line 2================");

	genlmsg_end(skb, hdr);

	printk("==================Test line 3================");

	genlmsg_unicast(genl_info_net(info), skb, info->snd_portid);
	printk(KERN_ALERT "==================Sent message to user from kernel================");
	return 0;

nlmsg_fail:
	genlmsg_cancel(skb, hdr);
	nlmsg_free(skb);
	return 0;
}
*/

//funtion to reply with the measured distance by HCSR
static int messg_kernel_to_user1(struct genl_info* info)
{	
	//printk("==================start message to user from kernel================");
	void *hdr;
	int res, flags = GFP_ATOMIC;
	int msg;
	struct sk_buff* skb = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);

	if (!skb) {
		printk(KERN_ERR "%d: OOM!!", __LINE__);
		return 0;
	}

	hdr = genlmsg_put(skb, 0, 0, &genlink_family, flags, GENL_TEST_C_MSG0);
	if (!hdr) {
		printk(KERN_ERR "%d: Unknown err !", __LINE__);
		goto nlmsg_fail;
	}
	//printk("==================Test line 1================");

	msg = hcsrdist.distancesensed;

	//snprintf(msg, GENL_TEST_ATTR_MSG_MAX, "Hello all. Your message1 is received!\n");

	res = nla_put(skb, GENL_TEST_ATTR_MSG2, sizeof(int), (void *)&msg);
	if (res) {
		printk(KERN_ERR "%d: err %d ", __LINE__, res);
		goto nlmsg_fail;
	}
	//printk("==================Test line 2================");

	genlmsg_end(skb, hdr);

	//printk("==================Test line 3================");

	genlmsg_unicast(genl_info_net(info), skb, info->snd_portid);
	//printk(KERN_ALERT "==================Sent message to user from kernel================");
	return 0;

nlmsg_fail:
	genlmsg_cancel(skb, hdr);
	nlmsg_free(skb);
	return 0;
}

//assigning operation to the socket family
static const struct genl_ops genlink_ops[] = {
	{
		.cmd = GENL_TEST_C_MSG,
		.policy = genlink_policy,
		.doit = genlink_rcv_rp_msg,
		.dumpit = NULL,
	},
};

//declaring the values of the the socket family
static struct genl_family genlink_family = {
	.name = GENLINK_FAMILY_NAME,
	.version = 1,
	.maxattr = GENL_TEST_ATTR_MAX,
	.netnsok = false,
	.module = THIS_MODULE,
	.ops = genlink_ops,
	.n_ops = ARRAY_SIZE(genlink_ops),
};

//Core init funtion for socket, spi and hcsr
static int __init genlink_init(void){
	int k_reg;
	int a=0;

	printk("genlink: loading netlink socket \n");

	k_reg = genl_register_family(&genlink_family);
	if (k_reg != 0)
		goto regfail;

	driver_initialisation(a);
	spidev_init(a);

	return 0;

regfail:
	printk(KERN_DEBUG "genlink: couldn't create socket");
	return -EINVAL;
}


//Core exit function for socket, spi, hcsr
static void __exit genlink_exit(void){

	int ret;
	int a=0;
	printk("genlink: Unloading netlink socket.\n");

	spidev_exit(a);
	driver_exiting(a);

	ret = genl_unregister_family(&genlink_family);
	if(ret !=0) {
		printk("Unregister family %i\n",ret);
	}

}



module_init(genlink_init);
module_exit(genlink_exit);

MODULE_AUTHOR("Viraj Savaliya");
MODULE_DESCRIPTION("Socket-kernel");
MODULE_LICENSE("GPL");