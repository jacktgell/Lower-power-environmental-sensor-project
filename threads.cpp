
#include "mbed.h"
#include "EthernetInterface.h"
#include "TCPServer.h"
#include "TCPSocket.h"
#include "sample_hardware.hpp"
#include "SDBlockDevice.h"
#include "FATFileSystem.h"
#include <Timer.h>
#include "TextLCD.h"
#include "BMP280.h"
#include "mbed_events.h"
#include <string>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <stm32f4xx.h>
#include <string>
#include "threads.h"

//interrupts
InterruptIn button1(USER_BUTTON);
DigitalOut yellowLED(PB_10);
InterruptIn button2(PE_12);
InterruptIn button3(PE_14);
DigitalOut myleds(PE_15);
FATFileSystem fs("fs");
HeapBlockDevice bd(128 * 512, 512);
SDBlockDevice sd(PB_5, D12, D13, D10);
Timer update_time;

using namespace std;

float LUX = 0; //stores most recent values of LUX                                                                                          shared resource LOCKED
float temp = 0; //stores most recent values of temp                                                                                        shared resource LOCKED
float pressure = 0; //stores most recent values of pressure                                                                                shared resource LOCKED
float LDR_sample[121];  //FIFO LDR values                                                                                                  shared resource LOCKED
float Pressure_sample[121]; //FIFO pressure values                                                                                         shared resource LOCKED
float Temp_sample[121]; //FIFO temp values                                                                                                 shared resource LOCKED
uint64_t myTime = 0;    //holds time and date return from class                                                                            unshared resource
uint32_t sample_rate = 15000;   //initial sample rate                                                                                      shared resource LOCKED
uint32_t old_samplerate = 15000;   //refrence to detect a change in sample rate                                                            shared resource LOCKED
uint16_t year = 0;  //stores the value for year                                                                                            shared resource LOCKED
uint8_t fetched_sample = 0; //selecting individual sample from buffer                                                                      shared resource LOCKED
uint8_t number_sample[121]; //FIFO address                                                                                                 shared resource LOCKED
uint8_t day_sample[120];    //FIFO array of day samples                                                                                    shared resource LOCKED
uint8_t month_sample[120];  //FIFO array of manth samples                                                                                  shared resource LOCKED
uint8_t year_sample[120];       //FIFO array of year samples                                                                               shared resource LOCKED
uint8_t hours_sample[120];  //FIFO array of hours samples                                                                                  shared resource LOCKED
uint8_t minutes_sample[120];    //FIFO array of minute sample                                                                              shared resource LOCKED
uint8_t sample_num = 0;             //points to FIFO current address to write to                                                           unshared resource
uint8_t sample_print_colour = 2;    //argument to set intial text colour in function                                                       unshared resource
uint8_t minutes = 0;    //single minute sample                                                                                             shared resource LOCKED
uint8_t hours = 0;  //single hour sample                                                                                                   shared resource LOCKED
uint8_t day = 0;    //single day sample                                                                                                    shared resource LOCKED
uint8_t month = 0;  //single month sample                                                                                                  shared resource LOCKED
uint8_t sd_counter = 0; //to know how many samples to push to buffer                                                                       unshared resource
bool time_to_sample = true; //set in ticker to signal thread to take sample                                                                shared resource
bool time_to_use_sample = true; //after sample taken signal print thread to print                                                          shared resource LOCKED
bool toggle = true; //toggle printing on and off                                                                                           shared resource LOCKED
bool Fetch = false; //toggle run code to fetch individual sample                                                                           shared resource LOCKED
bool erase = false; //condition when true FIFO will be deleted                                                                             shared resource LOCKED
bool show_sampling_resume = false;  //used in function to display in putty that sampling has been resumed                                  shared resource LOCKED
bool show_sampling_paused = false;                                                                                                       //shared resource LOCKED
bool set_t_d = false;                                                                                                                    //safe
bool storing_samples = false;                                                                                                            //safe
bool debuger = false;                                                                                                                    //safe
bool LCD_td = false;                                                                                                                     //safe
bool b2 = false;                                                                                                                         //ISR
bool b3 = false;                                                                                                                         //ISR

AnalogIn ADC_In(A0);
EthernetInterface eth; 

//interrupts
Ticker SampleRateISR;

//function to set entire FIFO to zero
void clear_arrays(){
    //cycle through buffer setting all values to zero
    for(uint8_t i = 0; i<=120; i++){
        Crit_LDR_sample(0,1,i);
        Crit_Pressure_sample(0,1,i);
        Crit_Temp_sample(0,1,i);
        Crit_number_sample(0,1,i);
        Crit_day_sample(0,1,i);
        Crit_month_sample(0,1,i);
        Crit_year_sample(0,1,i);
        Crit_hours_sample(0,1,i);
        Crit_minutes_sample(0,1,i);
    }
}

//changes the frequency of the ticker
void update_ticker(){
    //detach interrupt
    SampleRateISR.detach();
    //set new samplerate and reattach
    attachtickerfreq();
    SampleRateisr();
}

//signals sample thread to read sensor values
void SampleRateisr(){
    time_to_sample = true;
}

//arguent is passed from scanf to attach new ticker frequeny
void attachtickerfreq(){
    switch(Crit_sample_rate(0,0)){
        case(30000):
            SampleRateISR.attach(&SampleRateisr, 30);
            break;     
        case(25000):
            SampleRateISR.attach(&SampleRateisr, 25);
            break;  
        case(20000):
            SampleRateISR.attach(&SampleRateisr, 20);
            break;  
        case(15000):
            SampleRateISR.attach(&SampleRateisr, 15);
            break;  
        case(10000):
            SampleRateISR.attach(&SampleRateisr, 10);
            break;  
        case(5000):
            SampleRateISR.attach(&SampleRateisr, 5);
            break;  
        case(1000):
            SampleRateISR.attach(&SampleRateisr, 1);
            break;  
        case(500):
            SampleRateISR.attach(&SampleRateisr, 0.5);
            break;  
        case(200):
            SampleRateISR.attach(&SampleRateisr, 0.2);
            break;        
    }
}

//terminal thread functions
void Print_Data()
{
    uint8_t i = 5;  //5 if the cursor value to start print downwards
    bool colour = true; //to toggle between two text colours
    text_colour(sample_print_colour);
    Crit_time_to_use_sample(0,1);   //blocks until the first sample has been read
    bool Switch = false;
    bool debug_ref = false;
    button2.rise(&lcd_isr); //either switch will trigger lcd to setup for stating a new time and date
    button3.rise(&lcd_isr);
    while(1) {
            
        while(toggle==false){ 
            if(Crit_show_sampling_paused(0,0)==true){
                Print_Sampling_Paused(); 
                Switch = true;
            }  
        }
        
        while(Crit_time_to_use_sample(0,0) == false){   //not yet time to sample release thread
            Thread::wait(50);
        
            while(Crit_Fetch(0,0) == true){     //run seperate function to fetch sample from FIFO
                Fetch_Sample();
            } 
            while(Crit_show_sampling_resume(0,0) == true && Switch == true){
                Print_Sampling_Resume();
                Switch = false;
            }
            while(Crit_erase(0,0) == true){     //Delete FIFO buffer
                FIFO_Deleted();
            } 
            while(Crit_set_t_d(0,0) == true){
                Putty_Set_Time();
            }
            while(Crit_debuger(0,0)!=debug_ref){
                print_debug_state(); 
                debug_ref = Crit_debuger(0,0);
            }
            while(LCD_td==true){
                Crit_toggle(0,1);
                Print_Sampling_Paused();
                button2.rise(&lcd_isr_b2);
                button3.rise(&lcd_isr_b3);
                Time_Date_Init();
                Print_Sampling_Resume();
                Crit_toggle(1,1);
                LCD_td=false;
            }
        }
                
        move_cursor(i, 02);
        printf(" %d/%d/%d %d:%d ",Crit_day(0,0),Crit_month(0,0),Crit_year(0,0),Crit_hours(0,0),Crit_minutes(0,0));
        move_cursor(i, 24);
        text_colour(sample_print_colour);
        printf("%6.1f  ", Crit_Lux(0,0));
        move_cursor(i, 40);
        printf("%.2f  ", Crit_pressure(0,0));
        move_cursor(i, 55);
        printf("%6.1f  ", Crit_temp(0,0));
        move_cursor(i, 71);
        printf("#%d    ", sample_num);
        
        Update_Samplerate();
        
        i++;//increment so mouse cursor can move to the next line down
        
        if(i==28){
            i = 5; //cursor is at the bottom of the terminal push back to the starting position 
            
            //switch colour for when sample are being print on top of one another
            if(colour == true){
                sample_print_colour += 4;
                text_colour(sample_print_colour); //save as variable so easy to get back to in other instances
            }
            else{
                sample_print_colour -= 4;
                text_colour(sample_print_colour); 
            }
            
            colour = !colour;   //toggle whether to use if or else on next run
        }
        
        Crit_time_to_use_sample(0,1);   //sampling is complete lower flag
    }
}

//use asci esc sequences to change the colour of the terminal text input argument to choose colour
void text_colour(int colour){
  
  switch(colour){
    case(0):  //black
      printf("\x1b[30m");
      break;
      
    case(1):  //
      printf("\x1b[31m");
      break;
      
    case(2):  //green
      printf("\x1b[32m");
      break;
      
    case(3):  //yellow
      printf("\x1b[33m");
      break;
      
    case(4): //blue
      printf("\x1b[34m");
      break;

    case(5): //magneta
      printf("\x1b[35m");
      break;
      
    case(6):  //cyan
      printf("\x1b[36m");
      break;

    case(7):  //white
      printf("\x1b[37m");
      break;
  }
}
  
void Erase_Line(){
    printf("\x1b");   // ESC
    printf("[");    
    printf("2K");
}

//sselect a row ad coloumn for ascii esc sequences to move around the terminal
void move_cursor(int row, int col){
    printf("\x1b[%d;%dH",row+1,col+1);
}

//print the new current sample to the terminal
void Update_Samplerate(){
    
    if(Crit_sample_rate(0,0)!=Crit_old_samplerate(0,0)){    //checks for a change in sample rate before printing
        Crit_old_samplerate(Crit_sample_rate(0,0),1);
        move_cursor(17, 95);
        text_colour(3);
        
        switch(Crit_sample_rate(0,0)){
            case(30000):
            printf("30");
            break; 
              
            case(25000):
            printf("25");
            break;   
       
            case(20000):
            printf("20");
            break;   
       
            case(15000):
            printf("15");
            break;   
       
            case(10000):
            printf("10");
            break;   
       
            case(5000):
            printf("5 ");
            break;   
       
            case(1000):
            printf("1 ");
            break;   
       
            case(500):
            printf(".5");
            break;   
       
            case(200):
            printf(".2");
            break;   
        }
        text_colour(sample_print_colour);
    }
}
void Scanf(void){
    Scan_Samplerate();
}

void Scan_Samplerate(){
    
    char scan = 0;  //char data type for cin
    Timer t;
    t.start();
    while(1){
        while(scan==0){ //while no serial is avaliable
            while(Fetch == true){Thread::wait(100);}    //fetching sample from buffer release thread
            scan = getchar();
        }
        switch(scan){
            case(49):   //ascii 1
                Crit_sample_rate(30000,1);
                break;   
                
            case(50):   //ascii 2
                Crit_sample_rate(25000,1);
                break;  
                
            case(51): //ascii 3
                Crit_sample_rate(20000,1);
                break; 
                
            case(52): //ascii 4
                Crit_sample_rate(15000,1);
                break;   
                
            case(53): //ascii 5
                Crit_sample_rate(10000,1);
                break;  
                
            case(54): //ascii 6
                Crit_sample_rate(5000,1);
                break; 
                
            case(55): //ascii 7
                Crit_sample_rate(1000,1);
                break;   
                
            case(56): //ascii 8
                Crit_sample_rate(500,1);
                break;  
                
            case(57): //ascii 9
                Crit_sample_rate(200,1);
                break;
                        
            case ('q'):
            case ('Q'):
                Crit_toggle(!toggle,1); //this variable will stop printing and sampling
                if(Crit_toggle(0,0)==false)
                   Crit_show_sampling_paused(1,1);
                else
                   Crit_show_sampling_resume(1,1);
                break; 
            case ('#'):
                Fetch = true;
                break; 
            case ('e'):
            case ('E'):
                clear_arrays(); //erases FIFO
                Crit_erase(1,1);    //signal to erase FIFO
                break;
            case ('w'):
            case ('W'):
                Crit_set_t_d(1,1);
                break;
            case ('r'):
            case ('R'):
                if(Crit_debuger(0,0) == false)
                    Crit_debuger(1,1);
                else
                    Crit_debuger(0,1);
                break;
        }
        
        //ensures no other arguments other than ascii vals 1-9 get into the update ticker function
        if(scan>=49&&scan<=57){
            update_ticker();
        }
                
        scan = 0;   //sets scanf back to zero so it can spin again
    }
}

//read sensors thread

void Read_Sensors()
{

    SampleRateISR.attach(&SampleRateisr, 15);
    update_time.start();

    while(1) {
        
        while(Crit_toggle(0,0)==false){
            Thread::wait(100);   //spin if user stops sampling
        }  
        
        if(time_to_sample == true) {    //returned signal from ticker
            Crit_temp(sensor.getTemperature(),1);   //takes temperature sample  
            Crit_pressure(sensor.getPressure(),1);
            Crit_Lux(20000-ADC_In.read_u16(),1);
            time_to_sample = false; //sampling complete lower flag
            Crit_time_to_use_sample(1,1);   //raise flag to toher thread that it is ready to print most recent sample
                            
        if(update_time.read_ms()>=60000) {
            Update_Time();  //udate the time set each minute
            update_time.reset();    //reset timer every minute
        }

        sample_num++;
        
        //buffer has 120 slots return to overwrite slot zero
        if(sample_num==120) {
            sample_num = 0;
            sd_counter = 1;
        }

        //assign most recent samples taken to a slot within the FIFO
        Crit_LDR_sample(Crit_Lux(0,0),1,sample_num);
        Crit_Pressure_sample(Crit_pressure(0,0),1,sample_num);
        Crit_Temp_sample(Crit_temp(0,0),1,sample_num);
        Crit_number_sample(sample_num,1,sample_num);
        Crit_day_sample(Crit_day(0,0),1,sample_num);
        Crit_month_sample(Crit_month(0,0),1,sample_num);
        Crit_year_sample(Crit_year(0,0),1,sample_num);
        Crit_hours_sample(Crit_hours(0,0),1,sample_num);
        Crit_minutes_sample(Crit_minutes(0,0),1,sample_num);
        
        }
    }
}
        
//DigitalOut myled(PE_15);
void Ethernet(){
 
    //SocketAddress clt_addr;               //Address of incoming connection
    //TCPSocket clt_sock;
    //Configure an ethernet connection
    DigitalOut ping(LED3);
    DigitalOut ping2(LED2);
    eth.set_network(IP, NETMASK, GATEWAY);
    eth.connect();   
    //Now setup a web server
    TCPServer srv;                          //TCP/IP Server
    //TCPSocket clt_sock;                   //Socket for communication
    //Open the server on ethernet stack 
    srv.open(&eth);
    //Bind the HTTP port (TCP 80) to the server 
    srv.bind(eth.get_ip_address(), 80);
    //Can handle 5 simultaneous connections
    srv.listen(5);
    SocketAddress clt_addr;                 //Address of incoming connection
    TCPSocket clt_sock;
    while (true) {
        //Block and wait on an incoming connection
        srv.accept(&clt_sock, &clt_addr);
        //Uses a C++ string to make it easier to concatinate
        string response;
        //This is a C string
        char LUX_str[8];
        char pressure_str[8];
        char temp_str[6];
        char day_str[3];
        char month_str[3];
        char year_str[5];
        char min_str[3];
        char hour_str[3];
        char slash_str[2] = "/";
        char colon_str[2] = ":";

        //Convert to a C String
        sprintf(LUX_str, "%6.1f", Crit_Lux(0,0));
        sprintf(pressure_str, "%.2f", Crit_pressure(0,0));
        sprintf(temp_str, "%6.1f", Crit_temp(0,0));
        sprintf(day_str, "%d", Crit_day(0,0));
        sprintf(month_str, "%d", Crit_month(0,0));
        sprintf(year_str, "%d", Crit_year(0,0));
        sprintf(min_str, "%d", Crit_minutes(0,0));
        sprintf(hour_str, "%d", Crit_hours(0,0));

        //Build the C++ string response
        response = HTTP_MESSAGE_BODY1;
        response =  "<html> \r\n ";
        response += "<meta http-equiv=\"refresh\" content=\"1; url=http://10.0.0.10\">";
        response += "</h1> \r\n <h1 style=\"color:black; font-size: 450%; position: fixed; top:25.2%; left:67%;\">";
        response += temp_str;
        response += "</h1> \r\n <h1 style=\"color:black; font-size: 450%; position: fixed; top:42.5%; left:67%;\">";
        response += pressure_str;
        response += "</h1> \r\n <h1 style=\"color:black; font-size: 450%; position: fixed; top:61%; left:67%;\">";
        response += LUX_str;
        response += "</h1> \r\n <h1 style=\"color:white; font-size: 300%; position: fixed; top:80%; left:10%;\">";
        response += day_str;
        response += "</h1> \r\n <h1 style=\"color:white; font-size: 300%; position: fixed; top:80%; left:14%;\">";
        response += slash_str;
        response += "</h1> \r\n <h1 style=\"color:white; font-size: 300%; position: fixed; top:80%; left:16%;\">";
        response += month_str;
        response += "</h1> \r\n <h1 style=\"color:white; font-size: 300%; position: fixed; top:80%; left:20%;\">";
        response += slash_str;
        response += "</h1> \r\n <h1 style=\"color:white; font-size: 300%; position: fixed; top:80%; left:22%;\">";
        response += year_str;
        response += "</h1> \r\n <h1 style=\"color:white; font-size: 300%; position: fixed; top:80%; left:30%;\">";
        response += hour_str;
        response += "</h1> \r\n <h1 style=\"color:white; font-size: 300%; position: fixed; top:80%; left:34%;\">";
        response += colon_str;
        response += "</h1> \r\n <h1 style=\"color:white; font-size: 300%; position: fixed; top:80%; left:36%;\">";
        response += min_str;
        response += "</h1> \r\n <body style=\"background-image:url(https://scontent-lhr3-1.xx.fbcdn.net/v/t1.0-9/43633750_664407870640840_7691079799023861760_o.jpg?_nc_cat=100&oh=8448d9bade252df3fa53463acfb5ed25&oe=5C4DC563); background-size: 100% 100%;\"></body> ";
        response += "\r\n </html>";
        response += HTTP_MESSAGE_BODY2;
        clt_sock.send(response.c_str(), response.size()+10);
        Thread::wait(1000);
    }
}

class LCD
{

public:

    uint64_t timedate[12];
    uint8_t user_count;
    uint8_t i;
    uint8_t x;
        
    //pushes most recent values to the terminal
    void Display_Values(){
        
        lcd.cls();
        lcd.printf("DegC  mBar  LUX");
        lcd.printf("%6.1f %.2f %6.1f\n\r", Crit_temp(0,0), Crit_pressure(0,0), Crit_Lux(0,0));
    }    

uint64_t Set_Time_Date() {

        //set up display to prompt user to set time and date
        lcd.cls();
        lcd.printf("set time + date");
        lcd.locate(0,1);
        lcd.printf("00:00 00/00/0000");
        user_count = 0; //user count is the cursor position on the lcd
        i = 0;  //allows user to cycle 0 through to ten on the lcd
        uint64_t return_values = 0; //large datatype to return compressed values
        while(user_count<16) {
            if(b3==1) {    //for when button is pressed
                button2.rise(NULL);
                button3.rise(NULL);
                b3 = 0;
                i++;    //increment the value of the number which will be displayed to the LCD
                if(i==10) {//if the user cycles past 9 the lcd will revert to zero to start cycling through values again
                    i = 0;
                }
                lcd.locate(user_count,1);   //move cursor postion
                lcd.printf("%d",i); //print the value of how many times the button has been pressed
                wait_ms(250);
                button2.rise(&lcd_isr_b2);
                button3.rise(&lcd_isr_b3);
            }
                        
            //when user has chosen the value for the first slot of the lcd position the other button will move the cursor to the right and repeat the process of before
            if(b2==1) {
                button2.rise(NULL);
                button3.rise(NULL);
                b2 = 0;
                user_count++;   
                timedate[x] = i;
                x++;
                i = 0;
                lcd.locate(user_count,1);
                if(user_count==2) { //prints : between the minutes and the hours
                    user_count++;
                    lcd.printf(":");
                }
                if(user_count==5) {
                    user_count++;
                    lcd.printf(" ");    //prints a space between the time and the date
                }
                if(user_count==8||user_count==11) {
                    user_count++;
                    lcd.printf("/");    //print a slash between the day, month and year
                }
                wait_ms(250);
                button2.rise(&lcd_isr_b2);
                button3.rise(&lcd_isr_b3);
            }
        }
                
        //cycles through the array into a single variable as we can only return a single value ffrom a function
        //each number that the user has chosen will own a nibble in this 64 bit address
        for(uint8_t t = 0; t<12; t++) {
            return_values += timedate[t]<<(t*4);
        }
        b2 = b3 = 0;
        lcd.cls();
        return return_values; //returns time and date
    }
};

void Time_Date_Init(void){
 
    LCD time;   //creates instance
    myTime = time.Set_Time_Date();  //64 bit compressed time and date value is returned
    hours = (myTime&0xF)*10+((myTime&0xF0)>>4); //unpacks the hours byte multiply one by 10 and adds together
    minutes = ((myTime&0xF00)>>8)*10+((myTime&0xF000)>>12); //isolates the nibbles in which the minutes and held and multiplys and adds
    day = ((myTime&0xF0000)>>16)*10+((myTime&0xF00000)>>20);    //repeats for day ,month and year
    month = ((myTime&0xF000000)>>24)*10+((myTime&0xF0000000)>>28);
    year = ((myTime&0xF00000000)>>32)*1000+((myTime&0xF000000000)>>36)*100+((myTime&0xF0000000000)>>40)*10+((myTime&0xF00000000000)>>44);
}

//when timer hits one minute increment to keep track of time
void Update_Time(){
    Crit_minutes(minutes+1,1);
    
    //when minute reaches 60 increments the hour
    if(Crit_minutes(0,0)==60){
        Crit_minutes(0,1);
        Crit_hours(hours+1,1);
    }  
  
    //when the hour hits 25 increment the day
    if(Crit_hours(0,0)==25){
        Crit_hours(0,1);
        Crit_day(day+1,1);    
    }
  
    //when days of the month is reached increment the month
    //isolating only months with 31 days
  if(Crit_month(0,0)==1||Crit_month(0,0)==3||Crit_month(0,0)==5|Crit_month(0,0)==7||Crit_month(0,0)==8||Crit_month(0,0)==10||Crit_month(0,0)==12){
      if(Crit_day(0,0)==32){
          Crit_day(0,1);
          Crit_month(month+1,1);
      }
   }
   
     //isolating only febuary
   if(Crit_month(0,0)==2){
      if(Crit_day(0,0)==29){
          Crit_day(0,1);
          Crit_month(month+1,1);
      }
   }
   
     //isolating months with 30 days
   if(Crit_month(0,0)==4||Crit_month(0,0)==6||Crit_month(0,0)==9||Crit_month(0,0)==11){
      if(Crit_day(0,0)==31){
          Crit_day(0,1);
          Crit_month(month+1,1);
      }
   }
   
     //increment the year
   if(Crit_month(0,0)==13){
     Crit_month(0,1);
     Crit_year(year++,1);    
   }
       
}

void Fetch_Sample(){
    int sample_to_fetch = 0;    //init val to fetch value
    char scan = 35; //init val store get char
    int count = 100;    //used to uce as a multiplier as dividion of 10 will allow for 3 loops
    move_cursor(1,0);   //allign cursor on terminal
    Erase_Line(); //clear the line of old text
    move_cursor(0, 0);  //allign sursor on terminal
    text_colour(1); //switch text colour to 
    printf("Printing paused "); //show printing has been paused 
    Crit_toggle(0,1); //let the MCU know to pause sampling
    text_colour(7); //set text colour to white
    printf("Enter a sample value to fetch from the FIFO (Eg, 004) #");  //instructions for user
    do{
        while(scan<48||scan>58){scan = getchar();}  //will only respond is a ascii number is inputed 0 - 9
            sample_to_fetch += (scan-48)*count; //subtract 48 to turn from ascii to int val, multiply by count to set the unit of the sample
            
            if(count == 100){   //only on first loop of fethcing a sample
                move_cursor(0, 73); //reallign cursor
            }
            
            count *= 0.1;   //uce count so the value will point to a new column of the sample id
            printf("%c",scan);  //print the first value of the sample we are fetching
            scan = 35;  //reset out scan value so it can be once again stuck in a loop
            Crit_fetched_sample(sample_to_fetch,1); //store the value for next run
            
            if(Crit_fetched_sample(0,0)>119){   //if the user input is outside of buffer range run the next block of code
                sample_to_fetch = 0;    //reset
                count = 100;                    //reset
                move_cursor(0, 0);      //reset
                text_colour(1);             //change text colour to 
                printf("Printing paused! ");    //show printing has been paused
                text_colour(7);             //change text colour to white
                printf("Enter a sample value to fetch from the FIFO (Eg, 004) #     ");  //reprint to clear old user text
            }
        }while(count>=1);   //when count is uced to 1 this means user has inputed 3 inputs
        
        fflush(stdin);  //clear buffer
        move_cursor(0, 0);  //reallign cursor
        text_colour(1); //change text colour to 
        printf("Printing paused! ");    //show that print has been resumed
        text_colour(7); //change text colour to white
        printf("Sample fetched from FIFO buffer #%d                                 \n",sample_to_fetch);   //display what sample has been fetched and print over old text
        move_cursor(1,0);   //reallign cursor
        text_colour(1); //change text colour to 
        Crit_toggle(1,1);   //tell MCU to start sampling again
        printf("Printing resume  ");    //show that sampling has been resumed
        text_colour(7); //change text colour to white
        //print all details of the fetched sample
        printf("Date: %d/%d/%d  Time: %d:%d LUX: %6.1f, mBar: %.2f, temp Deg(C): %6.1f Fifo slot: %d\r\n" ,Crit_day_sample(0,0,sample_to_fetch),Crit_month_sample(0,0,sample_to_fetch),Crit_year_sample(0,0,sample_to_fetch),Crit_hours_sample(0,0,sample_to_fetch),Crit_minutes_sample(0,0,sample_to_fetch),Crit_LDR_sample(0,0,sample_to_fetch),Crit_Pressure_sample(0,0,sample_to_fetch),Crit_Temp_sample(0,0,sample_to_fetch),Crit_number_sample(0,0,sample_to_fetch));
        text_colour(sample_print_colour);   //change text colour back to what it was before sampling was interrupted
        Fetch = false;  //lower fetch flag 
}

void print_debug_state(){
    
    move_cursor(27,81); //reallign cursor
    text_colour(3);         //change text colour to yellow
    if(Crit_debuger(0,0)==false)
        printf("Debug = off: toggle = R");
    if(Crit_debuger(0,0)==true)
        printf("Debug =  on: toggle = R");
    text_colour(sample_print_colour);   //change text colour back to what it was before sampling was interrupted
}

void FIFO_Deleted(){
    text_colour(1); //set text colour to 
    move_cursor(0,0);   //allign cursor position
    Erase_Line();   //clear the line that the cursor in on
    printf("Fifo has been erased\n\r"); //show user that fifo has been erased
    Erase_Line();   //clear the line of text
    text_colour(sample_print_colour);   //change text colour back to what it was before sampling was interrupted
    Crit_erase(0,1);    //lower erase flag
    sample_num = 0;     //set sample num pointer back to zero
}

void Print_Sampling_Paused(){
    move_cursor(0,0);   //allign cursor
    text_colour(1); //set text colour to 
    Erase_Line();   //clear the line of text
    printf("Sampling has been paused \n\r");    //show user that the sampling has been paused
    Erase_Line();   //clear the line of text    
    text_colour(sample_print_colour);   ////change text colour back to what it was before sampling was interrupted
    Crit_show_sampling_paused(0,1); //lower flag
}

void Print_Sampling_Resume(){
    move_cursor(0,0);   //allign cursor
    text_colour(1); //change text colour to 
    Erase_Line();   //erase text from the line
    printf("Sampling has been resumed \n\r"); //show user that the sampling has been resumed
    Erase_Line();   //clear the line of text
    text_colour(sample_print_colour);   //change text colour back to what it was before sampling was interrupted
    Crit_show_sampling_resume(0,1); //lower flag
}

void Putty_Set_Time(void){
    //init char values
    uint8_t new_minute = 0;
    uint8_t new_hour = 0;
    uint8_t new_day = 0;
    uint8_t new_month = 0;
    uint16_t new_year = 0;
    char scan = 0;      //init scan value to somthing that wont effect the ascii reading loop
    move_cursor(0,0);   //allign cursor
    text_colour(1);     //change text colour to 
    Erase_Line();           //clear line of text
    printf("Printing paused, specify a new time and date\r\n"); //give user intstruction
    Erase_Line();   //c;ear line of text
    printf("dd/mm/yyyy hh:mm\r");   //print a place holder to show how to format the data
    text_colour(7); //set text colour to white
    
    for(uint8_t count = 0; count <= 11; count++){
        fflush(stdin);
        
        if(count==2||count==4) //for when a / needs to be printed
            printf("/");
        if(count==8)    //for when a space needs to be ente
            printf(" ");
        if(count==10)   // for when a : needs to be ente
            printf(":");   
            
        while(scan==0){ //get stuck into loop
            scan = getchar();   //poll
            if (scan<48||scan>58){  //if non ascii number entered
                scan=0; //reset scan
            }
        }
        
        printf("%c",scan);  //print user input to the terminal
        fflush(stdout);//flush the buffer
            
        //subtract 48 to convert from ascii to raw
        if(count==0){new_day = (scan-48)*10;}   //muliply by 10 to place in 10s column
        if(count==1){new_day += (scan-48);}     //no multiply needed
        if(count==2){new_month = (scan-48)*10;}
        if(count==3){new_month += (scan-48);}
        if(count==4){new_year = (scan-48)*1000;}    //muliply by 1000 to place in 10s column
        if(count==5){new_year += (scan-48)*100;}    //muliply by 100 to place in 10s column
        if(count==6){new_year += (scan-48)*10;}
        if(count==7){new_year += (scan-48);}
        if(count==10){new_minute = (scan-48)*10;}
        if(count==11){new_minute += (scan-48);}
        if(count==8){new_hour = (scan-48)*10;}
        if(count==9){new_hour += (scan-48);}
        scan = 0;   //reset scan value
    }
    
    move_cursor(0,0);   //reallign cursor
    text_colour(1); //set text colour to 
    Erase_Line();   //erase old text from the line
    printf("New time and date has been specified \r\n");    //text for the user
    Erase_Line();   //remove old text from line
    printf("print resume!\r");  //show that print has been resumed
    fflush(stdin);  //flush the buffer
    
    //write values to store the time and date of each sample
    Crit_minutes(new_minute,1);
    Crit_hours(new_hour,1);
    Crit_day(new_day,1);
    Crit_month(new_month,1);
    Crit_year(new_year,1);
    text_colour(sample_print_colour);       //set sampling colour back to what it was before the sampling was interrupted
    //lower flag
    Crit_set_t_d(0,1);
}

bool sdflip = false;

void flip(){
    sdflip = true; //for when user button has been pressed
}


void SD(){
    //Create a filing system for SD Card
    button1.fall(&flip);    //trigger interrupt on rising edge
    FATFileSystem::format(&sd); 
    FATFileSystem fs("sd", &sd); 
    //fs.mount(&bd);  
    uint8_t sd_counter = 0;   //init variable
    string sample;    //define string
    bool buttonmode = false;    //set button mode off
    char holder[10];  //array of chars
    Timer SD; //create a timer
    Timer Debug;
    SD.start();   //start timer
    
    IWDG_HandleTypeDef hiwdg;
    
    if ( sd.init() != 0) {  //check if init worked ok reurn zero if not
        yellowLED = 1;
        //HAL_IWDG_Refresh(&hiwdg);
        //printf("%d\n",sd.init());
    }
    
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_256; 
    hiwdg.Init.Reload = 4095;  
    //watchdog refresh rate at (256*4095)/(32*10^3)  = 32.768 seconds             
    HAL_IWDG_Init(&hiwdg);
    
    while(1){
        
        while(sdflip==false){
            if(Crit_debuger(0,0)==true){
                if(Debug.read()>1){
                    debug(2);
                    Debug.stop();
                    Debug.reset();
                }
            }
            if(SD.read()>10){   //start saving when timer has hit 10 seconds
                HAL_IWDG_Refresh(&hiwdg);
            
                if(buttonmode == false){
                    FILE* fp = fopen("/sd/test.txt","a");   //edit test.txt file
                    while(sd_counter!=sample_num){  
                        if(Crit_debuger(0,0)==true){
                            debug(1);
                            Debug.start();
                        }
                                        
                                            
                        //add to string text and values
                        sample = "Date: ";
                        sprintf(holder,"%d",Crit_day_sample(0,0,sd_counter));
                        sample += holder;           
                        sample += "/";
                        sprintf(holder,"%d",Crit_month_sample(0,0,sd_counter));
                        sample += holder;           
                        sample += "/";
                        sprintf(holder,"%d",Crit_year_sample(0,0,sd_counter));
                        sample += holder;           
                        sample += "  Time: ";
                        sprintf(holder,"%d",Crit_hours_sample(0,0,sd_counter)); 
                        sample += holder;           
                        sample += ":";
                        sprintf(holder,"%d",Crit_minutes_sample(0,0,sd_counter));   
                        sample += holder;           
                        sample += "  LUX: ";
                        sprintf(holder,"%f",Crit_LDR_sample(0,0,sd_counter));
                        sample += holder;           
                        sample += ", mBar: ";
                        sprintf(holder,"%f",Crit_Pressure_sample(0,0,sd_counter));
                        sample += holder;           
                        sample += ", temp Deg(C): ";
                        sprintf(holder,"%f",Crit_Temp_sample(0,0,sd_counter));
                        sample += holder;           
                        sample += " Fifo slot: ";
                        sprintf(holder,"%d",Crit_number_sample(0,0,sd_counter));
                        sample += holder;   
                        sample += "\r\n"; 
                        fprintf(fp, "%s",sample.data());              //Print response
                                                    
                        //add 1 to counter so that it can refrence to the buffer
                        sd_counter++;
                        fclose(fp); //close sd card
                        
                        if(sd_counter==120) //sd counter is chasing the value of the buffer current value, force premature overflow.
                            sd_counter=0;
                    }
                    SD.reset(); //reset timer
                }
            }
        }
        
        if(sdflip==true){
            buttonmode=!buttonmode;
            if(buttonmode==true){
                greenLED  = 1;
                sd.deinit();
                SD.stop();
                SD.reset();
            }
            else{
                greenLED  = 0;
                sd.init();
                SD.start();
                sd_counter=sample_num;
            }
            sdflip=false;
        }
    }
}

void BD(){
    //Create a filing system for SD Card
    button1.fall(&flip);    //trigger interrupt on rising edge
    FATFileSystem::format(&bd);  
    uint8_t sd_counter = 0;   //init variable
    string sample;    //define string
    bool buttonmode = false;    //set button mode off
    char holder[10];  //array of chars
    Timer SD; //create a timer
    Timer Debug;
    SD.start();   //start timer
    
    if ( fs.mount(&bd) != 0) {  //check if init worked ok reurn zero if not
        yellowLED = 1;
        //printf("%d\n",sd.init());
    }
    
    IWDG_HandleTypeDef hiwdg;
    
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_256; 
    hiwdg.Init.Reload = 4095;  
    //watchdog refresh rate at (256*4095)/(32*10^3)  = 32.768 seconds             
    HAL_IWDG_Init(&hiwdg);
    
    while(1){
        while(sdflip == false){   
            if(Crit_debuger(0,0)==true){
                if(Debug.read()>1){
                    debug(2);
                    Debug.stop();
                    Debug.reset();
                }
            }
            if(SD.read()>10){   //start saving when timer has hit 10 seconds
                HAL_IWDG_Refresh(&hiwdg);
                 
                if(buttonmode == false){
                    FILE* fd = fopen("/fs/test.txt","a");   //edit test.txt file
                    while(sd_counter!=sample_num){  
                        if(Crit_debuger(0,0)==true){
                            debug(1);
                            Debug.start();
                        }
                                            
                                                
                        //add to string text and values
                        sample = "Date: ";
                        sprintf(holder,"%d",Crit_day_sample(0,0,sd_counter));
                        sample += holder;           
                        sample += "/";
                        sprintf(holder,"%d",Crit_month_sample(0,0,sd_counter));
                        sample += holder;           
                        sample += "/";
                        sprintf(holder,"%d",Crit_year_sample(0,0,sd_counter));
                        sample += holder;           
                        sample += "  Time: ";
                        sprintf(holder,"%d",Crit_hours_sample(0,0,sd_counter)); 
                        sample += holder;           
                        sample += ":";
                        sprintf(holder,"%d",Crit_minutes_sample(0,0,sd_counter));   
                        sample += holder;           
                        sample += "  LUX: ";
                        sprintf(holder,"%f",Crit_LDR_sample(0,0,sd_counter));
                        sample += holder;           
                        sample += ", mBar: ";
                        sprintf(holder,"%f",Crit_Pressure_sample(0,0,sd_counter));
                        sample += holder;           
                        sample += ", temp Deg(C): ";
                        sprintf(holder,"%f",Crit_Temp_sample(0,0,sd_counter));
                        sample += holder;           
                        sample += " Fifo slot: ";
                        sprintf(holder,"%d",Crit_number_sample(0,0,sd_counter));
                        sample += holder;   
                        sample += "\r\n"; 
                        fprintf(fd, "%s",sample.data());              //Print response
                                                    
                        //add 1 to counter so that it can refrence to the buffer
                        sd_counter++;
                        fclose(fd); //close sd card
                        
                        if(sd_counter==120) //sd counter is chasing the value of the buffer current value, force premature overflow.
                            sd_counter=0;
                    }
                    SD.reset(); //reset timer
                }
            }
        }
        if(sdflip==true){
            buttonmode=!buttonmode;
            if(buttonmode==true){
                greenLED  = 1;
                //sd.deinit();
                //SD.stop();
                //SD.reset();
            }
            else{
                greenLED  = 0;
                //sd.init();
                //SD.start();
                //sd_counter=sample_num;
            }
            sdflip=false;
        }
    }
}

void debug(uint8_t inputarg){
    switch(inputarg){
        case(1):
            myleds = true;
            break;
        case(2):
            myleds = false;
            break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//critical lock sections

double Crit_Lux(double write,bool rw) { //if rw = 0 write is unused
    {   
        CriticalSectionLock lock;   //create lock
        if(rw==true){   //if true overwrite lux with write argument
            LUX = write*0.0073076923f;
            return 0;   //call destructor
        }
        else{
            return LUX; //function is being used to read and not write return lux and call destructor
        }
    }
}

float Crit_temp(float write,bool rw) {
    {   
        CriticalSectionLock lock;
        if(rw==true){
            temp = write;
            return 0;
        }
        else{
            return temp;
        }
    }
}

float Crit_pressure(float write,bool rw) {
    {   
        CriticalSectionLock lock;
        if(rw==true){
            pressure = write;
            return 0;
        }
        else{
            return pressure;
        }
    }
}

float Crit_LDR_sample(float write,bool rw, uint8_t pointer) {//if rw = 0 write is unused
    {   
        CriticalSectionLock lock;       //create lock
        if(rw==true){       //if true overwrite lux with write argument
            LDR_sample[pointer] = write;    //pointer argument to select element of the array
            return 0;       //call destructor
        }
        else{
            return LDR_sample[pointer];     //function is being used to read and not write return lux and call destructor
        }
    }
}

float Crit_Pressure_sample(float write,bool rw, uint8_t pointer) {
    {   
        CriticalSectionLock lock;
        if(rw==true){
            Pressure_sample[pointer] = write;
            return 0;
        }
        else{
            return Pressure_sample[pointer];
        }
    }
}
 
float Crit_Temp_sample(float write,bool rw, uint8_t pointer) {
    {   
        CriticalSectionLock lock;
        if(rw==true){
            Temp_sample[pointer] = write;
            return 0;
        }
        else{
            return Temp_sample[pointer];
        }
    }
}

uint32_t Crit_sample_rate(uint32_t write,bool rw) {
    {   
        CriticalSectionLock lock;
        if(rw==true){
            sample_rate = write;
            return 0;
        }
        else{
            return sample_rate;
        }
    }
}


uint32_t Crit_old_samplerate(uint32_t write,bool rw) {
    {   
        CriticalSectionLock lock;
        if(rw==true){
            old_samplerate = write;
            return 0;
        }
        else{
            return old_samplerate;
        }
    }
}
 
uint16_t Crit_year(uint16_t write,bool rw) {
    {   
        CriticalSectionLock lock;
        if(rw==true){
            year = write;
            return 0;
        }
        else{
            return year;
        }
    }
}
 
uint8_t Crit_fetched_sample(uint8_t write,bool rw) {
    {   
        CriticalSectionLock lock;
        if(rw==true){
            fetched_sample = write;
            return 0;
        }
        else{
            return fetched_sample;
        }
    }
}
 
uint8_t Crit_number_sample(uint8_t write,bool rw, uint8_t pointer) {
    {   
        CriticalSectionLock lock;
        if(rw==true){
            number_sample[pointer] = write;
            return 0;
        }
        else{
            return number_sample[pointer];
        }
    }
}
 
uint8_t Crit_day_sample(uint8_t write,bool rw, uint8_t pointer) {
    {   
        CriticalSectionLock lock;
        if(rw==true){
            day_sample[pointer] = write;
            return 0;
        }
        else{
            return day_sample[pointer];
        }
    }
}
 
uint8_t Crit_month_sample(uint8_t write,bool rw, uint8_t pointer) {
    {   
        CriticalSectionLock lock;
        if(rw==true){
            month_sample[pointer] = write;
            return 0;
        }
        else{
            return month_sample[pointer];
        }
    }
}
 
uint8_t Crit_year_sample(uint8_t write,bool rw, uint8_t pointer) {
    {   
        CriticalSectionLock lock;
        if(rw==true){
            year_sample[pointer] = write;
            return 0;
        }
        else{
            return year_sample[pointer];
        }
    }
}

uint8_t Crit_hours_sample(uint8_t write,bool rw, uint8_t pointer) {
    {   
        CriticalSectionLock lock;
        if(rw==true){
            hours_sample[pointer] = write;
            return 0;
        }
        else{
            return hours_sample[pointer];
        }
    }
}

uint8_t Crit_minutes_sample(uint8_t write,bool rw, uint8_t pointer) {
    {   
        CriticalSectionLock lock;
        if(rw==true){
            minutes_sample[pointer] = write;
            return 0;
        }
        else{
            return minutes_sample[pointer];
        }
    }
}

uint8_t Crit_minutes(uint8_t write,bool rw) {
    {   
        CriticalSectionLock lock;
        if(rw==true){
            minutes = write;
            return 0;
        }
        else{
            return minutes;
        }
    }
}

uint8_t Crit_hours(uint8_t write,bool rw) {
    {   
        CriticalSectionLock lock;
        if(rw==true){
            hours = write;
            return 0;
        }
        else{
            return hours;
        }
    }
}

uint8_t Crit_day(uint8_t write,bool rw) {
    {   
        CriticalSectionLock lock;
        if(rw==true){
            day = write;
            return 0;
        }
        else{
            return day;
        }
    }
}

uint8_t Crit_month(uint8_t write,bool rw) {
    {   
        CriticalSectionLock lock;
        if(rw==true){
            month = write;
            return 0;
        }
        else{
            return month;
        }
    }
} 

bool Crit_time_to_use_sample(bool write,bool rw) {
    {   
        CriticalSectionLock lock;
        if(rw==true){
            time_to_use_sample = write;
            return 0;
        }
        else{
            return time_to_use_sample;
        }
    }
}

bool Crit_toggle(bool write,bool rw) {
    {   
        CriticalSectionLock lock;
        if(rw==true){
            toggle = write;
            return 0;
        }
        else{
            return toggle;
        }
    }
}

bool Crit_Fetch(bool write,bool rw) {
    {   
        CriticalSectionLock lock;
        if(rw==true){
            Fetch = write;
            return 0;
        }
        else{
            return Fetch;
        }
    }
}

bool Crit_erase(bool write,bool rw) {
    {   
        CriticalSectionLock lock;
        if(rw==true){
            erase = write;
            return 0;
        }
        else{
            return erase;
        }
    }
}

bool Crit_show_sampling_resume(bool write,bool rw) {
    {   
        CriticalSectionLock lock;
        if(rw==true){
            show_sampling_resume = write;
            return 0;
        }
        else{
            return show_sampling_resume;
        }
    }
}

bool Crit_show_sampling_paused(bool write,bool rw) {
    {   
        CriticalSectionLock lock;
        if(rw==true){
            show_sampling_paused = write;
            return 0;
        }
        else{
            return show_sampling_paused;
        }
    }
}

bool Crit_set_t_d(bool write,bool rw) {
    {   
        CriticalSectionLock lock;
        if(rw==true){
            set_t_d = write;
            return 0;
        }
        else{
            return set_t_d;
        }
    }
}

bool Crit_debuger(bool write,bool rw){
    {   
        CriticalSectionLock lock;
        if(rw==true){
            debuger = write;
            return 0;
        }
        else{
            return debuger;
        }
    }
}

bool Crit_LCD_td(bool write,bool rw){
    {   
        CriticalSectionLock lock;
        if(rw==true){
            LCD_td = write;
            return 0;
        }
        else{
            return LCD_td;
        }
    }
}

void lcd_isr_b2(){
    b2 = true;
}

void lcd_isr_b3(){
    b3 = true;
}

void lcd_isr(){
    LCD_td = true;
}