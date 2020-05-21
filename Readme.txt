ASU ID: 1217678787
Name: Viraj Savaliya

The project submission contains 4 .c files, 3 .h files and a Makefile.

The files are as follows:
1. user.c = the user program 
2. mysocket.c = the the core module on the kernel side which includes socket interface of kernel side and is linked with 2 submodules.
3. header.h = the header file linked with user and mysocket which defines Generic Netlink messages which will be transferred via socket.
4. spi_drv.c = the SPI module which is basically the submodule of the core module here and creates a slave SPI device for the LED dot-matrix.
5. spihead.h = the header file which is linked with the spi_drv and mysocket.
6. hcsr_drv.c = the HCSR sensor module which is basically the submodule of the core module here and operates the ultrasonic sensor.
7. hcsrhead.h = the hcsr header which is linked with the hcsr_drv and mysocket.
8. Makefile = the make file for Galileo Gen 2 board whic will create the executable binary to run.

***********************************************************************************************************************************************************************************************************

1. To make the executable binary type "make" on the command of terminal while inside the directory of the all the above files.
	2 executable binaries for running will be formed:
	a. dummy.ko
	b. user

2. To transfer your executable binaries onto the board use "sudo scp <filename> root@<inet_address>:/home/root"
	Use the command 2 times, once each for the one binary.
	It will prompt for password, type the root password you have set for your board.

3. Run the files using following commands in same order.
	a. use "insmod dummy.ko" to insert the loadable kernel module and initialise all the modules
	b. use "chmod +x user", it will make your user progam executable.
	c. use "./user" to run the user program
	d. when you want to stop just create an interrupt using "ctrl+c" to stop the program from running.
	e. remove the kernel module using "rmmod dummy".

********************************************************************************************************************************************************************************************************* 

Operation:

When the onnject infront of the HCSR sensor keeps coming closer by 4cm an animation of a 'skull' and 'neutral-face smiley' will be displayed.
For all other cases an animation of the game 'Pacman closing mouth' and 'ghost' will be displayed.

Note: For default the pins in the program are CS:5, Trigger:8, Echo:10 are being used. Feel free to change the pins defined at the top in the user.c file and run.

********************************************************************************************************************************************************************************************************* 

Sample Output:

[ 8454.230157] The distance is calculated
Kernel says: The distance by HCSR is 8
[ 8454.710136] The distance is calculated
Kernel says: The distance by HCSR is 8
[ 8455.190136] The distance is calculated
Kernel says: The distance by HCSR is 8
[ 8455.680136] The distance is calculated
Kernel says: The distance by HCSR is 8
[ 8456.160140] The distance is calculated
Kernel says: The distance by HCSR is 5
[ 8457.160138] The distance is calculated
Kernel says: The distance by HCSR is 3
[ 8457.650136] The distance is calculated
Kernel says: The distance by HCSR is 2
[ 8458.140137] The distance is calculated
Kernel says: The distance by HCSR is 1
[ 8458.653721] The distance is calculated
Kernel says: The distance by HCSR is 2
[ 8461.610158] The distance is calculated
Kernel says: The distance by HCSR is 12
[ 8462.090134] The distance is calculated
Kernel says: The distance by HCSR is 14
[ 8462.570128] The distance is calculated
Kernel says: The distance by HCSR is 15
[ 8463.060136] The distance is calculated
Kernel says: The distance by HCSR is 14
Kernel says: The distance by HCSR is 8
[ 8466.960135] The distance is calculated
Kernel says: The distance by HCSR is 6
[ 8467.470134] The distance is calculated
Kernel says: The distance by HCSR is 6
[ 8467.950135] The distance is calculated
Kernel says: The distance by HCSR is 5


********************************************************************************************************************************************************************************************************* 

References:

For writing the code refernces are being taken from the following sources:
1. The Generic netlink socket code provided by Prof.Lee during the lecture.
2. The previous years student's project github links provided by Prof.Lee.
3. The lecture notes of class CSE530.
4. The TA office hours and canvas discussion board.
5. My HCSR Assignment 2 code for ultrsonic sensor.