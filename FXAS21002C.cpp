#include <Wire.h>
#include <math.h>

#include <FXAS21002C.h>

FXAS21002C::FXAS21002C(byte addr)
{
	address = addr;
	gyroODR = GODR_200HZ; // In hybrid mode, accel/mag data sample rates are half of this value
	gyroFSR = GFS_250DPS;
}

void FXAS21002C::writeReg(byte reg, byte value)
{
	Wire.beginTransmission(address);
	Wire.write(reg);
	Wire.write(value);
	Wire.endTransmission();
}

// Reads a register
byte FXAS21002C::readReg(byte reg)
{
	byte value;

	Wire.beginTransmission(address);
	Wire.write(reg);
	Wire.endTransmission();
	Wire.requestFrom(address, (uint8_t)1);
	value = Wire.read();
	Wire.endTransmission();

	return value;
}

void FXAS21002C::readRegs(byte reg, uint8_t count, byte dest[])
{
	uint8_t i = 0;

	Wire.beginTransmission(address);   // Initialize the Tx buffer
	Wire.write(reg);            	   // Put slave register address in Tx buffer
	Wire.endTransmission(false);       // Send the Tx buffer, but send a restart to keep connection alive
	Wire.requestFrom(address, count);  // Read bytes from slave register address 

	while (Wire.available()) {
		dest[i++] = Wire.read();   // Put read results in the Rx buffer
	}
}

// Read the temperature data
void FXAS21002C::readTempData()
{
	tempData = readReg(FXAS21002C_H_TEMP);
}

// Put the FXAS21002C into standby mode.
// It must be in standby for modifying most registers
void FXAS21002C::standby()
{
	byte c = readReg(FXAS21002C_H_CTRL_REG1);
	writeReg(FXAS21002C_H_CTRL_REG1, c & ~(0x03));// Clear bits 0 and 1; standby mode
}

// Put the FXAS21002C into active mode.
// Needs to be in this mode to output data.
void FXAS21002C::active()
{
	byte c = readReg(FXAS21002C_H_CTRL_REG1);
	writeReg(FXAS21002C_H_CTRL_REG1, c & ~(0x03));  // Clear bits 0 and 1; standby mode
  	writeReg(FXAS21002C_H_CTRL_REG1, c |   0x02);   // Set bit 1 to 1, active mode; data acquisition enabled
}

void FXAS21002C::init()
{
	standby();  // Must be in standby to change registers

	// Set up the full scale range to 200, 400, 800, or 1600 deg/s.
	writeReg(FXAS21002C_H_CTRL_REG0, gyroFSR); 
	 // Setup the 3 data rate bits, 4:2
	if (gyroODR < 8) 
		writeReg(FXAS21002C_H_CTRL_REG1, gyroODR << 2);      
	//writeReg(FXOS8700CQ_CTRL_REG2, readReg(FXOS8700CQ_CTRL_REG2) & ~(0x03)); // clear bits 0 and 1
	//writeReg(FXOS8700CQ_CTRL_REG2, readReg(FXOS8700CQ_CTRL_REG2) |  (0x02)); // select normal(00) or high resolution (10) mode

	// Disable FIFO, route FIFO and rate threshold interrupts to INT2, enable data ready interrupt, route to INT1
  	// Active HIGH, push-pull output driver on interrupts
  	writeReg(FXAS21002C_H_CTRL_REG2, 0x0E);

  	 // Set up rate threshold detection; at max rate threshold = FSR; rate threshold = THS*FSR/128
  	writeReg(FXAS21002C_H_RT_CFG, 0x07);         // enable rate threshold detection on all axes
  	writeReg(FXAS21002C_H_RT_THS, 0x00 | 0x0D);  // unsigned 7-bit THS, set to one-tenth FSR; set clearing debounce counter
  	writeReg(FXAS21002C_H_RT_COUNT, 0x04);       // set to 4 (can set up to 255)         
	// Configure interrupts 1 and 2
	//writeReg(CTRL_REG3, readReg(CTRL_REG3) & ~(0x02)); // clear bits 0, 1 
	//writeReg(CTRL_REG3, readReg(CTRL_REG3) |  (0x02)); // select ACTIVE HIGH, push-pull interrupts    
	//writeReg(CTRL_REG4, readReg(CTRL_REG4) & ~(0x1D)); // clear bits 0, 3, and 4
	//writeReg(CTRL_REG4, readReg(CTRL_REG4) |  (0x1D)); // DRDY, Freefall/Motion, P/L and tap ints enabled  
	//writeReg(CTRL_REG5, 0x01);  // DRDY on INT1, P/L and taps on INT2

	active();  // Set to active to start reading
}

// Read the gyroscope data
void FXAS21002C::readGyroData()
{
	uint8_t rawData[6];  // x/y/z accel register data stored here

	readRegs(FXAS21002C_H_OUT_X_MSB, 6, &rawData[0]);  // Read the six raw data registers into data array
	gyroData.x = ((int16_t) rawData[0] << 8 | rawData[1]) >> 2;
	gyroData.y = ((int16_t) rawData[2] << 8 | rawData[3]) >> 2;
	gyroData.z = ((int16_t) rawData[4] << 8 | rawData[5]) >> 2;
}

// Get accelerometer resolution
float FXAS21002C::getGres(void)
{
	switch (gyroFSR)
	{
		// Possible gyro scales (and their register bit settings) are:
  // 250 DPS (11), 500 DPS (10), 1000 DPS (01), and 2000 DPS  (00). 
    case GFS_2000DPS:
          gRes = 1600.0/8192.0;
          break;
    case GFS_1000DPS:
          gRes = 800.0/8192.0;
          break;
    case GFS_500DPS:
          gRes = 400.0/8192.0;
          break;           
    case GFS_250DPS:
          gRes = 200.0/8192.0;
	}
}
/////////////////////////////////////////////////////////////////////////////////////////
// Public Methods //////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
/*
FXOS8700CQ::FXOS8700CQ(byte addr)
{
	address = addr;
	accelFSR = AFS_2g;     // Set the scale below either 2, 4 or 8
	accelODR = AODR_200HZ; // In hybrid mode, accel/mag data sample rates are half of this value
	magOSR = MOSR_5;     // Choose magnetometer oversample rate
}

// Writes a register
void FXOS8700CQ::writeReg(byte reg, byte value)
{
	Wire.beginTransmission(address);
	Wire.write(reg);
	Wire.write(value);
	Wire.endTransmission();
}

// Reads a register
byte FXOS8700CQ::readReg(byte reg)
{
	byte value;

	Wire.beginTransmission(address);
	Wire.write(reg);
	Wire.endTransmission();
	Wire.requestFrom(address, (uint8_t)1);
	value = Wire.read();
	Wire.endTransmission();

	return value;
}

void FXOS8700CQ::readRegs(byte reg, uint8_t count, byte dest[])
{
	uint8_t i = 0;

	Wire.beginTransmission(address);   // Initialize the Tx buffer
	Wire.write(reg);            	   // Put slave register address in Tx buffer
	Wire.endTransmission(false);       // Send the Tx buffer, but send a restart to keep connection alive
	Wire.requestFrom(address, count);  // Read bytes from slave register address 

	while (Wire.available()) {
		dest[i++] = Wire.read();   // Put read results in the Rx buffer
	}
}

// Read the accelerometer data
void FXOS8700CQ::readAccelData()
{
	uint8_t rawData[6];  // x/y/z accel register data stored here

	readRegs(FXOS8700CQ_OUT_X_MSB, 6, &rawData[0]);  // Read the six raw data registers into data array
	accelData.x = ((int16_t) rawData[0] << 8 | rawData[1]) >> 2;
	accelData.y = ((int16_t) rawData[2] << 8 | rawData[3]) >> 2;
	accelData.z = ((int16_t) rawData[4] << 8 | rawData[5]) >> 2;
}

// Read the magnometer data
void FXOS8700CQ::readMagData()
{
	uint8_t rawData[6];  // x/y/z accel register data stored here

	readRegs(FXOS8700CQ_M_OUT_X_MSB, 6, &rawData[0]);  // Read the six raw data registers into data array
	magData.x = ((int16_t) rawData[0] << 8 | rawData[1]) >> 2;
	magData.y = ((int16_t) rawData[2] << 8 | rawData[3]) >> 2;
	magData.z = ((int16_t) rawData[4] << 8 | rawData[5]) >> 2;
}

// Read the temperature data
void FXOS8700CQ::readTempData()
{
	tempData = readReg(FXOS8700CQ_TEMP);
}

// Put the FXOS8700CQ into standby mode.
// It must be in standby for modifying most registers
void FXOS8700CQ::standby()
{
	byte c = readReg(FXOS8700CQ_CTRL_REG1);
	writeReg(FXOS8700CQ_CTRL_REG1, c & ~(0x01));
}

// Put the FXOS8700CQ into active mode.
// Needs to be in this mode to output data.
void FXOS8700CQ::active()
{
	byte c = readReg(FXOS8700CQ_CTRL_REG1);
	writeReg(FXOS8700CQ_CTRL_REG1, c | 0x01);
}

void FXOS8700CQ::init()
{
	standby();  // Must be in standby to change registers

	// Configure the accelerometer
	writeReg(FXOS8700CQ_XYZ_DATA_CFG, accelFSR);  // Choose the full scale range to 2, 4, or 8 g.
	//writeReg(FXOS8700CQ_CTRL_REG1, readReg(FXOS8700CQ_CTRL_REG1) & ~(0x38)); // Clear the 3 data rate bits 5:3
	if (accelODR <= 7) 
		writeReg(FXOS8700CQ_CTRL_REG1, readReg(FXOS8700CQ_CTRL_REG1) | (accelODR << 3));      
	//writeReg(FXOS8700CQ_CTRL_REG2, readReg(FXOS8700CQ_CTRL_REG2) & ~(0x03)); // clear bits 0 and 1
	//writeReg(FXOS8700CQ_CTRL_REG2, readReg(FXOS8700CQ_CTRL_REG2) |  (0x02)); // select normal(00) or high resolution (10) mode

	// Configure the magnetometer
	writeReg(FXOS8700CQ_M_CTRL_REG1, 0x80 | magOSR << 2 | 0x03); // Set auto-calibration, set oversampling, enable hybrid mode 
		                                     
	// Configure interrupts 1 and 2
	//writeReg(CTRL_REG3, readReg(CTRL_REG3) & ~(0x02)); // clear bits 0, 1 
	//writeReg(CTRL_REG3, readReg(CTRL_REG3) |  (0x02)); // select ACTIVE HIGH, push-pull interrupts    
	//writeReg(CTRL_REG4, readReg(CTRL_REG4) & ~(0x1D)); // clear bits 0, 3, and 4
	//writeReg(CTRL_REG4, readReg(CTRL_REG4) |  (0x1D)); // DRDY, Freefall/Motion, P/L and tap ints enabled  
	//writeReg(CTRL_REG5, 0x01);  // DRDY on INT1, P/L and taps on INT2

	active();  // Set to active to start reading
}

// Get accelerometer resolution
float FXOS8700CQ::getAres(void)
{
	switch (accelFSR)
	{
		// Possible accelerometer scales (and their register bit settings) are:
		// 2 gs (00), 4 gs (01), 8 gs (10). 
		case AFS_2g:
			return 2.0/8192.0;
		break;
		case AFS_4g:
			return 4.0/8192.0;
		break;
		case AFS_8g:
			return 8.0/8192.0;
		break;
	}

	return 0.0;
}

// Get magnometer resolution
float FXOS8700CQ::getMres(void)
{
	return 10./32768.;
}
*/
// Private Methods //////////////////////////////////////////////////////////////