// Copyright Jim Merkle, 2/17/2020
// File: command_line.c
//
// Command Line Parser
//
// Using serial interface, receive commands with parameters.
// Parse the command and parameters, look up the command in a table, execute the command.
// Since the command/argument buffer is global with global pointers, each command will parse its own arguments.
// Since no arguments are passed in the function call, all commands will have int command_name(void) prototype.

// Notes:
// The stdio library's stdout stream is buffered by default.  This makes printf() and putchar() work strangely
// for character I/O.  Buffering needs to be disabled for this code module.  See setvbuf() in cl_setup().

#include <stdio.h> // printf()
#include <string.h>
#include <stdlib.h>
#include <stdint.h> // uint8_t
#include "command_line.h"
#include "main.h"   // HAL functions and defines
#include "soft_i2c.h"
#include "version.h"


// Typedefs
typedef struct {
  char * command;
  char * comment;
  int arg_cnt; // count of arguments plus command
  int (*function)(void); // pointer to command function
} COMMAND_ITEM;

int cl_i2c_scan(void);
int cl_i2c_write(void);
int cl_i2c_read(void);

const COMMAND_ITEM cmd_table[] = {
    {"?",         "display help menu",                            1, cl_help},
    {"help",      "display help menu",                            1, cl_help},
    {"add",       "add <number> <number>",                        3, cl_add},
    {"id",        "unique ID",                                    1, cl_id},
    {"info",      "processor info",                               1, cl_info},
    {"reset",     "reset processor",                              1, cl_reset},
	{"version",   "display version",                              1, cl_version},
    {"timer",     "timer test - testing 50ms delay",              1, cl_timer},
//	{"delaytest", "test microsecond delays",                      1, cl_timer_delay_test},
	{"i2cscan",   "scan i2c bus for connected devices",           1, cl_i2c_scan},
	{"i2cwrite",  "test - write 0 to DS3231",                     1, cl_i2c_write},
	{"i2cread",   "test - read byte from DS3231",                 1, cl_i2c_read},

    {NULL,NULL,0,NULL}, /* end of table */
};

// Globals:
char cmd_buffer[MAXSERIALBUF]; // holds command strings from user
char * argv[MAXWORDS]; // pointers into buffer
int argc; // number of words (command & arguments)

// Project version
const VERSION_MAJOR_MINOR fw_version = {VERSION_MAJOR,VERSION_MINOR,VERSION_BUILD};
char szversion[16];

void cl_setup(void) {
    // The STM32 development environment's stdio library provides buffering of stdout stream by default.  Turn it off!
    setvbuf(stdout, NULL, _IONBF, 0);
    // Write version string
    sprintf(szversion,"Ver %u.%u.%u",fw_version.major,fw_version.minor,fw_version.build);
    // Turn on yellow text, print greeting, reset attributes
    printf("\n" COLOR_YELLOW "Command Line parser, %s, %s" COLOR_RESET "\n",szversion,__DATE__);
    printf(COLOR_YELLOW "Enter \"help\" or \"?\" for list of commands" COLOR_RESET "\n");
    __io_putchar('>'); // initial prompt
}

// Externals
int __io_getchar(void);   // main.c
int __io_putchar(int ch); // main.c

// Check for data available from USART interface.  If none present, just return.
// If data available, process it (add it to character buffer if appropriate)
void cl_loop(void)
{
    static int index = 0; // index into global buffer
    int c;

    // Spin, reading characters until EOF character is received (no data), buffer is full, or
    // a <line feed> character is received.  Null terminate the global string, don't return the <LF>
    while(1) {
      c = __io_getchar();
      switch(c) {
          case EOF:
              return; // non-blocking - return
          case _CR:
          case _LF:
        	  cmd_buffer[index] = 0; // null terminate
            if(index) {
        		putchar(_LF); // newline
            	cl_process_buffer(); // process the null terminated buffer
            }
    		printf("\n>");
            index = 0; // reset buffer index
            return;
          case _BS:
            if(index<1) continue;
            printf("\b \b"); // remove the previous character from the screen and buffer
            index--;
            break;
          default:
        	if(index<(MAXSERIALBUF - 1) && c >= ' ' && c <= '~') {
				putchar(c); // write character to terminal
				cmd_buffer[index] = (char)c;
				index++;
        	}
      } // switch
  } // while(1)
  return;
}

void cl_process_buffer(void)
{
    argc = cl_parseArgcArgv(cmd_buffer, argv, MAXWORDS);
    // Display each of the "words" / command and arguments
    //for(int i=0;i<argc;i++)
    //  printf("%d >%s<\n",i,argv[i]);
    if (argc) {
        // At least one "word" / argument found
        // See if command has a match in the command table
        // If null function pointer found, exit for-loop
        int cmdIndex;
        for (cmdIndex = 0; cmd_table[cmdIndex].function; cmdIndex++) {
            if (strcmp(argv[0], cmd_table[cmdIndex].command) == 0) {
                // We found a match in the table
                // Enough arguments?
                if (argc < cmd_table[cmdIndex].arg_cnt) {
                    printf("\r\nInvalid Arg cnt: %d Expected: %d\n", argc - 1,
                            cmd_table[cmdIndex].arg_cnt - 1);
                    break;
                }
                // Call the function associated with the command
                (*cmd_table[cmdIndex].function)();
                break; // exit for-loop
            }
        } // for-loop
          // If we compared all the command strings and didn't find the command, or we want to fake that event
        if (!cmd_table[cmdIndex].command) {
            printf("Command \"%s\" not found\r\n", argv[0]);
        }
    } // At least one "word" / argument found
}

// Return true (non-zero) if character is a white space character
int cl_isWhiteSpace(char c) {
  if(c==' ' || c=='\t' ||  c=='\r' || c=='\n' )
    return 1;
  else
    return 0;
}

// Parse string into arguments/words, returning count
// Required an array of char pointers to store location of each word, and number of strings available
// "count" is the maximum number of words / parameters allowed
int cl_parseArgcArgv(char * inBuf,char **words, int count)
{
  int wordcount = 0;
  while(*inBuf) {
    // We have at least one character
    while(cl_isWhiteSpace(*inBuf)) inBuf++; // remove leading whitespace
    if(*inBuf) {// have a non-whitespace
      if(wordcount < count) {
        // If pointing at a double quote, need to remove/advance past the first " character
        // and find the second " character that goes with it, and remove/advance past that one too.
        if(*inBuf == '\"' && inBuf[1]) {
            // Manage double quoted word
            inBuf++; // advance past first double quote
            words[wordcount]=inBuf; // point at this new word
            wordcount++;
            while(*inBuf && *inBuf != '\"') inBuf++; // move to end of word (next double quote)
        } else {
            // normal - not double quoted string
            words[wordcount]=inBuf; // point at this new word
            wordcount++;
            while(*inBuf && !cl_isWhiteSpace(*inBuf)) inBuf++; // move to end of word
        }
        if(cl_isWhiteSpace(*inBuf) || *inBuf == '\"') { // null terminate this word
          *inBuf=0;
          inBuf++;
        }
      } // if(wordcount < count)
      else {
        *inBuf=0; // null terminate string
        break; // exit while-loop
      }
    }
  } // while(*inBuf)
  return wordcount;
} // parseArgcArgv()

#define COMMENT_START_COL  12  //Argument quantity displayed at column 12
// We may want to add a comment/description field to the table to describe each command
int cl_help(void) {
    printf("Help - command list\r\n");
    printf("Command     Comment\r\n");
    // Walk the command array, displaying each command
    // Continue until null function pointer found
    for (int i = 0; cmd_table[i].function; i++) {
        printf("%s", cmd_table[i].command);
        // insert space depending on length of command
        unsigned cmdlen = strlen(cmd_table[i].command);
        for (unsigned j = COMMENT_START_COL; j > cmdlen; j--)
            printf(" "); // variable space so comment fields line up
        printf("%s\r\n", cmd_table[i].comment);
    }
    printf("\n");
    return 0;
}

// This function is included here as a template - example of how to create / add your own command
int cl_add(void) {
    printf("add..  A: %s  B: %s\n", argv[1], argv[2]);
    int A = (int) strtol(argv[1], NULL, 0); // allow user to use decimal or hex
    int B = (int) strtol(argv[2], NULL, 0);
    int ret = A + B;
    printf("returning %d\n\n", ret);
    return ret;
}

//Unique device ID register (96 bits)
int cl_id(void) {
    volatile uint8_t *p_id = (uint8_t*) UID_BASE; // stm32f091xc.h
    printf("Unique ID: 0x");
    for (int i = 11; i >= 0; i--)
        printf("%02X", p_id[i]); // display bytes in from high byte to low byte

    printf("\n");
    return 0;
}

//Memory size register
//Contains number of K bytes of FLASH, IE: 0x80 = 128K bytes flash

//MCU device ID code
//Only 32-bits access supported. Read-only.
int cl_info(void) {
    volatile uint16_t *p_k_bytes = (uint16_t*) FLASHSIZE_BASE; // stm32f091xc.h
    volatile uint32_t *p_dev_id = (uint32_t*) DBGMCU_BASE; // stm32f091xc.h
    printf("Processor Flash: %uK bytes\n", *p_k_bytes);
    printf("Processor ID Code: 0x%08lX\n", *p_dev_id);
    return 0;
}


// Return appropriate error code string
// Yes, we could just return the HAL strings vs copy them...
char * PrintHalStatus(int status)
{
    static char s_status[4];
    switch(status) {
        case HAL_OK:
          //sprintf(s_status,"HAL_OK");
          return "HAL_OK";
          break;
        case HAL_ERROR:
          //sprintf(s_status,"HAL_ERROR");
          return "HAL_ERROR";
          break;
        case HAL_BUSY:
          //sprintf(s_status,"HAL_BUSY");
          return "HAL_BUSY";
          break;
        case HAL_TIMEOUT:
          //sprintf(s_status,"HAL_TIMEOUT");
          return "HAL_TIMEOUT";
          break;
        default:
          sprintf(s_status,"%d",status);
          break;
    } // switch
    return s_status;
} // PrintHalStatus()

// Reset the processor
int cl_reset(void) {
    NVIC_SystemReset(); // CMSIS Cortex-M3 function - see Drivers/CMSIS/Include/core_cm3.h
    while (1) ; // wait here until reset completes

    return 0;
}

// Display version
int cl_version(void)
{
	printf("%s\n",szversion);
	return 0;
}


// Perform a timer4 test.
// Timer2 is a 16-bit free-running timer with pre-scale counter.
// Is the us timer tracking System Ticks?
// Timer2 is configured to increment each micro-second
// Alternatively, if used a GPIO, we could toggle a pin after X micro-seconds
int cl_timer(void)
{
    printf("%s(), Timing HAL_Delay(50)\n",__func__);
    volatile TIM_TypeDef *TIMx = TIM4; // Use timer 4
    uint32_t start_ticks = HAL_GetTick();
    uint16_t start_us = TIMx->CNT; // read us hardware timer
    HAL_Delay(50); // delay 50us
    uint16_t stop_us = TIMx->CNT; // read us hardware timer
    uint32_t stop_ticks = HAL_GetTick();
    // Report results
    printf("HAL_GetTick() time: %lu ms\n",stop_ticks-start_ticks);
    printf("TIMx->CNT time: %u us\n",(uint16_t)(stop_us - start_us));
    return 0;
}

// Using a 16 bit timer spin-delay a quantity of micro-seconds
// Timer is configured to increment each micro-second
// This function appears to work perfectly at 64-72MHz system clock, always returning 1000us, when 1000us was requested
//  - Release build only.  Debug build runs noticeably slower, returning values greater than what was expected.
// With 16MHz system clock and 8MHz peripheral clock, the delta times are 1000, 1001, and 1019 when systick interrupts fire
// With 8MHz system clock and 8MHz peripheral clock, the delta times are 1000, 1002, and 1033, 1036, 1038 when systick interrupts fire
uint16_t timer_delay_us(uint16_t delay_us)
{
    //printf("%s(%lu)\n",__func__,delay_us);
    volatile TIM_TypeDef *TIMx = TIM4; // Establish pointer to timer 4 registers
    uint16_t start_us = TIMx->CNT; // function entry count
    uint16_t delta;
    do {
    	delta = TIMx->CNT - start_us;
    } while(delta < delay_us);

    return delta;
}

#if 0
// Comment out the following define to use the non-array method
//#define USEARRAY	1

// Test timer_delay_us() function
int cl_timer_delay_test(void)
{
    printf("%s()\n",__func__);
#ifdef USEARRAY
    // Use array to collect and then display the results of 1024 tests
    uint16_t delay_results[1024];
    uint16_t i;

	// collect results from 1024 1 ms delays (1 second or so)
	for(i=0; i<1024; i++)
		delay_results[i] = timer_delay_us(1000); // 1ms delay

	// When using array, dump the array contents
	for(i=0; i<1024; i++)
		printf("%u:%u%s\n",i,delay_results[i],delay_results[i]<=1002?"":" <======="); // display marker for larger values

#else
    	// Analyze the delta time returned
        uint16_t delta;
        uint16_t i;

        // For 60 seconds, test the timer_delay_us timer, looking for a delta that isn't 1000us
        // 60 seconds count down
        for(int seconds=59; seconds >= 0; seconds--) {
        	// 1024 1 ms delays (1 second or so)
        	for(i=0; i<1024; i++) {
    			delta = timer_delay_us(1000); // 1ms delay
    			if(delta > 1000) {
    				printf("Not 1000us: %u\n",delta);
    				return 1;
    			}
        	}
        	printf("\b\b  \b\b%d",seconds); // seconds count down - erase previous display each time
        }
        printf("\b \n"); // erase the remaining '0', then line feed
        printf("60 seconds worth of 1000us delays - each delay returned 1000us!\n");
#endif

    return 0;
}
#endif
