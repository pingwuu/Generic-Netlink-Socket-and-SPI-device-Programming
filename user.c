#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <pthread.h>
#include <linux/genetlink.h>

#include "header.h"

#ifndef GRADING
#define MAX7219_CS_PIN 5
#define HCSR04_TRIGGER_PIN 8
#define HCSR04_ECHO_PIN 10
#endif

pthread_t hcsr_thread, ledmatrix_thread;
pthread_mutex_t mutex;

int distance;

//funtion to send message to kernel
static int msg_to_kernel(struct nl_sock *sock, Response *passingvalue)
{
	struct nl_msg* msg;
	int family_id, err = 0;

	family_id = genl_ctrl_resolve(sock, GENLINK_FAMILY_NAME);
	if(family_id < 0){
		fprintf(stderr, "Unable to resolve family name!\n");
		exit(EXIT_FAILURE);
	}

	msg = nlmsg_alloc();
	if (!msg) {
		fprintf(stderr, "failed to allocate netlink message\n");
		exit(EXIT_FAILURE);
	}

	//printf("Socket message allocated\n");

	if(!genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family_id, 0, 
		NLM_F_REQUEST, GENL_TEST_C_MSG, 0)) {
		fprintf(stderr, "failed to put nl hdr!\n");
		err = -ENOMEM;
		goto out;
	}

	err = nla_put(msg, GENL_TEST_ATTR_MSG,sizeof(struct _Response),(void *)passingvalue);
	if (err) {
		fprintf(stderr, "failed to put nl message!\n");
		goto out;
	}

	//printf("Socket message sent\n");

	err = nl_send_auto(sock, msg);
	if (err < 0) {
		fprintf(stderr, "failed to send nl message!\n");
	}

out:
	nlmsg_free(msg);
	return err;
}


//funtion to send message0 to kernel
static int msg_to_kernel0(struct nl_sock *sock, ConfPin *passpin)
{
	struct nl_msg* msg;
	int family_id, err = 0;

	family_id = genl_ctrl_resolve(sock, GENLINK_FAMILY_NAME);
	if(family_id < 0){
		fprintf(stderr, "Unable to resolve family name!\n");
		exit(EXIT_FAILURE);
	}

	msg = nlmsg_alloc();
	if (!msg) {
		fprintf(stderr, "failed to allocate netlink message0\n");
		exit(EXIT_FAILURE);
	}

	//printf("Socket message0 allocated\n");

	if(!genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family_id, 0, 
		NLM_F_REQUEST, GENL_TEST_C_MSG, 0)) {
		fprintf(stderr, "failed to put nl hdr!\n");
		err = -ENOMEM;
		goto out;
	}

	err = nla_put(msg, GENL_TEST_ATTR_MSG0,sizeof(struct _configpins),(void *)passpin);
	if (err) {
		fprintf(stderr, "failed to put nl message0!\n");
		goto out;
	}

	//printf("Socket message0 sent\n");

	err = nl_send_auto(sock, msg);
	if (err < 0) {
		fprintf(stderr, "failed to send nl message!\n");
	}

out:
	nlmsg_free(msg);
	return err;
}

//funtion to send message1 to kernel
static int msg_to_kernel1(struct nl_sock *sock, Distmsr *distmeasure)
{
	struct nl_msg* msg;
	int family_id, err = 0;

	family_id = genl_ctrl_resolve(sock, GENLINK_FAMILY_NAME);
	if(family_id < 0){
		fprintf(stderr, "Unable to resolve family name!\n");
		exit(EXIT_FAILURE);
	}

	msg = nlmsg_alloc();
	if (!msg) {
		fprintf(stderr, "failed to allocate netlink message1\n");
		exit(EXIT_FAILURE);
	}

	//printf("Socket message1 allocated\n");

	if(!genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family_id, 0, 
		NLM_F_REQUEST, GENL_TEST_C_MSG, 0)) {
		fprintf(stderr, "failed to put nl hdr!\n");
		err = -ENOMEM;
		goto out;
	}

	err = nla_put(msg, GENL_TEST_ATTR_MSG1,sizeof(struct _requestdist),(void *)distmeasure);
	if (err) {
		fprintf(stderr, "failed to put nl message1!\n");
		goto out;
	}

	//printf("Socket message1 sent\n");

	err = nl_send_auto(sock, msg);
	if (err < 0) {
		fprintf(stderr, "failed to send nl message1!\n");
	}

out:
	nlmsg_free(msg);
	return err;
}

//Printing incoming message from kernel
static int print_rx_msg(struct nl_msg *msg, void* arg)
{
	struct nlattr *attr[GENL_TEST_ATTR_MAX+1];

	genlmsg_parse(nlmsg_hdr(msg), 0, attr, 
			GENL_TEST_ATTR_MAX, genlink_policy);

	//printf("---------------start receive mssg----------\n");

	if (attr[GENL_TEST_ATTR_MSG]){

		fprintf(stdout, "Kernel says: %s \n", 
		nla_get_string(attr[GENL_TEST_ATTR_MSG]));
	}
	else if(attr[GENL_TEST_ATTR_MSG0]){

		fprintf(stdout, "Kernel says: %s \n", 
		nla_get_string(attr[GENL_TEST_ATTR_MSG0]));

	}
	else if(attr[GENL_TEST_ATTR_MSG1]){

		fprintf(stdout, "Kernel says: %s \n", 
		nla_get_string(attr[GENL_TEST_ATTR_MSG1]));
		distance = nla_get_u32(attr[GENL_TEST_ATTR_MSG2]);
	}
	else if(attr[GENL_TEST_ATTR_MSG2]){

		fprintf(stdout, "Kernel says: The distance by HCSR is %d \n", 
		nla_get_u32(attr[GENL_TEST_ATTR_MSG2]));
		distance = nla_get_u32(attr[GENL_TEST_ATTR_MSG2]);
		//printf("The stored distance in the variable is %d", distance);

	}
	else {
		fprintf(stdout, "Kernel sent empty message!!\n");
		return NL_OK;
	}

	return NL_OK;
}

//Preparing the socket to start communication
static void prep_nl_sock(struct nl_sock** nlsock)
{
	int family_id;
	
	*nlsock = nl_socket_alloc();
	if(!*nlsock) {
		fprintf(stderr, "Unable to alloc nl socket!\n");
		exit(EXIT_FAILURE);
	}

	//printf("Socket allocated\n");

	nl_socket_disable_seq_check(*nlsock);
	nl_socket_disable_auto_ack(*nlsock);

	/* connect to genl */
	if (genl_connect(*nlsock)) {
		fprintf(stderr, "Unable to connect to genl!\n");
		goto exit_err;
	}

	//printf("Socket connected\n");

	/* resolve the generic nl family id*/
	family_id = genl_ctrl_resolve(*nlsock, GENLINK_FAMILY_NAME);
	if(family_id < 0){
		fprintf(stderr, "Unable to resolve family name!\n");
		goto exit_err;
	}

    return;

exit_err:
    nl_socket_free(*nlsock);
    exit(EXIT_FAILURE);
}

//Distance measurement thread for HCSR sensor
void *hcsr_distance(void *arg)
{
	struct nl_sock* nlsock = NULL;
	struct nl_cb *cb = NULL;

	cb = nl_cb_alloc(NL_CB_DEFAULT);

	while(1){
		prep_nl_sock(&nlsock);
		Distmsr *distmeasure;
		distmeasure = malloc(sizeof(Distmsr));

		strcpy(distmeasure->rqmsgs, "HCSR, Please measure distance");

		msg_to_kernel1(nlsock, distmeasure);
		
		pthread_mutex_lock(&mutex);
		nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_rx_msg, NULL);
		nl_recvmsgs(nlsock, cb);
		pthread_mutex_unlock(&mutex);
		//sleep(3);
	}
	nl_cb_put(cb);
	nl_socket_free(nlsock);
	usleep(50000);
}

//Thread function to send Dot matrix pattern 
void *led_display(void *arg)
{
	struct nl_sock* nlsock = NULL;
	struct nl_cb *cb = NULL;
	int dist_curr=0, dist_prev=0;

	cb = nl_cb_alloc(NL_CB_DEFAULT);

	while(1){
		pthread_mutex_lock(&mutex);
		dist_prev = dist_curr;
		//printf("The stored distance in dist_prev is %d", dist_prev);

		dist_curr = distance;
		//printf("The stored distance in dist_curr is %d", dist_curr);
		pthread_mutex_unlock(&mutex);

		prep_nl_sock(&nlsock);

		Response *passingvalue;
		passingvalue = malloc(sizeof(Response));
		passingvalue->flagmssg = 0;
		unsigned char pattbuff1[4][8] = {
			{0x3C,0x42,0x81,0x81,0x99,0xA5,0x42,0x18},
			{0x3C,0x42,0x81,0x81,0x99,0xA5,0x66,0x18},
			{0x3C,0x42,0x81,0x81,0x99,0x99,0x5A,0x3C},
			{0xFC,0xFA,0x73,0xFF,0xFF,0x7B,0xF2,0xFC}
			};
		unsigned char pattbuff2[4][8] = {
			{0x00,0x1E,0xF3,0x6F,0x6F,0xF3,0x1E,0x00},
			{0x3C,0x42,0x95,0x91,0x91,0x95,0x42,0x3C},
			{0x00,0x1E,0xF3,0x6F,0x6F,0xF3,0x1E,0x00},
			{0x3C,0x42,0x95,0x91,0x91,0x95,0x42,0x3C}
			};


		if ((dist_prev - dist_curr) > 4){										//if the distance keeps decreasing by factor 4cm 
			//printf(" message pattern flag updated\n");

			memcpy(passingvalue->patternBuffer,pattbuff2, sizeof(pattbuff2));


			//printf(" message pattern updated\n");

			msg_to_kernel(nlsock, passingvalue);
		}
		else{																	//original animation

			memcpy(passingvalue->patternBuffer,pattbuff1, sizeof(pattbuff1));
			msg_to_kernel(nlsock, passingvalue);
		}
		sleep(2);
	}
	nl_cb_put(cb);
	nl_socket_free(nlsock);
	usleep(100000);
}

int main(){

	struct nl_sock* nlsock = NULL;
	struct nl_cb *cb = NULL;
	int ret, ret1;
	distance =0;

	prep_nl_sock(&nlsock);
	cb = nl_cb_alloc(NL_CB_DEFAULT);

	ConfPin *passpin;
	passpin = malloc(sizeof(ConfPin));

	passpin->trigg = HCSR04_TRIGGER_PIN;
	passpin->ech = HCSR04_ECHO_PIN;
	passpin->spics = MAX7219_CS_PIN;

	msg_to_kernel0(nlsock, passpin);			//sending pin configurations for HCSR and led matrix

	if (pthread_mutex_init(&mutex, NULL) != 0) 
	{
	    printf("\n mutex init failed\n");
	    return 1;
	}

	ret1 = pthread_create(&hcsr_thread, NULL, hcsr_distance, NULL);			//creating thread for continously measuring distance
	if (ret1 != 0)
	      printf("\ncan't create hcsr thread\n");
	
	ret = pthread_create(&ledmatrix_thread, NULL, led_display, NULL);		//creating thread for continously sending pattern
	if (ret != 0)
	      printf("\ncan't create led matrix thread\n");

	pthread_join (hcsr_thread, NULL);
	pthread_join (ledmatrix_thread, NULL);
	pthread_mutex_destroy(&mutex);

	nl_cb_put(cb);
	nl_socket_free(nlsock);
	return 0;

}