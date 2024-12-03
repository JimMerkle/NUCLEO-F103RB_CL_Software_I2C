# NUCLEO-F103RB_CL_Software_I2C
    
    Develop / Demonstrate a Software I2C Bus Master using GPIOs functioning
    as Open Collector / Open Drain.
    
## Development Board
    
    Using STMicro NUCLEO-F103RB


## DS3231 - Connections
    
    Connect DS3231 module to Soft I2C pins:
     PC0 (SCL)
     PC1 (SDA)
		
## 1us delay timer
    
    Using 16-bit timer, TIM4, to count 1us time increments
    This is to provide accurate delays for Soft I2C.
    Prescaler is adjusted such that timer increments once each microsecond.
    
## Using a Circular DMA buffer for Serial RX to prevent losing characters
    
## Using UART2 for Command Line serial I/O
    
    Command Line parser, Ver 1.1.0, Dec  3 2024
    Enter "help" or "?" for list of commands
    >help
    Help - command list
    Command     Comment
    ?           display help menu
    help        display help menu
    add         add <number> <number>
    id          unique ID
    info        processor info
    reset       reset processor
    version     display version
    timer       timer test - testing 50ms delay
    i2cscan     scan i2c bus for connected devices
    i2cwrite    test - write 0 to DS3231
    i2cread     test - read byte from DS3231
    
    Note: the "i2cwrite" and "i2cread" are used to generate waveforms
    on the connected SCL/SDA pins, to measure/validate correct functionality.
    
## Notes
    

