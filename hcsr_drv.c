#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/string.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <asm/div64.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <asm/delay.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/workqueue.h>
#include <linux/stat.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include "hcsrhead.h"

#define DEVICE_NAME                 "HCSR"  // device name created and registered
#define MAX 15

extern struct configurepins hcsrconfig;
struct measd_dist hcsrdist;


static short int num_of_devices = 1;
int ultrasonic_speed= 346;    // 346 m/s

static int gpios[80];

void command_sigs(int pin_1, int pin_2,void *ptr);

struct Cmd{
  char cmd_name[20];
  int para_1;
  int para_2;
};

// configuration 
struct configurations{         
    int trigPin; // trigPin=5 if trig is connected to IO 5
    int echoPin; 
    int m;      //samples Per Measurement
    int delta;  //sampling Period
};

struct fifo_buffer{                      
  unsigned long long int time_stamp;
  unsigned long long int value;
};

struct mutex write_mutex;
struct mutex device_mutex;


/* per device structure */
struct HCSR_dev {
	struct cdev cdev;               /* The cdev structure */
    char name[20];                  /* Name of device*/
    struct fifo_buffer buff[6];            /* buffer for the input string */
    int head;
    int current_pos;                        // indices for the fifo buffer
    int count;
    struct configurations conf; 
    int mu_flag;
  	struct hrtimer hrt;
    long long unsigned int distance;
	int trigger_flag;    
	int expire_time;
	int data_available;
	int delay_flag;
	ktime_t ref1;
	ktime_t ref2;
	int irq_num;
	struct miscdevice my_misc_dev;             							/* The  miscdevice  structure */
	struct task_struct *dev_kthreads[10];
	struct mutex device_mutex;
	int thr_cr;
	struct semaphore mr_sem;
	struct task_struct *thread;

};
struct HCSR_dev *HCSR_devpointer;


struct thread_parms{

	struct HCSR_dev *HCSR_devpointer;
	int write_parameter;

};

void sending_trigger (void *ptr)  // triggers the sensor thus causing ultrasonic sound to be transmitted.
{	struct HCSR_dev *HCSR_devpointer;
	HCSR_devpointer=(struct HCSR_dev *)ptr;
	HCSR_devpointer->trigger_flag=1;
	gpio_set_value_cansleep(HCSR_devpointer->conf.trigPin,0);
	udelay(2);
	gpio_set_value_cansleep(HCSR_devpointer->conf.trigPin,1);
	udelay(10);
	
	gpio_set_value_cansleep(HCSR_devpointer->conf.trigPin,0);
	HCSR_devpointer->trigger_flag=0;
}

void measuring_distance(void *ptr)			// make m+2 measurements every delta milliseconds
{

	static int i;
	static unsigned long long sum=0;
	static unsigned long long maximum=0;
	static unsigned long long minimum= 100000000;

		HCSR_devpointer=(struct HCSR_dev *)ptr;
	//mutex_lock_interruptible(&HCSR_devpointer->device_mutex);

	HCSR_devpointer->mu_flag=1;

	
	for (i=0;i< HCSR_devpointer->conf.m +2 ;i++)
	 {  
	 	sending_trigger((void *)HCSR_devpointer);
	 	
	 	mdelay(1);
	 	
	 	sum+=HCSR_devpointer->distance;
	 	
	 	if(HCSR_devpointer->distance > maximum)
	 		maximum=HCSR_devpointer->distance;
	 	if(HCSR_devpointer->distance<minimum)
	 		minimum=HCSR_devpointer->distance;

	 	HCSR_devpointer->delay_flag=1;
	 	mdelay(HCSR_devpointer->conf.delta);
	HCSR_devpointer->delay_flag=0;
	 }
	 
	 sum = sum - maximum;
	 sum = sum - minimum;
	 do_div(sum,HCSR_devpointer->conf.m);
	 //printk("final value calculated : %llu \n",sum);

		hcsrdist.distancesensed = sum;
		hcsrdist.dist_flag = 1;
	 
	 HCSR_devpointer->mu_flag=0;
     sum=0;

    gpio_set_value_cansleep(HCSR_devpointer->conf.trigPin, 0);
	gpio_set_value_cansleep(HCSR_devpointer->conf.echoPin, 0);
	free_irq(HCSR_devpointer->irq_num,HCSR_devpointer);
	HCSR_devpointer->thr_cr=0;
		// Free resources.
	for (i= 0; i <80; i++)
	{
		if(gpios[i]!=-1)
		gpio_free(gpios[i]);
	}

}


int work_threads(void *ptr_thread){

 	struct thread_parms *write_tf =(struct thread_parms *)ptr_thread;
	struct HCSR_dev *HCSR_devpointer;

	HCSR_devpointer=write_tf->HCSR_devpointer;

	//printk("--------------inside work handler of %s-----------\n",HCSR_devpointer->name);

	if(HCSR_devpointer->mu_flag==1) // if there is a measurement going return EINVAL
		return -EINVAL;
	else 
	{		
		measuring_distance((void *)HCSR_devpointer);
			
	}

	do_exit(0);

}

//Check if the trigger pin and echopin is valid to use or not
int check_valid(int trigger, int echo, int spics){

	if(trigger < 0 || trigger > 19 || trigger == echo || trigger == spics || trigger == 11 || trigger == 13){
		return -EINVAL;
	}
	if(echo < 0 || echo > 19 || echo == trigger || echo == spics || echo == 7 || echo == 8|| echo == 11 || echo == 13){
		return -EINVAL;
	}

	return 0;
}

//The funtion which starts the measurement when requested by user
int measure_the_distance(int beta){


	struct thread_parms *write_tf;
	int trigger_hcsr, echo_hcsr, spi_cs;

	mutex_lock_interruptible(&device_mutex);

	hcsrdist.dist_flag = 0;
	trigger_hcsr = hcsrconfig.trigger_pin;
	echo_hcsr = hcsrconfig.echo_pin;
	spi_cs = hcsrconfig.cspin;

	HCSR_devpointer->conf.trigPin = trigger_hcsr;
	HCSR_devpointer->conf.echoPin = echo_hcsr;

	check_valid(trigger_hcsr, echo_hcsr, spi_cs);

	command_sigs(trigger_hcsr,echo_hcsr, (void *)HCSR_devpointer);

	write_tf = (struct thread_parms *)kmalloc(sizeof(struct thread_parms), GFP_KERNEL);	

	write_tf->HCSR_devpointer=HCSR_devpointer;

	if(HCSR_devpointer->mu_flag==1) // if measurement going return EINVAL
	{
		mutex_unlock(&device_mutex);
		//mutex_unlock(&write_mutex);
		return -EINVAL;
	}
	else 
	{

		HCSR_devpointer->thread=kthread_create(work_threads,(void *)write_tf,"running in background");


		if(HCSR_devpointer->thread) {

			wake_up_process(HCSR_devpointer->thread);

			} 	
		else {

			printk(KERN_ERR "Cannot create kthread\n");

		}

	mutex_unlock(&device_mutex);
	return 0;
	}
}

//irq interrupt handling function
static irq_handler_t handling_irq(unsigned int irq, void *dev_id) // interrupt handler
{
	long long int t;
	int val;
	struct HCSR_dev *HCSR_devpointer;
	HCSR_devpointer=(struct HCSR_dev *)dev_id;
	
	val=gpio_get_value(HCSR_devpointer->conf.echoPin);
	if(val==1)												// at rising edge
	{
	HCSR_devpointer->ref1=ktime_set(0,0);
	HCSR_devpointer->ref1= ktime_get();
	irq_set_irq_type(HCSR_devpointer->irq_num,IRQ_TYPE_EDGE_FALLING);
	}

	else					// at falling edge

	{
		
		HCSR_devpointer->ref2=ktime_set(0,0);				// performing the calculations and fiding the distance.
		HCSR_devpointer->ref2=ktime_get();
		
		t = ktime_to_us(ktime_sub(HCSR_devpointer->ref2,HCSR_devpointer->ref1));  
		HCSR_devpointer->distance = ((t*ultrasonic_speed)/2);
		do_div(HCSR_devpointer->distance,10000);
       
		HCSR_devpointer->data_available=1;
		irq_set_irq_type(HCSR_devpointer->irq_num,IRQ_TYPE_EDGE_RISING);
		
	}

	return (irq_handler_t) IRQ_HANDLED;
	}


//HCSR module init
int driver_initialisation(int beta)   //initilize HCSR driver
{
	int i=0;
	printk("Number of devices : %d\n",num_of_devices);
	
	/* Registering the Miscellaneous drivers */

	HCSR_devpointer = (struct HCSR_dev *)kmalloc(sizeof(struct HCSR_dev), GFP_KERNEL);
	if(!HCSR_devpointer){
		printk("Bad kmalloc for hcsr\n");
		return -ENOMEM;
	}

	sprintf(HCSR_devpointer->name,"HCSR");

	HCSR_devpointer->conf.m = 5;
	HCSR_devpointer->conf.delta = 60;

   
    //initializing HCSR indices
	HCSR_devpointer->head=0;
	HCSR_devpointer->current_pos=0;
	HCSR_devpointer->count=0;
	HCSR_devpointer->distance = 0;
	HCSR_devpointer->trigger_flag=0;    
	HCSR_devpointer->expire_time=5;
	HCSR_devpointer->data_available=0;
	HCSR_devpointer->irq_num=0;
	HCSR_devpointer->delay_flag=1;
	HCSR_devpointer->thr_cr=0;
	
	//mutex_init(&HCSR_devpointer->device_mutex);
	mutex_init(&device_mutex);
	//mutex_init(&write_mutex);

   
	for (i = 0; i <80; i++)
	{
		gpios[i]=-1;
	}

	return 0;
}


/* HCSR Module Exit */
void driver_exiting(int beta)
{
	printk("\n %s is closing\n", HCSR_devpointer->name);

	kfree(HCSR_devpointer);
	printk("HCSR driver removed.\n");

}

MODULE_AUTHOR("Viraj Savaliya");
MODULE_DESCRIPTION("HCSR04 Sensor Driver");
MODULE_LICENSE("GPL v2");


//function the confiure the the gpios of the trigger and echo pins used by user
void command_sigs(int pin_1, int pin_2,void *ptr){  // 1: output, 2:input

    
	struct HCSR_dev *HCSR_devpointer;
	HCSR_devpointer=(struct HCSR_dev *)ptr;
    switch(pin_1){  //trigger, output
        case 0:    
            //printk("trig pin is 0\n");
            gpios[11] = 11;
            gpios[32] = 32;
            if( gpio_request(11, "gpio_out_11") != 0 )  printk("gpio_out_11 error!\n");
            if( gpio_request(32, "dir_out_32") != 0 )  printk("dir_out_32 error!\n");
            HCSR_devpointer->conf.trigPin = 11;
            //printk("HCSR_devpointer->conf.trigPin is:%d \n", HCSR_devpointer->conf.trigPin);
            gpio_direction_output(HCSR_devpointer->conf.trigPin, 0);
            gpio_set_value(32, 0);
            break;
        case 1:
           // printk("trig pin is 1\n");
            gpios[12] = 12;
            gpios[28] = 28;
            gpios[45] = 45;
            if( gpio_request(12, "gpio_out_12") != 0 )  printk("gpio_out_12 error!\n");
            if( gpio_request(28, "dir_out_28") != 0 )  printk("dir_out_28 error!\n");
            if( gpio_request(45, "pin_mux_45") != 0 )  printk("pin_mux_45 error!\n");
            HCSR_devpointer->conf.trigPin = 12;
            //printk("HCSR_devpointer->conf.trigPin is:%d \n", HCSR_devpointer->conf.trigPin);
            gpio_direction_output(HCSR_devpointer->conf.trigPin, 0);
            gpio_set_value_cansleep(28, 0);
            gpio_set_value_cansleep(45, 0);
            break;
        case 2:
            //printk("trig pin is 2\n");
            gpios[13] = 13;
            gpios[34] = 34;
            gpios[77] = 77;
            if( gpio_request(13, "gpio_out_13") != 0 )  printk("gpio_out_13 error!\n");
            if( gpio_request(34, "dir_out_34") != 0 )  printk("dir_out_34 error!\n");
            if( gpio_request(77, "pin_mux_77") != 0 )  printk("pin_mux_77 error!\n");
            HCSR_devpointer->conf.trigPin = 13;
            //printk("HCSR_devpointer->conf.trigPin is:%d \n", HCSR_devpointer->conf.trigPin);
            gpio_direction_output(HCSR_devpointer->conf.trigPin, 0);
            gpio_set_value_cansleep(34, 0);
            gpio_set_value_cansleep(77, 0);
            break;
        case 3:
            //printk("trig pin is 3\n");
            gpios[14] = 14;
            gpios[16]= 16;
            gpios[76]= 76;
            gpios[64]= 64;
            if( gpio_request(14, "gpio_out_14") != 0 )  printk("gpio_out_14 error!\n");
            if( gpio_request(16, "dir_out_16") != 0 )  printk("dir_out_16 error!\n");
            if( gpio_request(76, "pin_mux_76") != 0 )  printk("pin_mux_76 error!\n");
            if( gpio_request(64, "pin_mux_64") != 0 )  printk("pin_mux_64 error!\n");
            HCSR_devpointer->conf.trigPin = 14;
            //printk("HCSR_devpointer->conf.trigPin is:%d \n", HCSR_devpointer->conf.trigPin);
            gpio_direction_output(HCSR_devpointer->conf.trigPin, 0);
            gpio_set_value_cansleep(16, 0);
            gpio_set_value_cansleep(76, 0);
            gpio_set_value_cansleep(64, 0);
            break;
        case 4:
            //printk("trig pin is 4\n");
            gpios[6] = 6;
            gpios[36]= 36;
            if( gpio_request(6, "gpio_out_6") != 0 )  printk("gpio_out_6 error!\n");
            if( gpio_request(36, "dir_out_36") != 0 )  printk("dir_out_36 error!\n");
            HCSR_devpointer->conf.trigPin = 6;
            //printk("HCSR_devpointer->conf.trigPin is:%d \n", HCSR_devpointer->conf.trigPin);
            gpio_direction_output(HCSR_devpointer->conf.trigPin, 0);
            gpio_set_value_cansleep(36, 0);
            break;
        case 5:
            //printk("trig pin is 5\n");
            gpios[0] = 0;
            gpios[18] = 18;
            gpios[66] = 66;
            if( gpio_request(0, "gpio_out_0") != 0 )  printk("gpio_out_0 error!\n");
            if( gpio_request(18, "dir_out_18") != 0 )  printk("dir_out_18 error!\n");
            if( gpio_request(66, "pin_mux_66") != 0 )  printk("pin_mux_66 error!\n");
            HCSR_devpointer->conf.trigPin = 0;
            //printk("HCSR_devpointer->conf.trigPin is:%d \n", HCSR_devpointer->conf.trigPin);
            gpio_direction_output(HCSR_devpointer->conf.trigPin, 0);
            gpio_set_value_cansleep(18, 0);
            gpio_set_value_cansleep(66, 0);
            break;
        case 6:
            //printk("trig pin is 6\n");
            gpios[1] = 1;
            gpios[20] = 20;
            gpios[68] = 68;
            if( gpio_request(1, "gpio_out_1") != 0 )  printk("gpio_out_1 error!\n");
            if( gpio_request(20, "dir_out_20") != 0 )  printk("dir_out_20 error!\n");
            if( gpio_request(68, "pin_mux_68") != 0 )  printk("pin_mux_68 error!\n");
            HCSR_devpointer->conf.trigPin = 1;
            //printk("HCSR_devpointer->conf.trigPin is:%d \n", HCSR_devpointer->conf.trigPin);
            gpio_direction_output(HCSR_devpointer->conf.trigPin, 0);
            gpio_set_value_cansleep(20, 0);
            gpio_set_value_cansleep(68, 0);
            break;
        case 7:
            //printk("trig pin is 7\n");
            gpios[38] = 38;
            if( gpio_request(38, "gpio_out_38") != 0 )  printk("gpio_out_38 error!\n");
            HCSR_devpointer->conf.trigPin = 38;
            //printk("HCSR_devpointer->conf.trigPin is:%d \n", HCSR_devpointer->conf.trigPin);
            gpio_direction_output(HCSR_devpointer->conf.trigPin, 0);
            break;
        case 8:
            //printk("trig pin is 8\n");
            gpios[40] = 40;
            if( gpio_request(40, "gpio_out_40") != 0 )  printk("gpio_out_40 error!\n");
            HCSR_devpointer->conf.trigPin = 40;
            //printk("HCSR_devpointer->conf.trigPin is:%d \n", HCSR_devpointer->conf.trigPin);
            gpio_direction_output(HCSR_devpointer->conf.trigPin, 0);
            break;
 
        case 9:
            //printk("trig pin is 9\n");
            gpios[4] = 4;
            gpios[22] = 22;
            gpios[70] = 70;
            if( gpio_request(4, "gpio_out_4") != 0 )  printk("gpio_out_4 error!\n");
            if( gpio_request(22, "dir_out_22") != 0 )  printk("dir_out_22 error!\n");
            if( gpio_request(70, "pin_mux_70") != 0 )  printk("pin_mux_70 error!\n");
            HCSR_devpointer->conf.trigPin = 4;
            //printk("HCSR_devpointer->conf.trigPin is:%d \n", HCSR_devpointer->conf.trigPin);
            gpio_direction_output(HCSR_devpointer->conf.trigPin, 0);
            gpio_set_value_cansleep(22, 0);
            gpio_set_value_cansleep(70, 0);
            break;
        case 10:
            //printk("trig pin is 10\n");
            gpios[10] = 10;
            gpios[26] = 26;
            gpios[74] = 74;
            if( gpio_request(10, "gpio_out_10") != 0 )  printk("gpio_out_10 error!\n");
            if( gpio_request(26, "dir_out_26") != 0 )  printk("dir_out_26 error!\n");
            if( gpio_request(74, "pin_mux_74") != 0 )  printk("pin_mux_74 error!\n");
            HCSR_devpointer->conf.trigPin = 10;
            //printk("HCSR_devpointer->conf.trigPin is:%d \n", HCSR_devpointer->conf.trigPin);
            gpio_direction_output(HCSR_devpointer->conf.trigPin, 0);
            gpio_set_value_cansleep(26, 0);
            gpio_set_value_cansleep(74, 0);
            break;
        case 11:
            //printk("trig pin is 11\n");
            gpios[5] = 5;
            gpios[24] = 24;
            gpios[44] = 44;
            gpios[72] = 72;
            if( gpio_request(5, "gpio_out_5") != 0 )  printk("gpio_out_5 error!\n");
            if( gpio_request(24, "dir_out_24") != 0 )  printk("dir_out_24 error!\n");
            if( gpio_request(44, "pin_mux_44") != 0 )  printk("pin_mux_44 error!\n");
            if( gpio_request(72, "pin_mux_72") != 0 )  printk("pin_mux_72 error!\n");
            HCSR_devpointer->conf.trigPin = 5;
            //printk("HCSR_devpointer->conf.trigPin is:%d \n", HCSR_devpointer->conf.trigPin);
            gpio_direction_output(HCSR_devpointer->conf.trigPin, 0);
            gpio_set_value_cansleep(24, 0);
            gpio_set_value_cansleep(44, 0);
            gpio_set_value_cansleep(72, 0);
            break;
        case 12:
            //printk("trig pin is 12\n");
            gpios[15] = 15;
            gpios[42] = 42;
            if( gpio_request(15, "gpio_out_15") != 0 )  printk("gpio_out_15 error!\n");
            if( gpio_request(42, "dir_out_42") != 0 )  printk("dir_out_42 error!\n");
            HCSR_devpointer->conf.trigPin = 15;
            //printk("HCSR_devpointer->conf.trigPin is:%d \n", HCSR_devpointer->conf.trigPin);
            gpio_direction_output(HCSR_devpointer->conf.trigPin, 0);
            gpio_set_value_cansleep(42, 0);
            break;
        case 13:
            //printk("trig pin is 13\n");
            gpios[7] = 7;
            gpios[30] = 30;
            gpios[46] = 46;
            if( gpio_request(7, "gpio_out_7") != 0 )  printk("gpio_out_7 error!\n");
            if( gpio_request(30, "dir_out_30") != 0 )  printk("dir_out_30 error!\n");
            if( gpio_request(46, "pin_mux_46") != 0 )  printk("pin_mux_46 error!\n");
            HCSR_devpointer->conf.trigPin = 7;
            //printk("HCSR_devpointer->conf.trigPin is:%d \n", HCSR_devpointer->conf.trigPin);
            gpio_direction_output(HCSR_devpointer->conf.trigPin, 0);
            gpio_set_value_cansleep(30, 0);
            gpio_set_value_cansleep(46, 0);
            break;
        case 14:
            //printk("trig pin is 14\n");
            gpios[48] = 48;
            if( gpio_request(48, "gpio_out_48") != 0 )  printk("gpio_out_48 error!\n");
            HCSR_devpointer->conf.trigPin = 48;
            //printk("HCSR_devpointer->conf.trigPin is:%d \n", HCSR_devpointer->conf.trigPin);
            gpio_direction_output(HCSR_devpointer->conf.trigPin, 0);
            break;
        case 15:
            //printk("trig pin is 15\n");
            gpios[50] = 50;
            if( gpio_request(50, "gpio_out_50") != 0 )  printk("gpio_out_50 error!\n");
            HCSR_devpointer->conf.trigPin = 50;
            //printk("HCSR_devpointer->conf.trigPin is:%d \n", HCSR_devpointer->conf.trigPin);
            gpio_direction_output(HCSR_devpointer->conf.trigPin, 0);
            break;
        case 16:
            //printk("trig pin is 16\n");
            gpios[52] = 52;
            if( gpio_request(52, "gpio_out_52") != 0 )  printk("gpio_out_52 error!\n");
            HCSR_devpointer->conf.trigPin = 52;
            //printk("HCSR_devpointer->conf.trigPin is:%d \n", HCSR_devpointer->conf.trigPin);
            gpio_direction_output(HCSR_devpointer->conf.trigPin, 0);
            break;
        case 17:
            //printk("trig pin is 17\n");
            gpios[54] = 54;
            if( gpio_request(54, "gpio_out_54") != 0 )  printk("gpio_out_54 error!\n");
            HCSR_devpointer->conf.trigPin = 54;
            //printk("HCSR_devpointer->conf.trigPin is:%d \n", HCSR_devpointer->conf.trigPin);
            gpio_direction_output(HCSR_devpointer->conf.trigPin, 0);
            break;
        case 18:
            //printk("trig pin is 18\n");
            gpios[56] = 56;
            gpios[60] = 60;
            gpios[78] = 78;
            if( gpio_request(56, "gpio_out_56") != 0 )  printk("gpio_out_56 error!\n");
            if( gpio_request(60, "pin_mux_60") != 0 )  printk("pin_mux_60 error!\n");
            if( gpio_request(78, "pin_mux_78") != 0 )  printk("pin_mux_78 error!\n");
            HCSR_devpointer->conf.trigPin = 56;
            //printk("HCSR_devpointer->conf.trigPin is:%d \n", HCSR_devpointer->conf.trigPin);
            gpio_direction_output(HCSR_devpointer->conf.trigPin, 0);
            gpio_set_value_cansleep(60, 1);
            gpio_set_value_cansleep(78, 1);
            break;
        case 19:
            //printk("trig pin is 19\n");
            gpios[58] = 58;
            gpios[60] = 60;
            gpios[79] = 79;
            if( gpio_request(58, "gpio_out_58") != 0 )  printk("gpio_out_58 error!\n");
            if( gpio_request(60, "pin_mux_60") != 0 )  printk("pin_mux_60 error!\n");
            if( gpio_request(79, "pin_mux_79") != 0 )  printk("pin_mux_79 error!\n");
            HCSR_devpointer->conf.trigPin = 58;
            //printk("HCSR_devpointer->conf.trigPin is:%d \n", HCSR_devpointer->conf.trigPin);
            gpio_direction_output(HCSR_devpointer->conf.trigPin, 0);
            gpio_set_value_cansleep(60, 1);
            gpio_set_value_cansleep(79, 1);
            break;
    }
 
    switch(pin_2){  //echo, input
    	int ret;
        case 0:    
            gpios[11] = 11;
            gpios[32] = 32;
            if( gpio_request(11, "gpio_in_11") != 0 )  printk("gpio_in_11 error!\n");
            if( gpio_request(32, "dir_in_32") != 0 )  printk("dir_in_32 error!\n");
            HCSR_devpointer->irq_num = gpio_to_irq(gpios[11]);
            if(HCSR_devpointer->irq_num<0)  printk("IRQ NUMBER ERROR\n");
            ret = request_irq(HCSR_devpointer->irq_num,(irq_handler_t) handling_irq, IRQF_TRIGGER_RISING, "rise",(void*)HCSR_devpointer);
            if (ret < 0)   printk("Error in request_irq\n");
            HCSR_devpointer->conf.echoPin = 11;
            gpio_direction_input(HCSR_devpointer->conf.echoPin);
            gpio_set_value_cansleep(32,1);
            break;
        case 1:
            gpios[12] = 12;
            gpios[28] = 28;
            gpios[45] = 45;
            if( gpio_request(12, "gpio_in_12") != 0 )  printk("gpio_in_12 error!\n");
            if( gpio_request(28, "dir_in_28") != 0 )  printk("dir_in_28 error!\n");
            if( gpio_request(45, "pin_mux_45") != 0 )  printk("pin_mux_45 error!\n");
            HCSR_devpointer->irq_num = gpio_to_irq(gpios[12]);
            if(HCSR_devpointer->irq_num<0)  printk("IRQ NUMBER ERROR\n");
            ret = request_irq(HCSR_devpointer->irq_num,(irq_handler_t) handling_irq, IRQF_TRIGGER_RISING, "rise", (void*)HCSR_devpointer);
            if (ret < 0)   printk("Error in request_irq\n");
            HCSR_devpointer->conf.echoPin = 12;
            gpio_direction_input(HCSR_devpointer->conf.echoPin);
            gpio_set_value_cansleep(28,1);
            gpio_set_value_cansleep(45, 0);
            break;
        case 2:
            gpios[13] = 13;
            gpios[34] = 34;
            gpios[77] = 77;
            if( gpio_request(13, "gpio_in_13") != 0 )  printk("gpio_in_13 error!\n");
            if( gpio_request(34, "dir_in_34") != 0 )  printk("dir_in_34 error!\n");
            if( gpio_request(77, "pin_mux_77") != 0 )  printk("pin_mux_77 error!\n");
            HCSR_devpointer->irq_num = gpio_to_irq(gpios[13]);
            if(HCSR_devpointer->irq_num<0)  printk("IRQ NUMBER ERROR\n");
            ret = request_irq(HCSR_devpointer->irq_num,(irq_handler_t) handling_irq, IRQF_TRIGGER_RISING, "rise", (void*)HCSR_devpointer);
            if (ret < 0)   printk("Error in request_irq\n");
            HCSR_devpointer->conf.echoPin = 13;
            gpio_direction_input(HCSR_devpointer->conf.echoPin);
            gpio_set_value_cansleep(34,1);
            gpio_set_value_cansleep(77, 0);
            break;
        case 3:
            gpios[14] = 14;
            gpios[16] = 16;
            gpios[76] = 76;
            gpios[64] = 64;
            if( gpio_request(14, "gpio_in_14") != 0 )  printk("gpio_in_14 error!\n");
            if( gpio_request(16, "dir_in_16") != 0 )  printk("dir_in_16 error!\n");
            if( gpio_request(76, "pin_mux_76") != 0 )  printk("pin_mux_76 error!\n");
            if( gpio_request(64, "pin_mux_64") != 0 )  printk("pin_mux_64 error!\n");
            HCSR_devpointer->irq_num = gpio_to_irq(gpios[14]);
            if(HCSR_devpointer->irq_num<0)  printk("IRQ NUMBER ERROR\n");
            ret = request_irq(HCSR_devpointer->irq_num, (irq_handler_t) handling_irq, IRQF_TRIGGER_RISING, "rise", (void*)HCSR_devpointer);
            if (ret < 0)   printk("Error in request_irq\n");
            HCSR_devpointer->conf.echoPin = 14;
            gpio_direction_input(HCSR_devpointer->conf.echoPin);
            gpio_set_value_cansleep(16,1);
            gpio_set_value_cansleep(76, 0);
            gpio_set_value_cansleep(64, 0);
            break;
        case 4:
        	gpios[6] = 6;
            if( gpio_request(6, "gpio_in_6") != 0 )  printk("gpio_in_6 error!\n");
            HCSR_devpointer->irq_num= gpio_to_irq(gpios[6]);
			if(HCSR_devpointer->irq_num<0)
			{printk("IRQ NUMBER ERROR\n");}
			ret = request_irq(HCSR_devpointer->irq_num,(irq_handler_t) handling_irq, IRQF_TRIGGER_RISING, "rise", (void*)HCSR_devpointer);
			if (ret < 0) 
			{printk("Error in request_irq\n");}
            HCSR_devpointer->conf.echoPin = 6;
            gpio_direction_input(HCSR_devpointer->conf.echoPin);

            break;
        case 5:
            gpios[0] = 0;
            gpios[18] = 18;
            gpios[66] = 66;
            if( gpio_request(0, "gpio_in_0") != 0 )  printk("gpio_in_0 error!\n");
            if( gpio_request(18, "dir_in_18") != 0 )  printk("dir_in_18 error!\n");
            if( gpio_request(66, "pin_mux_66") != 0 )  printk("pin_mux_66 error!\n");
            HCSR_devpointer->irq_num = gpio_to_irq(gpios[0]);
            if(HCSR_devpointer->irq_num<0)  printk("IRQ NUMBER ERROR\n");
            ret = request_irq(HCSR_devpointer->irq_num,(irq_handler_t) handling_irq, IRQF_TRIGGER_RISING, "rise", (void*)HCSR_devpointer);
            if (ret < 0)   printk("Error in request_irq\n");
            HCSR_devpointer->conf.echoPin = 0;
            gpio_direction_input(HCSR_devpointer->conf.echoPin);
            gpio_set_value_cansleep(18, 1);
            gpio_set_value_cansleep(66, 0);
            break;
        case 6:
            gpios[1] = 1;
            gpios[20] = 20;
            gpios[68] = 68;
            if( gpio_request(1, "gpio_in_1") != 0 )  printk("gpio_in_1 error!\n");
            if( gpio_request(20, "dir_in_20") != 0 )  printk("dir_in_20 error!\n");
            if( gpio_request(68, "pin_mux_68") != 0 )  printk("pin_mux_68 error!\n");
            HCSR_devpointer->irq_num = gpio_to_irq(gpios[1]);
            if(HCSR_devpointer->irq_num<0)  printk("IRQ NUMBER ERROR\n");
            ret = request_irq(HCSR_devpointer->irq_num,(irq_handler_t) handling_irq, IRQF_TRIGGER_RISING, "rise", (void*)HCSR_devpointer);
            if (ret < 0)   printk("Error in request_irq\n");
            HCSR_devpointer->conf.echoPin = 1;
            gpio_direction_input(HCSR_devpointer->conf.echoPin);
            gpio_set_value_cansleep(20,1);
            gpio_set_value_cansleep(68, 0);
            break;
        case 7:
            gpios[38] = 38;
            if( gpio_request(38, "gpio_in_38") != 0 )  printk("gpio_in_38 error!\n");
            HCSR_devpointer->irq_num = gpio_to_irq(gpios[38]);
            if(HCSR_devpointer->irq_num<0)  printk("IRQ NUMBER ERROR\n");
            ret = request_irq(HCSR_devpointer->irq_num,(irq_handler_t) handling_irq, IRQF_TRIGGER_RISING, "rise", (void*)HCSR_devpointer);
            if (ret < 0)   printk("Error in request_irq\n");
            HCSR_devpointer->conf.echoPin = 38;
            gpio_direction_input(HCSR_devpointer->conf.echoPin);
            break;
        case 8:
            gpios[40] = 40;
            if( gpio_request(40, "gpio_in_40") != 0 )  printk("gpio_in_40 error!\n");
            HCSR_devpointer->irq_num = gpio_to_irq(gpios[40]);
            if(HCSR_devpointer->irq_num<0)  printk("IRQ NUMBER ERROR\n");
            ret = request_irq(HCSR_devpointer->irq_num,(irq_handler_t) handling_irq, IRQF_TRIGGER_RISING, "rise", (void*)HCSR_devpointer);
            if (ret < 0)   printk("Error in request_irq\n");
            gpio_direction_input(HCSR_devpointer->conf.echoPin);
            HCSR_devpointer->conf.echoPin = 40;
            break;
        case 9:
            gpios[4] = 4;
            gpios[22] = 22;
            gpios[70] = 70;
            if( gpio_request(4, "gpio_in_4") != 0 )  printk("gpio_in_4 error!\n");
            if( gpio_request(22, "dir_in_22") != 0 )  printk("dir_in_22 error!\n");
            if( gpio_request(70, "pin_mux_70") != 0 )  printk("pin_mux_70 error!\n");
            HCSR_devpointer->irq_num = gpio_to_irq(gpios[4]);
            if(HCSR_devpointer->irq_num<0)  printk("IRQ NUMBER ERROR\n");
            ret = request_irq(HCSR_devpointer->irq_num,(irq_handler_t) handling_irq, IRQF_TRIGGER_RISING, "rise", (void*)HCSR_devpointer);
            if (ret < 0)   printk("Error in request_irq\n");
            HCSR_devpointer->conf.echoPin = 4;
            gpio_direction_input(HCSR_devpointer->conf.echoPin);
            gpio_set_value_cansleep(22,1);
            gpio_set_value_cansleep(70, 0);
            break;
        case 10:
            gpios[10] = 10;
            gpios[26] = 26;
            gpios[74] = 74;
            if( gpio_request(10, "gpio_in_10") != 0 )  printk("gpio_in_10 error!\n");
            if( gpio_request(26, "dir_in_26") != 0 )  printk("dir_in_26 error!\n");
            if( gpio_request(74, "pin_mux_74") != 0 )  printk("pin_mux_74 error!\n");
            HCSR_devpointer->irq_num = gpio_to_irq(gpios[10]);
            if(HCSR_devpointer->irq_num<0)  printk("IRQ NUMBER ERROR\n");
            ret = request_irq(HCSR_devpointer->irq_num, (irq_handler_t) handling_irq, IRQF_TRIGGER_RISING, "rise", (void*)HCSR_devpointer);
            if (ret < 0)   printk("Error in request_irq\n");
            HCSR_devpointer->conf.echoPin = 10;
            gpio_direction_input(HCSR_devpointer->conf.echoPin);
            gpio_set_value_cansleep(26,1);
            gpio_set_value_cansleep(74, 0);
            break;
        case 11:
            //printk("echo pin is 11\n");
            gpios[5] = 5;
            gpios[24] = 24;
            gpios[44] = 44;
            gpios[72] = 72;
            if( gpio_request(5, "gpio_in_5") != 0 )  printk("gpio_in_5 error!\n");
            if( gpio_request(24, "dir_in_24") != 0 )  printk("dir_in_24 error!\n");
            if( gpio_request(44, "pin_mux_44") != 0 )  printk("pin_mux_44 error!\n");
            if( gpio_request(72, "pin_mux_72") != 0 )  printk("pin_mux_72 error!\n");
            HCSR_devpointer->irq_num = gpio_to_irq(gpios[5]);
            if(HCSR_devpointer->irq_num<0)  printk("IRQ NUMBER ERROR\n");
            ret = request_irq(HCSR_devpointer->irq_num,(irq_handler_t) handling_irq, IRQF_TRIGGER_RISING, "rise", (void*)HCSR_devpointer);
            if (ret < 0)   printk("Error in request_irq\n");
            HCSR_devpointer->conf.echoPin = 5;
            gpio_direction_input(HCSR_devpointer->conf.echoPin);
            gpio_set_value_cansleep(24,1);
            gpio_set_value_cansleep(44, 0);
            gpio_set_value_cansleep(72, 0);
            break;
        case 12:
            gpios[15] = 15;
            gpios[42] = 42;
            if( gpio_request(15, "gpio_in_15") != 0 )  printk("gpio_in_15 error!\n");
            if( gpio_request(42, "dir_in_42") != 0 )  printk("dir_in_42 error!\n");
            HCSR_devpointer->irq_num = gpio_to_irq(gpios[15]);
            if(HCSR_devpointer->irq_num<0)  printk("IRQ NUMBER ERROR\n");
            ret = request_irq(HCSR_devpointer->irq_num, (irq_handler_t) handling_irq, IRQF_TRIGGER_RISING, "rise", (void*)HCSR_devpointer);
            if (ret < 0)   printk("Error in request_irq\n");
            HCSR_devpointer->conf.echoPin = 15;
            gpio_direction_input(HCSR_devpointer->conf.echoPin);
            gpio_set_value_cansleep(42,1);
            break;
        case 13:
            gpios[7] = 7;
            gpios[30] = 30;
            gpios[46] = 46;
            if( gpio_request(7, "gpio_in_7") != 0 )  printk("gpio_in_7 error!\n");
            if( gpio_request(30, "dir_in_30") != 0 )  printk("dir_in_30 error!\n");
            if( gpio_request(46, "pin_mux_46") != 0 )  printk("pin_mux_46 error!\n");
            HCSR_devpointer->irq_num = gpio_to_irq(gpios[7]);
            if(HCSR_devpointer->irq_num<0)  printk("IRQ NUMBER ERROR\n");
            ret = request_irq(HCSR_devpointer->irq_num,(irq_handler_t) handling_irq, IRQF_TRIGGER_RISING, "rise", (void*)HCSR_devpointer);
            if (ret < 0)   printk("Error in request_irq\n");
            HCSR_devpointer->conf.echoPin = 7;
            gpio_direction_input(HCSR_devpointer->conf.echoPin);
            gpio_set_value_cansleep(30,1 );
            gpio_set_value_cansleep(46, 0);
            break;
        case 14:
            gpios[48] = 48;
            if( gpio_request(48, "gpio_in_48") != 0 )  printk("gpio_in_48 error!\n");
            HCSR_devpointer->irq_num = gpio_to_irq(gpios[48]);
            if(HCSR_devpointer->irq_num<0)  printk("IRQ NUMBER ERROR\n");
            ret = request_irq(HCSR_devpointer->irq_num, (irq_handler_t) handling_irq, IRQF_TRIGGER_RISING, "rise", (void*)HCSR_devpointer);
            if (ret < 0)   printk("Error in request_irq\n");
            HCSR_devpointer->conf.echoPin = 48;
            gpio_direction_input(HCSR_devpointer->conf.echoPin);
            break;
        case 15:
            gpios[50] = 50;
            if( gpio_request(50, "gpio_in_50") != 0 )  printk("gpio_in_50 error!\n");
            HCSR_devpointer->irq_num = gpio_to_irq(gpios[50]);
            if(HCSR_devpointer->irq_num<0)  printk("IRQ NUMBER ERROR\n");
            ret = request_irq(HCSR_devpointer->irq_num,(irq_handler_t) handling_irq, IRQF_TRIGGER_RISING, "rise", (void*)HCSR_devpointer);
            if (ret < 0)   printk("Error in request_irq\n");
            HCSR_devpointer->conf.echoPin = 50;
            gpio_direction_input(HCSR_devpointer->conf.echoPin);
            break;
        case 16:
            gpios[52] = 52;
            if( gpio_request(52, "gpio_in_52") != 0 )  printk("gpio_in_52 error!\n");
            HCSR_devpointer->irq_num = gpio_to_irq(gpios[52]);
            if(HCSR_devpointer->irq_num<0)  printk("IRQ NUMBER ERROR\n");
            ret = request_irq(HCSR_devpointer->irq_num,(irq_handler_t) handling_irq, IRQF_TRIGGER_RISING, "rise", (void*)HCSR_devpointer);
            if (ret < 0)   printk("Error in request_irq\n");
            HCSR_devpointer->conf.echoPin = 52;
            gpio_direction_input(HCSR_devpointer->conf.echoPin);
            break;
        case 17:
            gpios[54] = 54;
            if( gpio_request(54, "gpio_in_54") != 0 )  printk("gpio_in_54 error!\n");
            HCSR_devpointer->irq_num = gpio_to_irq(gpios[54]);
            if(HCSR_devpointer->irq_num<0)  printk("IRQ NUMBER ERROR\n");
            ret = request_irq(HCSR_devpointer->irq_num,(irq_handler_t) handling_irq, IRQF_TRIGGER_RISING, "rise", (void*)HCSR_devpointer);
            if (ret < 0)   printk("Error in request_irq\n");
            HCSR_devpointer->conf.echoPin = 54;
            gpio_direction_input(HCSR_devpointer->conf.echoPin);
            break;
        case 18:
            gpios[56] = 56;
            gpios[60] = 60;
            gpios[78] = 78;
            if( gpio_request(56, "gpio_in_56") != 0 )  printk("gpio_in_56 error!\n");
            if( gpio_request(60, "pin_mux_60") != 0 )  printk("pin_mux_60 error!\n");
            if( gpio_request(78, "pin_mux_78") != 0 )  printk("pin_mux_78 error!\n");
            HCSR_devpointer->irq_num = gpio_to_irq(gpios[56]);
            if(HCSR_devpointer->irq_num<0)  printk("IRQ NUMBER ERROR\n");
            ret = request_irq(HCSR_devpointer->irq_num,(irq_handler_t) handling_irq, IRQF_TRIGGER_RISING, "rise", (void*)HCSR_devpointer);
            if (ret < 0)   printk("Error in request_irq\n");
            HCSR_devpointer->conf.echoPin = 56;
            gpio_direction_input(HCSR_devpointer->conf.echoPin);
            gpio_set_value_cansleep(60, 1);
            gpio_set_value_cansleep(78, 1);
            break;
        case 19:
            gpios[58] = 58;
            gpios[60] = 60;
            gpios[79] = 79;
            if( gpio_request(58, "gpio_in_58") != 0 )  printk("gpio_in_58 error!\n");
            if( gpio_request(60, "pin_mux_60") != 0 )  printk("pin_mux_60 error!\n");
            if( gpio_request(79, "pin_mux_79") != 0 )  printk("pin_mux_79 error!\n");
            HCSR_devpointer->irq_num = gpio_to_irq(gpios[58]);
            if(HCSR_devpointer->irq_num<0)  printk("IRQ NUMBER ERROR\n");
            ret = request_irq(HCSR_devpointer->irq_num,(irq_handler_t) handling_irq, IRQF_TRIGGER_RISING, "rise", (void*)HCSR_devpointer);
            if (ret < 0)   printk("Error in request_irq\n");
            HCSR_devpointer->conf.echoPin = 58;
            gpio_direction_input(HCSR_devpointer->conf.echoPin);
            gpio_set_value_cansleep(60, 1);
            gpio_set_value_cansleep(79, 1);
            break;
    }
}
