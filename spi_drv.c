#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/init.h>
#include "spihead.h"


struct spidev_data {
	struct spi_device	*spi_matrix;
	char user_pattern[4][8];
};

struct mutex spi_device_mutex;


//extern Response *r1;
extern struct data_dotmat ddm_info;
extern struct data_pins dattconfig;

static struct spidev_data *spidevp;

static unsigned char xfer_tx[2]={0};


void commmand_cs (int pin);
void commmand_clear (int pin);
void command_cs_1(int pin);
void command_cs_0(int pin);
static int spidev_over(int spi_cs_pin);

static void spidev_transfer(unsigned char ch1, unsigned char ch2)           //funtion to transfer data to led matrix
{
    int cs_pin;
    xfer_tx[0] = ch1;       //transfer row
    xfer_tx[1] = ch2;       //transfer column

    cs_pin = dattconfig.cspin;
    command_cs_0(cs_pin);
    spi_write(spidevp->spi_matrix, &xfer_tx, sizeof(xfer_tx));
    command_cs_1(cs_pin);
    return;
}

//pattern transfer thread
int spidev_pattern_thread(void *data){


	int cs_pin;
	int i=0, j=0, k=0;
	unsigned int row[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

	cs_pin = dattconfig.cspin;

	for(i=0;i<4;i++)
			{
				for(j=0;j<8;j++)
				{
				spidevp->user_pattern[i][j] = ddm_info.ledpatt[i][j];
				}
			}

    //while(1){
	for (j=0; j<1; j++){
		for(i=0;i<4;i++)
		{
					k=0;
					while(k<8)
					{
						spidev_transfer(row[k], spidevp->user_pattern[i][k]);
						k++;
					}
					msleep(400);
		}
	}

	spidev_over(cs_pin);

	do_exit(0);
	return 0;
}

//Initial setting of the led matrix when data comes
static int spidev_start(int spi_cs_pin){

	int i=0;

	commmand_cs(spi_cs_pin);

	spidev_transfer(0x0F, 0x00);
	spidev_transfer(0x0C, 0x01);
	spidev_transfer(0x0B, 0x07);
	spidev_transfer(0x09, 0x00);
	spidev_transfer(0x0A, 0x02);	

	for(i=1; i < 9; i++)
	{
		spidev_transfer(i, 0x00);
	}

	return 0;
}

//Setting of led matrix after transfer is done
static int spidev_over(int spi_cs_pin){

	int i=0;

	for(i=1; i < 9; i++)
	{
		spidev_transfer(i, 0x00);
	}
	
	gpio_free(44);
	gpio_free(72);
	gpio_free(46);
	gpio_free(24);
	//gpio_free(42);
	gpio_free(30);
	//gpio_free(15);
	commmand_clear(spi_cs_pin);

	//printk("Spidev LED transfer ended\n");

	return 0;
}

//checking if CS pin is valid to be used or not
int check_pin_valid(int spi_cs){

	if(spi_cs < 0 || spi_cs > 19 || spi_cs == 11 || spi_cs == 13 || 
		spi_cs == dattconfig.trigg || spi_cs == dattconfig.ech) {
		return -EINVAL;
	}
	return 0;
}

//Fauntion to send the pattern coming from user to led matrix
int send_pattern(int alpha){

	int cs_pin;
	struct task_struct *task;

	mutex_lock_interruptible(&spi_device_mutex);

	cs_pin = dattconfig.cspin;

	check_pin_valid(cs_pin);

	//spidev_over(cs_pin);
	spidev_start(cs_pin);
	
	task = kthread_run(&spidev_pattern_thread, (void *)NULL, "spidev_pattern_kthread");        //creating thread to tranfer the data

	mutex_unlock(&spi_device_mutex);
	return 0;

}


//Initialising SPI module
int spidev_init(int alpha)
{
	int ret;
    struct spi_master *master;

    //Information for slave device
    struct spi_board_info spi_device_info = {
        .modalias = "SPI_DOTMAT",
        .max_speed_hz = 500000,
        .bus_num = 1,
        .chip_select = 1,
        .mode = 3,
    };

    //memory allocation for device
    spidevp = kzalloc(sizeof(*spidevp), GFP_KERNEL);
    if(!spidevp){
    	return -ENOMEM;
    }
    
    
    //allocating master for spi device
    master = spi_busnum_to_master( spi_device_info.bus_num );
    if( !master ){
        printk("MASTER not found.\n");
            return -ENODEV;
    }
     
    //creating slave device for the master
    spidevp->spi_matrix = spi_new_device( master, &spi_device_info );
 
    if( !spidevp->spi_matrix ) {
        printk("FAILED to create slave.\n");
        return -ENODEV;
    }
     
    spidevp->spi_matrix->bits_per_word = 8;
 
    //setting up the slave device with data
    ret = spi_setup( spidevp->spi_matrix );
     
    if( ret ){
        printk("FAILED to setup slave.\n");
        spi_unregister_device( spidevp->spi_matrix );
        return -ENODEV;
    }

    mutex_init(&spi_device_mutex);

	//Freeing gpio pins of Din and CLK initially for use later
    gpio_free(24);
	gpio_free(44);
	gpio_free(72);
	gpio_free(46);
	gpio_free(30);
    //gpio_free(42);
	//gpio_free(15);

	//requesting the required gpio for Din and CLK
    gpio_request_one(24, GPIOF_DIR_OUT, "MOSI_SHIFT");
	gpio_request_one(44, GPIOF_DIR_OUT, "MOSI_MUX1");
	gpio_request_one(72, GPIOF_OUT_INIT_LOW, "MOSI_MUX2");
	gpio_request_one(46, GPIOF_DIR_OUT, "SPI_SCK");
	gpio_request_one(30, GPIOF_DIR_OUT, "SCK_SHIFT");
    //gpio_request_one(42, GPIOF_DIR_OUT, "SS_SHIFT");
	//gpio_request_one(15, GPIOF_DIR_OUT, "SS_PIN");

	//Initialising Din adn CLK pins
    gpio_set_value_cansleep(24, 0);
	gpio_set_value_cansleep(44, 1);
	gpio_set_value_cansleep(72, 0);
	gpio_set_value_cansleep(46, 1);
	gpio_set_value_cansleep(30, 0);
    //gpio_set_value_cansleep(42, 0);
	//gpio_set_value_cansleep(15, 1);

	printk("Spidev LED driver Initialized\n");
	return ret;
}

//Exiting funtion for SPI module
void spidev_exit(int alpha)
{
	spi_unregister_device(spidevp->spi_matrix);
	printk("Spidev LED driver Uninitialiased...\n");
}

MODULE_AUTHOR("Viraj Savaliya");
MODULE_DESCRIPTION("SPI LED Driver");
MODULE_LICENSE("GPL");

//function to initialise the CS pin as per which user is using
void commmand_cs (int pin){

	switch(pin){
        case 0:
        	gpio_free(11);
        	gpio_free(32);    
            gpio_request_one(11,GPIOF_DIR_OUT, "gpio_out_11");
            gpio_request_one(32,GPIOF_DIR_OUT, "dir_out_32");
            gpio_set_value_cansleep(11, 1);
            gpio_set_value_cansleep(32, 0);
            break;
        case 1:
            gpio_free(12);
            gpio_free(28);
            gpio_free(45);
            gpio_request_one(12,GPIOF_DIR_OUT, "gpio_out_12");
            gpio_request_one(28,GPIOF_DIR_OUT, "dir_out_28");
            gpio_request_one(45,GPIOF_DIR_OUT, "pin_mux_45");
            gpio_set_value_cansleep(12, 1);
            gpio_set_value_cansleep(28, 0);
            gpio_set_value_cansleep(45, 0);
            break;
        case 2:
            gpio_free(13);
            gpio_free(34);
            gpio_free(77);
            gpio_request_one(13,GPIOF_DIR_OUT, "gpio_out_13");
            gpio_request_one(34,GPIOF_DIR_OUT, "dir_out_34");
            gpio_request_one(77,GPIOF_DIR_OUT, "pin_mux_77");
            gpio_set_value_cansleep(13, 1);
            gpio_set_value_cansleep(34, 0);
            gpio_set_value_cansleep(77, 0);
            break;
        case 3:
            gpio_free(14);
            gpio_free(16);
            gpio_free(76);
            gpio_free(64);
            gpio_request_one(14,GPIOF_DIR_OUT, "gpio_out_14");
            gpio_request_one(16,GPIOF_DIR_OUT, "dir_out_16");
            gpio_request_one(76,GPIOF_DIR_OUT, "pin_mux_76");
            gpio_request_one(64,GPIOF_DIR_OUT, "pin_mux_64");
            gpio_set_value_cansleep(14, 1);
            gpio_set_value_cansleep(16, 0);
            gpio_set_value_cansleep(76, 0);
            gpio_set_value_cansleep(64, 0);
            break;
        case 4:
            gpio_free(6);
            gpio_free(36);
            gpio_request_one(6,GPIOF_DIR_OUT, "gpio_out_6");
            gpio_request_one(36,GPIOF_DIR_OUT, "dir_out_36");
            gpio_set_value_cansleep(6, 1);
            gpio_set_value_cansleep(36, 0);
            break;
        case 5:
            gpio_free(0);
            gpio_free(18);
            gpio_free(66);
            gpio_request_one(0,GPIOF_DIR_OUT, "gpio_out_0");
            gpio_request_one(18,GPIOF_DIR_OUT, "dir_out_18");
            gpio_request_one(66,GPIOF_DIR_OUT,"pin_mux_66");
            gpio_set_value_cansleep(0, 1);
            gpio_set_value_cansleep(18, 0);
            gpio_set_value_cansleep(66, 0);
            break;
        case 6:
            gpio_free(1);
            gpio_free(20);
            gpio_free(68);
            gpio_request_one(1,GPIOF_DIR_OUT, "gpio_out_1");
            gpio_request_one(20,GPIOF_DIR_OUT, "dir_out_20");
            gpio_request_one(68,GPIOF_DIR_OUT, "pin_mux_68");
            gpio_set_value_cansleep(1, 1);
            gpio_set_value_cansleep(20, 0);
            gpio_set_value_cansleep(68, 0);
            break;
        case 7:
            gpio_free(38);
            gpio_request_one(38,GPIOF_DIR_OUT, "gpio_out_38");
            gpio_set_value_cansleep(38, 1);
            break;
        case 8:
            gpio_free(40);
            gpio_request_one(40,GPIOF_DIR_OUT, "gpio_out_40");
            gpio_set_value_cansleep(40, 1);
            break;
        case 9:
            gpio_free(4);
            gpio_free(22);
            gpio_free(70);
            gpio_request_one(4,GPIOF_DIR_OUT, "gpio_out_4");
            gpio_request_one(22,GPIOF_DIR_OUT, "dir_out_22");
            gpio_request_one(70,GPIOF_DIR_OUT, "pin_mux_70");
            gpio_set_value_cansleep(4, 1);
            gpio_set_value_cansleep(22, 0);
            gpio_set_value_cansleep(70, 0);
            break;
        case 10:
            gpio_free(10);
            gpio_free(26);
            gpio_free(74);
            gpio_request_one(10,GPIOF_DIR_OUT, "gpio_out_10");
            gpio_request_one(26,GPIOF_DIR_OUT, "dir_out_26");
            gpio_request_one(74,GPIOF_DIR_OUT, "pin_mux_74");
            gpio_set_value_cansleep(10, 1);
            gpio_set_value_cansleep(26, 0);
            gpio_set_value_cansleep(74, 0);
            break;
        case 12:
            gpio_free(15);
            gpio_free(42);
            gpio_request_one(42, GPIOF_DIR_OUT, "SS_SHIFT");
            gpio_request_one(15, GPIOF_DIR_OUT, "SS_PIN");
            gpio_set_value_cansleep(15, 1);
            gpio_set_value_cansleep(42, 0);
            break;
        case 14:
            gpio_free(48);
            gpio_request_one(48,GPIOF_DIR_OUT, "gpio_out_48");
            gpio_set_value_cansleep(48, 1);
            break;
        case 15:
            gpio_free(50);
            gpio_request_one(50,GPIOF_DIR_OUT, "gpio_out_50");
            gpio_set_value_cansleep(50, 1);
            break;
        case 16:
            gpio_free(52);
            gpio_request_one(52,GPIOF_DIR_OUT, "gpio_out_52");
            gpio_set_value_cansleep(52, 1);
            break;
        case 17:
            gpio_free(54);
            gpio_request_one(54,GPIOF_DIR_OUT, "gpio_out_54");
            gpio_set_value_cansleep(54, 1);
            break;
        case 18:
            gpio_free(56);
            gpio_free(60);
            gpio_free(78);
            gpio_request_one(56,GPIOF_DIR_OUT, "gpio_out_56");
            gpio_request_one(60,GPIOF_DIR_OUT, "pin_mux_60");
            gpio_request_one(78,GPIOF_DIR_OUT, "pin_mux_78");
            gpio_set_value_cansleep(56, 1);
            gpio_set_value_cansleep(60, 1);
            gpio_set_value_cansleep(78, 1);
            break;
        case 19:
            gpio_free(58);
            gpio_free(60);
            gpio_free(79);
            gpio_request_one(58,GPIOF_DIR_OUT, "gpio_out_58");
            gpio_request_one(60,GPIOF_DIR_OUT, "pin_mux_60");
            gpio_request_one(79,GPIOF_DIR_OUT, "pin_mux_79");
            gpio_set_value_cansleep(58, 1);
            gpio_set_value_cansleep(60, 1);
            gpio_set_value_cansleep(79, 1);
            break;
    }
}

//funtion for clearing the gpio pins of CS which user is using
void commmand_clear (int pin){

	switch(pin){
        case 0:
        	gpio_free(11);
        	gpio_free(32);    
            break;
        case 1:
            gpio_free(12);
            gpio_free(28);
            gpio_free(45);
            break;
        case 2:
            gpio_free(13);
            gpio_free(34);
            gpio_free(77);
            break;
        case 3:
            gpio_free(14);
            gpio_free(16);
            gpio_free(76);
            gpio_free(64);
            break;
        case 4:
            gpio_free(6);
            gpio_free(36);
            break;
        case 5:
            gpio_free(0);
            gpio_free(18);
            gpio_free(66);
            break;
        case 6:
            gpio_free(1);
            gpio_free(20);
            gpio_free(68);
            break;
        case 7:
            gpio_free(38);
            break;
        case 8:
            gpio_free(40);
            break;
        case 9:
            gpio_free(4);
            gpio_free(22);
            gpio_free(70);
            break;
        case 10:
            gpio_free(10);
            gpio_free(26);
            gpio_free(74);
            break;
        case 12:
            gpio_free(15);
            gpio_free(42);
            break;
        case 14:
            gpio_free(48);
            break;
        case 15:
            gpio_free(50);
            break;
        case 16:
            gpio_free(52);
            break;
        case 17:
            gpio_free(54);
            break;
        case 18:
            gpio_free(56);
            gpio_free(60);
            gpio_free(78);
            break;
        case 19:
            gpio_free(58);
            gpio_free(60);
            gpio_free(79);
            break;
    }

}

//funtion for setting the CS pin of user to high
void command_cs_1(int pin){

    switch(pin){
        case 0:
            gpio_set_value_cansleep(11, 1);    
            break;
        case 1:
            gpio_set_value_cansleep(12, 1);
            break;
        case 2:
            gpio_set_value_cansleep(13, 1);
            break;
        case 3:
            gpio_set_value_cansleep(14, 1);
            break;
        case 4:
            gpio_set_value_cansleep(6, 1);
            break;
        case 5:
            gpio_set_value_cansleep(0, 1);
            break;
        case 6:
            gpio_set_value_cansleep(1, 1);
            break;
        case 7:
            gpio_set_value_cansleep(38, 1);
            break;
        case 8:
            gpio_set_value_cansleep(40, 1);
            break;
        case 9:
            gpio_set_value_cansleep(4, 1);
            break;
        case 10:
            gpio_set_value_cansleep(10, 1);
            break;
        case 12:
            gpio_set_value_cansleep(15, 1);
            break;
        case 14:
            gpio_set_value_cansleep(48, 1);
            break;
        case 15:
            gpio_set_value_cansleep(50, 1);
            break;
        case 16:
            gpio_set_value_cansleep(52, 1);
            break;
        case 17:
            gpio_set_value_cansleep(54, 1);
            break;
        case 18:
            gpio_set_value_cansleep(56, 1);
            break;
        case 19:
            gpio_set_value_cansleep(58, 1);
            break;
    }
}

//funtion for setting the CS pin of user to low
void command_cs_0(int pin){

    switch(pin){
        case 0:
            gpio_set_value_cansleep(11, 0);    
            break;
        case 1:
            gpio_set_value_cansleep(12, 0);
            break;
        case 2:
            gpio_set_value_cansleep(13, 0);
            break;
        case 3:
            gpio_set_value_cansleep(14, 0);
            break;
        case 4:
            gpio_set_value_cansleep(6, 0);
            break;
        case 5:
            gpio_set_value_cansleep(0, 0);
            break;
        case 6:
            gpio_set_value_cansleep(1, 0);
            break;
        case 7:
            gpio_set_value_cansleep(38, 0);
            break;
        case 8:
            gpio_set_value_cansleep(40, 0);
            break;
        case 9:
            gpio_set_value_cansleep(4, 0);
            break;
        case 10:
            gpio_set_value_cansleep(10, 0);
            break;
        case 12:
            gpio_set_value_cansleep(15, 0);
            break;
        case 14:
            gpio_set_value_cansleep(48, 0);
            break;
        case 15:
            gpio_set_value_cansleep(50, 0);
            break;
        case 16:
            gpio_set_value_cansleep(52, 0);
            break;
        case 17:
            gpio_set_value_cansleep(54, 0);
            break;
        case 18:
            gpio_set_value_cansleep(56, 0);
            break;
        case 19:
            gpio_set_value_cansleep(58, 0);
            break;
    }
}