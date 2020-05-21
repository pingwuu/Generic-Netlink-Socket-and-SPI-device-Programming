#ifndef __TEST_CONFIG
#define __TEST_CONFIG

//extern Response *r1;

struct data_dotmat
{
  int ind1;
  unsigned char ledpatt[4][8];
  //unsigned int ledseq[10];
};

extern struct data_dotmat ddm_info;

struct data_pins
{
	int trigg;
	int ech;
  	int cspin;
  //unsigned int ledseq[10];
};

extern struct data_pins dattconfig;

 //static void spidev_transfer(unsigned char ch1, unsigned char ch2);

 int send_pattern(int alpha);

 int spidev_init(int alpha);

 void spidev_exit(int alpha);

#endif
