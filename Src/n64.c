#include <stdint.h>
#include <string.h>
#include "n64.h"
#include "stm32f4xx_hal.h"

N64ControllerData n64_data[4096];
N64ControllerData *tail;
N64ControllerData *current;

void my_wait_us_asm(int n);

void initialize_n64_buffer()
{
	memset(n64_data,0,sizeof(n64_data)); // clear controller state
	tail = n64_data;
	current = n64_data;
}

uint32_t readCommand()
{
	uint8_t retVal;

	// we are already at the first falling edge
	// get middle of first pulse, 2us later
	my_wait_us_asm(2);
	uint32_t command = (GPIOA->IDR & 0x0100) ? 1U : 0U, bits_read = 1;

    while(1) // read at least 9 bits (1 byte + stop bit)
    {
        command = command << 1; // make room for the new bit
        retVal = GetMiddleOfPulse();
        if(retVal == 5) // timeout
        {
        	if(bits_read >= 8)
        	{
				command = command >> 2; // get rid of the stop bit AND the room we made for an additional bit
				return command;
        	}
        	else // there is no possible way this can be a real command
        	{
        		return 5; // dummy value
        	}
        }
        command += retVal;

        bits_read++;

        if(bits_read >= 25) // this is the longest known command length
        {
        	command = command >> 1; // get rid of the stop bit (which is always a 1)
        	return command;
        }
    }
}

uint8_t GetMiddleOfPulse()
{
	uint8_t ct = 0;
    // wait for line to go high
    while(1)
    {
        if(GPIOA->IDR & 0x0100) break;

        ct++;
        if(ct == 200) // failsafe limit TBD
        	return 5; // error code
    }

    ct = 0;

    // wait for line to go low
    while(1)
    {
        if(!(GPIOA->IDR & 0x0100)) break;

        ct++;
		if(ct == 200) // failsafe limit TBD
			return 5; // error code
    }

    // now we have the falling edge

    // wait 2 microseconds to be in the middle of the pulse, and read. high --> 1.  low --> 0.
    my_wait_us_asm(2);

    return (GPIOA->IDR & 0x0100) ? 1U : 0U;
}

void SendIdentityN64()
{
    // reply 0x05, 0x00, 0x02
    SendByte(0x05);
    SendByte(0x00);
    SendByte(0x02);
    SendStop();
}

void SetN64DataInputMode()
{
	// port A8 to input mode
	GPIOA->MODER &= ~(1 << 17);
	GPIOA->MODER &= ~(1 << 16);
}

void SetN64DataOutputMode()
{
	// port A8 to output mode
	GPIOA->MODER &= ~(1 << 17);
	GPIOA->MODER |= (1 << 16);
}

void write_1()
{
	GPIOA->BSRR = (1 << 24);
	my_wait_us_asm(1);
	GPIOA->BSRR = (1 << 8);
    my_wait_us_asm(3);
}

void write_0()
{
	GPIOA->BSRR = (1 << 24);
	my_wait_us_asm(3);
	GPIOA->BSRR = (1 << 8);
    my_wait_us_asm(1);
}

void SendStop()
{
	GPIOA->BSRR = (1 << 24);
	my_wait_us_asm(1);
	GPIOA->BSRR = (1 << 8);
}

// send a byte from LSB to MSB (proper serialization)
void SendByte(unsigned char b)
{
    for(int i = 0;i < 8;i++) // send all 8 bits, one at a time
    {
        if((b >> i) & 1)
        {
            write_1();
        }
        else
        {
            write_0();
        }
    }
}

void SendControllerDataN64()
{
    unsigned long data = *(unsigned long*)&n64_data;
    unsigned int size = sizeof(data) * 8; // should be 4 bytes * 8 = 32 bits

    for(unsigned int i = 0;i < size;i++)
    {
        if((data >> i) & 1)
        {
            write_1();
        }
        else
        {
            write_0();
        }
    }

    SendStop();
}