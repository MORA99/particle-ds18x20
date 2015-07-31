/************************************************************************/
/*                                                                      */
/*        Access Dallas 1-Wire Device with ATMEL AVRs                   */
/*                                                                      */
/*              Author: Peter Dannegger                                 */
/*                      danni@specs.de                                  */
/*                                                                      */
/* modified by Martin Thomas <eversmith@heizung-thomas.de> 9/2004       */
/* modified by https://github.com/MORA99 2010-15                        */
/************************************************************************/

#include "application.h"
#include "onewire.h"

#ifdef OW_DEBUG
#  define debug_print(msg) Spark.publish("log", msg);
#else
#  define debug_print(msg) (void)0
#endif

uint8_t pin = D0 //Default

#define OW_GET_IN()   ( digitalRead(pin) )
#define OW_OUT_LOW()  ( digitalWrite(pin,LOW) )
#define OW_OUT_HIGH() ( digitalWrite(pin,HIGH) )
#define OW_DIR_IN()   ( pinMode(pin, INPUT) )
#define OW_DIR_OUT()  ( pinMode(pin, OUTPUT) )

void setPin(uint8_t newpin)
{
	pin = newpin;
}

void find_sensor(uint8_t *diff, uint8_t id[])
{
	for (;;) {
		*diff = ow_rom_search( *diff, &id[0] );
		if ( *diff==OW_PRESENCE_ERR || *diff==OW_DATA_ERR || *diff == OW_LAST_DEVICE ) return;
		return;
	}
}

uint8_t search_sensors(int maxSensors, uint8_t *sensorScan)
{
	uint8_t i;
	uint8_t id[OW_ROMCODE_SIZE];
	uint8_t diff, nSensors;
	
	ow_reset();
	
	nSensors = 0;
	
	for( diff = OW_SEARCH_FIRST; diff != OW_LAST_DEVICE && nSensors < maxSensors ; )
	{
		find_sensor( &diff, &id[0] );
		
		if( diff == OW_PRESENCE_ERR ) {
			debug_print("No Sensor found\r" );
			break;
		}
		
		if( diff == OW_DATA_ERR ) {
			debug_print("Bus Error\r" );
			break;
		}
		
		for (i=0;i<OW_ROMCODE_SIZE;i++)
		{
			sensorScan[nSensors*OW_ROMCODE_SIZE+i] = id[i];
		}
		/*
		char msg[100];
		sprintf(msg, "Found sensor: %02X%02X%02X%02X%02X%02X%02X%02X", sensorScan[nSensors*OW_ROMCODE_SIZE+0], sensorScan[nSensors*OW_ROMCODE_SIZE+1], sensorScan[nSensors*OW_ROMCODE_SIZE+2], sensorScan[nSensors*OW_ROMCODE_SIZE+3], sensorScan[nSensors*OW_ROMCODE_SIZE+4], sensorScan[nSensors*OW_ROMCODE_SIZE+5], sensorScan[nSensors*OW_ROMCODE_SIZE+6], sensorScan[nSensors*OW_ROMCODE_SIZE+7]);
		Spark.publish("log", msg);
		*/
		nSensors++;
	}
	
	return nSensors;
}

uint8_t ow_input_pin_state()
{
	return OW_GET_IN();
}

void ow_parasite_enable(void)
{
	OW_OUT_HIGH();
	OW_DIR_OUT();
}

void ow_parasite_disable(void)
{
	OW_OUT_LOW();
	OW_DIR_IN();
}

uint8_t ow_reset(void)
{
	uint8_t err;
	
	OW_OUT_LOW(); // disable internal pull-up (maybe on from parasite)
	OW_DIR_OUT(); // pull OW-Pin low for 480us
	
	delayMicroseconds(480);
	
	// set Pin as input - wait for clients to pull low
	OW_DIR_IN(); // input
	
	delayMicroseconds(66);
	err = OW_GET_IN();		// no presence detect
	// nobody pulled to low, still high	

	// after a delay the clients should release the line
	// and input-pin gets back to high due to pull-up-resistor
	delayMicroseconds(480-66);
	if( OW_GET_IN() == 0 )		// short circuit
	err = 1;
	
	return err;
}

uint8_t ow_read_bit()
{
	int result;

	OW_DIR_OUT(); // drive bus low
	delayMicroseconds(6);
	OW_DIR_IN();
	delayMicroseconds(6); //Recommended values say 9
	if( OW_GET_IN() == 0 )
	result = 0;
	else
	result=1; // sample at end of read-timeslot (Reading only possible with high bit)
	delayMicroseconds(58); // Complete the time slot and 10us recovery

	return result;
}

void ow_write_bit(uint8_t bit)
{
	if (bit)
	{
		// Write '1' bit
		OW_DIR_OUT(); // drive bus low
		delayMicroseconds(6);
		OW_DIR_IN();
		delayMicroseconds(64); // Complete the time slot and 10us recovery
	}
	else
	{
		// Write '0' bit
		OW_DIR_OUT(); // drive bus low
		delayMicroseconds(60);
		OW_DIR_IN();
		delayMicroseconds(10);
	}
}

void ow_byte_wr( uint8_t b )
{
	for (uint8_t i=8; i>0; i--)
	{
		ow_write_bit( b & 1 );
		b >>= 1;
	}
}


uint8_t ow_byte_rd( void )
{
	int loop, result=0;
	for (loop = 0; loop < 8; loop++)
	{
		// shift the result to get it ready for the next bit
		result >>= 1;

		// if result is one, then set MS bit
		if (ow_read_bit())
		result |= 0x80;
	}
	return result;
}


uint8_t ow_rom_search( uint8_t diff, uint8_t *id )
{
	uint8_t i, j, next_diff;
	uint8_t b;
	
	if( ow_reset() ) return OW_PRESENCE_ERR;	// error, no device found
	
	ow_byte_wr( OW_SEARCH_ROM );			// ROM search command
	next_diff = OW_LAST_DEVICE;			// unchanged on last device
	
	i = OW_ROMCODE_SIZE * 8;					// 8 bytes
	
	do {
		j = 8;					// 8 bits
		do {
			b = ow_read_bit();			// read bit
			if( ow_read_bit() ) {			// read complement bit
				if( b )					// 11
				return OW_DATA_ERR;			// data error
			}
			else {
				if( !b ) {				// 00 = 2 devices
					if( diff > i || ((*id & 1) && diff != i) ) {
						b = 1;				// now 1
						next_diff = i;			// next pass 0
					}
				}
			}
			ow_write_bit( b );     			// write bit
			*id >>= 1;
			if( b ) *id |= 0x80;			// store bit
			
			i--;
			
		} while( --j );
		
		id++;					// next byte
		
	} while( i );
	
	return next_diff;				// to continue search
}


void ow_command( uint8_t command, uint8_t *id )
{
	uint8_t i;

	ow_reset();

	if( id ) {
		ow_byte_wr( OW_MATCH_ROM );			// to a single device
		i = OW_ROMCODE_SIZE;
		do {
			ow_byte_wr( *id );
			id++;
		} while( --i );
	}
	else {
		ow_byte_wr( OW_SKIP_ROM );			// to all devices
	}
	
	ow_byte_wr( command );
}

