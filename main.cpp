
#include "EthernetInterface.h"
#include "TCPServer.h"
#include "TCPSocket.h"
#include "mbed.h"
#include "sample_hardware.hpp"
#include "TextLCD.h"
#include "Timer.h"
#include "BMP280.h"
#include "threads.h"
#include "mbed_events.h"
#include <string>
#include <iostream>
#include <stm32f4xx.h>
using namespace std;
//threads
TextLCD lcd(D9, D8, D7, D6, D4, D2);
Thread t1, t2, t3, t4(osPriorityNormal, 32 * 1024), t5, t6;
int main()
{
    //Time_Date_Init();
    //////Pre thread INIT//////
    Terminal_Init();
    //////Attach Threads//////
    t1.start(callback(Read_Sensors));   // Start sensors thread
    t2.start(callback(Print_Data));   // Start sensors thread
    t3.start(callback(Scanf));
    t4.start(callback(Ethernet));
    t5.start(callback(SD));
    //t6.start(callback(BD));
}


void Terminal_Init(){
  
            printf("\e[?25l");
            text_colour(5); //change text colour to magneta
            move_cursor(0,0);
            printf("                                                                                                                \n\r");
            printf("                                                                                                                \n\r");
            //columns   00000000001111111111222222222233333333334444444445555555555666666666677777777778888888889999999999 
            //columns   01234567890123456789012345678901234567890123456790123456789012345678901234567890123456890123456789    //Rows
      printf("***************************************************************************************************************\n\r" //00
             "*     Time/Date     * Light (Lux) * Pressure (Mbar) * Temp (DegC) *            *          User Input          *\n\r" //01
             "***************************************************************************************************************\n\r" //02
             "*                   *             *                 *             *            * Set sample rate (Enter 1-9)  *\n\r" //03
             "*                   *             *                 *             *            *                              *\n\r" //04
             "*                   *             *                 *             *            * 1 = 30 seconds               *\n\r" //05
             "*                   *             *                 *             *            * 2 = 25 seconds               *\n\r" //06
             "*                   *             *                 *             *            * 3 = 20 seconds               *\n\r" //07
             "*                   *             *                 *             *            * 4 = 15 seconds               *\n\r" //08
             "*                   *             *                 *             *            * 5 = 10 seconds               *\n\r" //09
             "*                   *             *                 *             *            * 6 = 5  seconds               *\n\r" //10
             "*                   *             *                 *             *            * 7 = 1  seconds               *\n\r" //11
             "*                   *             *                 *             *            * 8 = .5 seconds               *\n\r" //12
             "*                   *             *                 *             *            * 9 = .2 seconds               *\n\r" //13
             "*                   *             *                 *             *            *                              *\n\r" //14
             "*                   *             *                 *             *            * Sample rate = 15 Seconds     *\n\r" //15
             "*                   *             *                 *             *            *                              *\n\r" //16
             "*                   *             *                 *             *            * Start/Stop sampling = Q      *\n\r" //17
             "*                   *             *                 *             *            *                              *\n\r" //18
             "*                   *             *                 *             *            * Set time and date   = W      *\n\r" //19
             "*                   *             *                 *             *            *                              *\n\r" //20
             "*                   *             *                 *             *            * Delete FIFO data    = E      *\n\r" //21
             "*                   *             *                 *             *            *                              *\n\r" //23
             "*                   *             *                 *             *            * Fetch FIFO sample   = #      *\n\r" //24
             "*                   *             *                 *             *            *                              *\n\r" //25
             "*                   *             *                 *             *            * Debug = off: toggle = R      *\n\r" //26
             "***************************************************************************************************************");   //27
    
    text_colour(3); //change text colour to yellow
    move_cursor(3, 6);
    printf("Time/Date");
    move_cursor(3, 22);
    printf("Light (Lux)");
    move_cursor(3, 36);
    printf("Pressure (Mbar)");
    move_cursor(3,54);
    printf("Temp (DegC)");
    move_cursor(3,70);
    printf("Fifo Num");
    move_cursor(3,90);
    printf("User Input");
    move_cursor(5,81);
    printf("Set sample rate (Enter 1-9)");
    move_cursor(7,81);
    printf("1 = 30 seconds");
    move_cursor(8,81);
    printf("2 = 25 seconds");
    move_cursor(9,81);
    printf("3 = 20 seconds");
    move_cursor(10,81);
    printf("4 = 15 seconds");
    move_cursor(11,81);
    printf("5 = 10 seconds");
    move_cursor(12,81);
    printf("6 = 5  seconds");
    move_cursor(13,81);
    printf("7 = 1  seconds");
    move_cursor(14,81);
    printf("8 = .5 seconds");
    move_cursor(15,81);
    printf("9 = .2 seconds");
    move_cursor(17,81);
    printf("Sample rate = 15 Seconds");
    move_cursor(19,81);
    printf("Fetch FIFO sample   = #");
    move_cursor(21,81);
    printf("Start/Stop sampling = Q");
    move_cursor(23,81);
    printf("Set time and date   = W");
    move_cursor(25,81);
    printf("Delete FIFO data    = E");
    move_cursor(27,81);
    printf("Debug = off: toggle = R");
}
