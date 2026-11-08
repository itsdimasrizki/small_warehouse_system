#pragma once

// PIN ESP32 MASTER

// UART Komunikasi ke Slave (Serial2)
#define PIN_MASTER_UART_RX    26  // Terhubung ke TX Slave (GPIO 17)
#define PIN_MASTER_UART_TX    27  // Terhubung ke RX Slave (GPIO 16)

#define PIN_DHT_DATA          4   
#define PIN_MQ2_ANALOG        36  

#define PIN_BUZZER            13  

// LED Rak 1
#define PIN_RGB1_RED          23
#define PIN_RGB1_GREEN        22
#define PIN_RGB1_BLUE         21

// LED Rak 2 
#define PIN_RGB2_RED          19
#define PIN_RGB2_GREEN        18
#define PIN_RGB2_BLUE         5

// LED Rak 3
#define PIN_RGB3_RED          17
#define PIN_RGB3_GREEN        16
#define PIN_RGB3_BLUE         14

// PIN ESP32 SLAVE

// UART Komunikasi ke Master (Serial2)
#define PIN_SLAVE_UART_TX     17  // Terhubung ke RX Master (GPIO 26)
#define PIN_SLAVE_UART_RX     16  // Terhubung ke TX Master (GPIO 27)

// Ultrasonic Rak 1
#define PIN_SLAVE_TRIG_F1_L   27
#define PIN_SLAVE_ECHO_F1_L   34
#define PIN_SLAVE_TRIG_F1_R   14
#define PIN_SLAVE_ECHO_F1_R   35  

// Ultrasonic Rak 2
#define PIN_SLAVE_TRIG_F2_L   13
#define PIN_SLAVE_ECHO_F2_L   32  
#define PIN_SLAVE_TRIG_F2_R   4
#define PIN_SLAVE_ECHO_F2_R   33  

// Ultrasonic Rak 3
#define PIN_SLAVE_TRIG_F3_L   5
#define PIN_SLAVE_ECHO_F3_L   25  
#define PIN_SLAVE_TRIG_F3_R   18
#define PIN_SLAVE_ECHO_F3_R   26  

// Alias
#define PIN_TRIG_F1_LEFT      PIN_SLAVE_TRIG_F1_L
#define PIN_ECHO_F1_LEFT      PIN_SLAVE_ECHO_F1_L
#define PIN_TRIG_F1_RIGHT     PIN_SLAVE_TRIG_F1_R
#define PIN_ECHO_F1_RIGHT     PIN_SLAVE_ECHO_F1_R
#define PIN_TRIG_F2_LEFT      PIN_SLAVE_TRIG_F2_L
#define PIN_ECHO_F2_LEFT      PIN_SLAVE_ECHO_F2_L
#define PIN_TRIG_F2_RIGHT     PIN_SLAVE_TRIG_F2_R
#define PIN_ECHO_F2_RIGHT     PIN_SLAVE_ECHO_F2_R
#define PIN_TRIG_F3_LEFT      PIN_SLAVE_TRIG_F3_L
#define PIN_ECHO_F3_LEFT      PIN_SLAVE_ECHO_F3_L
#define PIN_TRIG_F3_RIGHT     PIN_SLAVE_TRIG_F3_R
#define PIN_ECHO_F3_RIGHT     PIN_SLAVE_ECHO_F3_R
