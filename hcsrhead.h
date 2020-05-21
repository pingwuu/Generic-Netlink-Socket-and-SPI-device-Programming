#ifndef __HCSR_CONFIG
#define __HCSR_CONFIG

struct configurepins
{

	int trigger_pin;
  	int echo_pin;
  	int cspin;
  
};

extern struct configurepins hcsrconfig;

struct measd_dist
{
  int distancesensed;
  int dist_flag;
};

extern struct measd_dist hcsrdist;

int measure_the_distance(int beta);

int driver_initialisation(int beta);

void driver_exiting(int beta);

#endif
