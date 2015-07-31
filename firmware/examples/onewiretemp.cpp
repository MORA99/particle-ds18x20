#include "ds18b20.h"
#include "onewire.h"
#include "crc8.h"
uint8_t sensors[80];

void log(char* msg)
{
    Spark.publish("log", msg);
    delay(500);
}

void setup()
{
    
}

void loop()
{
    uint8_t subzero, cel, cel_frac_bits;
    char msg[100];
    log("Starting measurement");    
    
    DS18X20_start_meas( DS18X20_POWER_PARASITE, NULL ); //Asks all DS18x20 devices to start temperature measurement, takes up to 750ms at max resolution
    delay(1000); //If your code has other tasks, you can store the timestamp instead and return when a second has passed.

    uint8_t numsensors = search_sensors(10, sensors);
    sprintf(msg, "Found %i sensors", numsensors);
    log(msg);

    
    for (uint8_t i=0; i<numsensors; i++)
    {
        if (sensors[i*OW_ROMCODE_SIZE+0] == 0x10 || sensors[i*OW_ROMCODE_SIZE+0] == 0x28)
        {
            //log("Found a DS18B20");
			uint8_t sensorID[OW_ROMCODE_SIZE];
			for (uint8_t o=0; o<OW_ROMCODE_SIZE; o++)
			{
				sensorID[o] = sensors[i*OW_ROMCODE_SIZE+o];
			}

			if ( DS18X20_read_meas( sensorID, &subzero, &cel, &cel_frac_bits) == DS18X20_OK ) {
				
				char sign = (subzero) ? '-' : '+';
				int frac = cel_frac_bits*DS18X20_FRACCONV;
				sprintf(msg, "Sensor# %d (%02X%02X%02X%02X%02X%02X%02X%02X) =  : %c%d.%04d\r\n",i+1,
				sensors[(i*OW_ROMCODE_SIZE)+0],
				sensors[(i*OW_ROMCODE_SIZE)+1],
				sensors[(i*OW_ROMCODE_SIZE)+2],
				sensors[(i*OW_ROMCODE_SIZE)+3],
				sensors[(i*OW_ROMCODE_SIZE)+4],
				sensors[(i*OW_ROMCODE_SIZE)+5],
				sensors[(i*OW_ROMCODE_SIZE)+6],
				sensors[(i*OW_ROMCODE_SIZE)+7],
				sign,
				cel,
				frac
				);
				log(msg);
			}
			else
			{
			    Spark.publish("log", "CRC Error (lost connection?)");
			}
        }
    }
    delay(10000);
}
