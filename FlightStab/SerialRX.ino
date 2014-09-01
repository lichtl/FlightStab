/* FlightStab **************************************************************************************************/

/***************************************************************************************************************
*
* SERIAL RX
* see http://paparazzi.github.io/docs/latest/stm32_2subsystems_2radio__control_2spektrum__arch_8c_source.html
*
***************************************************************************************************************/

#if defined(SERIALRX_SPEKTRUM)
#define SERIAL_FRAME_SIZE	16
//jrb - added 20140831 Add define for Quiet Time
#define SERIAL_WAIT_TIME	7000
int8_t rshift;
#endif

#if defined(SERIALRX_SBUS)
// The following value is added to the received pulse count
// to make the center pulse width = 1500 when the TX output is 1500
// TODO(noobee): i guess we would need to make this configurable..
#define SBUS_OFFSET 1009 // 1009 for Futaba X8R, 1003 for Taranis FRSKY X8R, 984 for Orange R800x

//jrb - added 20140829 Use defines for SBUS Constants instead of hard coded numbers
// SBUS unique defines
#define SBUS_SYNC_BYTE 		0x0F
#define SBUS_END_BYTE		0x00
#define SERIAL_FRAME_SIZE	25
#define SERIAL_WAIT_TIME	5000
#endif
      
#if (defined(SERIALRX_SPEKTRUM) || defined(SERIALRX_SBUS))
  #if defined(NANOWII)
    HardwareSerial *pSerial = &Serial1; // TODO: hardcoded for NanoWii for now
  #else
    HardwareSerial *pSerial = &Serial; 
  #endif // NANOWI
  
  //jrb add for debug  
  #if defined(SERIALRX_DEBUG) && 1
    volatile int8_t RXcount;
    int8_t PrintIndex;
    uint16_t work;
  #endif  


  void serialrx_init()
  {
    #if defined (SERIALRX_SPEKTRUM)
      pSerial->begin(115200L);
      rshift = cfg.serialrx_spektrum_levels == SERIALRX_SPEKTRUM_LEVELS_1024 ? 10 : 11; // 1024->10, 2048->11
    #endif
    #if defined (SERIALRX_SBUS)
      pSerial->begin(100000L, SERIAL_8E2);
      #if defined (FLIP_SERIALRX_DEBUG)
        pinMode(4,OUTPUT);
        pinMode(5,OUTPUT);
        pinMode(6,OUTPUT);
        digitalWrite(4,HIGH);		// D4 provides negative pulse that windows the serial receive frame
        digitalWrite(5,HIGH);		// D5 provides negative pulse when last byte of SBUS frame != 0x00
        digitalWrite(5,HIGH);		// D6 provides negative pulse when serialrx_update function is active
      #endif
    #endif
  }

  bool serialrx_update()
  {
    static int8_t index = 0;
    #if defined (SERIALRX_SBUS)
      bool sbus_return = false;
    #endif // SERIALRX_SBUS  
    
  //jrb 20140831 use for both Spektrum & SBUS #if defined (SERIALRX_SPEKTRUM) 	// Used only for Spektrum    
    static uint32_t last_rx_time;
    uint32_t t;
    static uint8_t buf[SERIAL_FRAME_SIZE];
  //jrb 20140831 use for both Spektrum & SBUS #endif  

  //jrb add for debug
  #if defined(SERIALRX_DEBUG) && 0
    Serial.println("Serial update");
  #endif
  
  #if defined (FLIP_SERIALRX_DEBUG)
    digitalWrite(5,HIGH);
    digitalWrite(6,LOW);
  #endif 
  
  //jrb - added 20140829 Limit "while loop" to not exceed buffer size   
    while (pSerial->available() && index<SERIAL_FRAME_SIZE) {
    uint8_t ch = pSerial->read();

//jrb - 20140832 make frame time test common to both Spektrum and SBUS
    t = micros1();
    // we assume loop() calls to serialrx_update() in << SERIAL_WAIT_TIME intervals
    if ((int32_t)(t - last_rx_time) > SERIAL_WAIT_TIME) {
      index = 0; // found pause before new frame, resync
    #if defined (FLIP_SERIALRX_DEBUG)
      digitalWrite(4,HIGH);
    #endif      
    }
    last_rx_time = t;
      
  #if defined (SERIALRX_SPEKTRUM)    
    #warning SERIALRX_SPEKTRUM defined // emit device name 
  //jrb add for debug    
      #if defined(SERIALRX_DEBUG) && 0
        Serial.println("Serial Spektrum");
      #endif

      buf[index++] = ch;
      if (index >= SERIAL_FRAME_SIZE) {

/*jrb        
//  Satellites alone never return frame size information in buf[1] the best I can tell
//  Data always seems to be 2048 bits, even with 9XR transmitter.  The following
//  should be removed and 10 or 11 bit format saved as configuration data
//          if ((buf[2] & 0x80) == 0x00) { 
//          // single frame type or 1st frame of two, contains "transmitter type"
//            rshift = (buf[1] & 0x10) ? 11 : 10; // 11 or 10 bit data
//          }
jrb*/ 
 
        if (rshift > 0) {
          // 10 bit == f  0 c3 c2 c1  c0 d9 d8 d7 d6 d5 d4 d3 d2 d1 d0
          // 11 bit == f c3 c2 c1 c0 d10 d9 d8 d7 d6 d5 d4 d3 d2 d1 d0        
          for (int8_t i=2; i<2+2*7; i+=2) {
            uint16_t w = ((uint16_t)buf[i] << 8) | (uint16_t)buf[i+1];
            int8_t chan = (w >> rshift) & 0xf;
            if (chan < rx_chan_size) {
              *rx_chan[chan] = ((w << (11 - rshift) & 0x7ff) - 1024 + 1500); // scale to 11 bits 1024 +/- 684;
            }          
          }
        }  
        index = 0;

  //jrb add for debug       
  #if defined(SERIALRX_DEBUG) && 0   
      if (RXcount > 100)
      {
        Serial.print("rshift = ");
        Serial.print(rshift);
        if ((buf[2] & 0x80) == 0x00) { 
          Serial.println(" Low Channels");
        }
        else  {
          Serial.println(" High Channels");  
        }
        
        for (PrintIndex = 0; PrintIndex < 8; PrintIndex++)
        {
          Serial.print(*rx_chan[cfg.serialrx_order-2][PrintIndex]); Serial.print(' ');
        }
        Serial.println(' '); 
              
        for (PrintIndex = 0; PrintIndex < 16; PrintIndex += 2)
        { 
          work = (buf[PrintIndex]<<8)+ buf[PrintIndex + 1];
          Serial.print(work,HEX); Serial.print(' ');
        }
        Serial.println(' '); 
       RXcount = 0; 
      }
      RXcount++; 
  #endif
      return (true);
    }
    else
      return (false);
  }  
  #endif // SERIALRX_SPEKTRUM



  #if defined (SERIALRX_SBUS)
    #warning SERIALRX_SBUS defined // emit device name 
    //jrb add for debug    
      #if defined(SERIALRX_DEBUG) && 0
        Serial.println("Serial S.BUS");
      #endif
      
      if (index == 0 && ch != SBUS_SYNC_BYTE) { 
        break;
      }
      #if defined (FLIP_SERIALRX_DEBUG)
        if (index == 0)
        {	
          digitalWrite(4,LOW);
        }  
      #endif
      buf[index++] = ch;
      if (index >= SERIAL_FRAME_SIZE) {
        volatile int16_t **p = rx_chan;
        uint8_t adj_index;
        //jrb - added 20140829 to detect and discard invalid frames
        if (buf[24] != SBUS_END_BYTE) { // There is a sync problem - discard this frame and try again
          index = 0;
          sbus_return = false;
      #if defined (FLIP_SERIALRX_DEBUG)
        digitalWrite(5,LOW);
      #endif          
  	} 	
         	
        // Only process first 8 channels
        *p[0] = (((((uint16_t)buf[1]  >> 0) | ((uint16_t)buf[2]  << 8)) & 0x7ff) >> 1) + SBUS_OFFSET;
        *p[1] = (((((uint16_t)buf[2]  >> 3) | ((uint16_t)buf[3]  << 5)) & 0x7ff) >> 1) + SBUS_OFFSET; 
        *p[2] = (((((uint16_t)buf[3]  >> 6) | ((uint16_t)buf[4]  << 2) | ((uint16_t)buf[5] << 10)) & 0x7ff) >> 1) + SBUS_OFFSET; 
        *p[3] = (((((uint16_t)buf[5]  >> 1) | ((uint16_t)buf[6]  << 7)) & 0x7ff) >> 1) + SBUS_OFFSET; 
        *p[4] = (((((uint16_t)buf[6]  >> 4) | ((uint16_t)buf[7]  << 4)) & 0x7ff) >> 1) + SBUS_OFFSET; 
        *p[5] = (((((uint16_t)buf[7]  >> 7) | ((uint16_t)buf[8]  << 1) | ((uint16_t)buf[9] << 9)) & 0x7ff) >> 1) + SBUS_OFFSET;
        *p[6] = (((((uint16_t)buf[9]  >> 2) | ((uint16_t)buf[10] << 6)) & 0x7ff) >> 1) + SBUS_OFFSET; 
        *p[7] = (((((uint16_t)buf[10] >> 5) | ((uint16_t)buf[11] << 3)) & 0x7ff) >> 1) + SBUS_OFFSET;
       
       // For some reason the SBUS data provides only about 75% of the actual RX output pulse width
       // Adjust the actual value by +/-25%.  Sign determined by pulse width above or below center of 1520us 
       for(adj_index=0; adj_index<rx_chan_size; adj_index++)
       {
       	if (*p[adj_index] < 1520)
       	  *p[adj_index] -= (1520 - *p[adj_index]) >> 2;		
       	else	
       	  *p[adj_index] += (*p[adj_index] - 1520) >> 2;
       }	 
        index = 0;
        sbus_return = true;
      #if defined (FLIP_SERIALRX_DEBUG)
        digitalWrite(4,HIGH);
      #endif
      }

  //jrb add for debug  
    #if defined(SERIALRX_DEBUG) && 1   
      if (RXcount > 100)
      {
        for (index = 0; index < 8; index++)
        {
          Serial.print(*rx_chan[cfg.serialrx_order-2][index]); Serial.print(' ');
        }
        Serial.println(' '); 
       RXcount = 0; 
      }
      RXcount++; 
    #endif
  }
#if defined (FLIP_SERIALRX_DEBUG)
    digitalWrite(6,HIGH);
  #endif 
    
  return (sbus_return); 
  #endif // SERIALRX_SBUS
}
#endif // SERIALRX_SPEKTRUM) || SERIALRX_SBUS 

