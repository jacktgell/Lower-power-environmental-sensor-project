
#ifndef _THREADS_H_
#define _THREADS_H_

#define HTTP_STATUS_LINE "HTTP/1.0 200 OK"
#define HTTP_HEADER_FIELDS "Content-Type: text/html; charset=utf-8"
#define HTTP_MESSAGE_BODY1 ""                                    \

#define HTTP_MESSAGE_BODY2 ""                                    \
       "</p>" "\r\n"                                             \
"    </div>" "\r\n"                                              \
"  </body>" "\r\n"                                               \
"</html>"

#define HTTP_RESPONSE HTTP_STATUS_LINE "\r\n"   \
                      HTTP_HEADER_FIELDS "\r\n" \
                      "\r\n"                    \
                      HTTP_MESSAGE_BODY "\r\n"

#define IP        "10.0.0.10"
#define NETMASK   "255.0.0.0"
#define GATEWAY   "10.0.0.1"

//prototypes
void Timers(void);
void Ethernet(void);
void Print_Data(void); 
void text_colour(int colour);
void move_cursor(int row, int col);
void Terminal_Init(void);
void Update_Samplerate(void);
void Read_Sensors(void); 
void Scan_Samplerate(void);
void Scanf(void);
void SampleRateisr(void);
void attachtickerfreq(void);
void fifo(void);
void Time_Date_Init(void);
void Update_Time(void);
void Fetch_Sample(void);
void FIFO_Deleted(void);
void Print_Sampling_Paused(void);
void Print_Sampling_Resume(void);
void Putty_Set_Time(void);
void SD(void);
void BD(void);
void debug(uint8_t inputarg);
void Erase_Line(void);
void print_debug_state(void);
void lcd_isr_b2(void);
void lcd_isr_b3(void);
void lcd_isr(void);
double Crit_Lux(double write,bool rw);
float Crit_temp(float write,bool rw);
float Crit_pressure(float write,bool rw);
float Crit_LDR_sample(float write,bool rw, uint8_t pointer);
float Crit_Pressure_sample(float write,bool rw, uint8_t pointer);
float Crit_Temp_sample(float write,bool rw, uint8_t pointer);
uint32_t Crit_sample_rate(uint32_t write,bool rw);
uint32_t Crit_old_samplerate(uint32_t write,bool rw);
uint16_t Crit_year(uint16_t write,bool rw);
uint8_t Crit_fetched_sample(uint8_t write,bool rw);
uint8_t Crit_number_sample(uint8_t write,bool rw, uint8_t pointer);
uint8_t Crit_day_sample(uint8_t write,bool rw, uint8_t pointer);
uint8_t Crit_month_sample(uint8_t write,bool rw, uint8_t pointer);
uint8_t Crit_year_sample(uint8_t write,bool rw, uint8_t pointer);
uint8_t Crit_hours_sample(uint8_t write,bool rw, uint8_t pointer);
uint8_t Crit_minutes_sample(uint8_t write,bool rw, uint8_t pointer);
uint8_t Crit_minutes(uint8_t write,bool rw);
uint8_t Crit_hours(uint8_t write,bool rw);
uint8_t Crit_day(uint8_t write,bool rw);
uint8_t Crit_month(uint8_t write,bool rw);
bool Crit_time_to_sample(bool write,bool rw);
bool Crit_time_to_use_sample(bool write,bool rw);
bool Crit_toggle(bool write,bool rw);
bool Crit_Fetch(bool write,bool rw);
bool Crit_erase(bool write,bool rw);
bool Crit_show_sampling_resume(bool write,bool rw);
bool Crit_show_sampling_paused(bool write,bool rw);
bool Crit_set_t_d(bool write,bool rw);
bool Crit_debuger(bool write,bool rw);
bool Crit_LCD_td(bool write,bool rw);
class LCD;

#endif
