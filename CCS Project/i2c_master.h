/*
 * i2c_master.h
 *
 *  Created on: Oct 15, 2022
 *      Author: passp
 */

#ifndef I2C_MASTER_H_
#define I2C_MASTER_H_


//********** Prototypes **********//

void              i2c_init( void );
unsigned char USI_TWI_Start_Transceiver_With_Data(unsigned char, unsigned char * , unsigned char );
unsigned char USI_TWI_Get_State_Info( void );

unsigned char i2c_read(unsigned char slave,unsigned char addr, unsigned char *msg, unsigned char msgSize);

unsigned char i2c_write(unsigned char slave, unsigned char addr, const uint8_t *data , uint8_t size);


/// float pins and disable input buffers

void USI_TWI_Master_disable( void );

// Drive both pins low

void i2c_shutdown();


#endif /* I2C_MASTER_H_ */
