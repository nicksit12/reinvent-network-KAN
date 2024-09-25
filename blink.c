#include <stdio.h>
#include <pigpiod_if2.h>

int main(){
    int P1t=27;
    int P1r=26;
    int P3t=23;
    int P3r=22;
    int P2t=25;
    int P2r=24;
    int P4t=21;
    int P4r=20;

    int pi = pigpio_start(NULL, NULL);

    set_mode(pi, P1t, PI_OUTPUT);
    set_mode(pi, P1r, PI_INPUT);
    set_mode(pi, P3t, PI_OUTPUT);
    set_mode(pi, P3r, PI_INPUT);
    set_mode(pi, P2t, PI_OUTPUT);
    set_mode(pi, P2r, PI_INPUT);
    set_mode(pi, P4t, PI_OUTPUT);
    set_mode(pi, P4r, PI_INPUT);

    //turn on the LEDs by setting the GPIO pins high
    gpio_write(pi, P1t, 1); 
    gpio_read(pi, P1r);
    set_pull_up_down(pi, P1r, PI_PUD_DOWN);
   
    time_sleep(2); 

    gpio_write(pi, P3t, 1);
    gpio_read(pi, P3r);
    set_pull_up_down(pi, P3r, PI_PUD_DOWN);

    time_sleep(2);

    gpio_write(pi, P2t, 1); 
    gpio_read(pi, P2r);
    set_pull_up_down(pi, P2r, PI_PUD_DOWN);
   
    time_sleep(2);

    gpio_write(pi, P4t, 1); 
    gpio_read(pi, P4r);
    set_pull_up_down(pi, P4r, PI_PUD_DOWN);

    printf("LEDs for Port 1 and Port 3 are now ON\n");

    time_sleep(5);

    //turn off the LEDs by setting the GPIO pins low
    gpio_write(pi, P1t, 0); 
    gpio_read(pi, P1r);
    time_sleep(2);  
    gpio_write(pi, P3t, 0); 
    gpio_read(pi, P3r); 
    time_sleep(2);
    gpio_write(pi, P2t, 0);
    gpio_read(pi, P2r);
    time_sleep(2);  
    gpio_write(pi, P4t, 0);
    gpio_read(pi, P4r); 

    printf("LEDs for Port 1 and Port 3 are now OFF\n");

    time_sleep(2); 

    //stop PIGPIO 
    pigpio_stop(pi); 

    return 0;
}

