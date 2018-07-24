/*PWM Test code
 * Gareth Davies, 23 July 2018.
 * 
 * /////////////////////////////////
 * This is the control code for an ATTINY85 based PWM 
 * controller for charging Lead Acid Cells.
 * 
 * ////////////////////////////////////////
 * How it works: 
 *  This sets up two PWM outputs:
 *  one at 20Khz to drive a charge pump to fire high-side
 *  N-CHANEL MOSFET in the controller circuit;
 *  second at 4Khz for the main PWM charging waveform.
 * 
 * TO BE IMPLEMENTED:
 *  Control of the PWM cycle is based on two measurements
 *  The Current flowing to the battery being charged
 *  The Voltage of the Lead Acid Cell (to avoid overcharging)
 * 
 * 
 * Refer to the ATTINY Datasheet which can be found here: 
 * http://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-2586-AVR-8-bit-Microcontroller-ATtiny25-ATtiny45-ATtiny85_Datasheet.pdf
 * 
 */


#define WAITBEFOREREAD 240
#define MAXPWM 240
#define PWMSTEP 1
#define BIGSTEP 50

void setup() {
 startDrivePWM();
 startChargePWM();
}

void loop() {


   for (int a =0; a<256; a++)
   {
      setPWMduty((byte)a);
      delay(100);
   }

}


/*
 * This is the setPWMduty cycle. In many ways it's the same as analogueWrite
 * but it's faster when used in this context. It doesn't do all the checks
 * that you'd want in a general setting for GPIO config etc. But because I know
 * what's going on in the code it's safe to use here and is faster as a result.
 * I may incorporate this directly into the main loop to save a stack call on the
 * way in.
 */

void setPWMduty (byte duty)
{
  OCR0A = duty;  
}


/*
 * startChargePWM sets up the registers so that asynchronous PWM is available
 * it then sets the frequency to 20Khz. This is for the charge pump PWM signal
 * it is designed to be faster than the control charge PWM so that there is charge
 * being built up in the firing circuit for the control MOSFET during an on-cycle
 * this helps to maintain the saturation mode for the FET without risk of the charge pump
 * draining from some gate-source leakage current.
 */

void startChargePWM (void)

{
    
    byte Mask; //Used to construct bit masks for the control registers.

    /*
     * This is for using OCRB on PB4, this caused problems that the
     * test rig couldn't uncover in time, not needed so commented out
     * 
    //CONFIGURE PORT B  Pin 4 as OUTPUT. This is Pin 3 on the ATTiny
    //DB4 when enabled for PWM is OC1B
    //Controlled by TCCR1 COM1B0 and COM1B1
    DDRB = DDRB | 1 << DDB4;  
    */
    
    //CONFIGURE PORT B  Pin 1 as OUTPUT. This is Pin 6 on the ATTiny
    //DB1 When configured for PWM is OC1A
    //Controlled by TCCR1 COM1A0 and COM1A1
    DDRB = DDRB | 1 << DDB1;
    
    //Enable PLL, this is needed in order to enable the asynchronous mode
    PLLCSR = PLLCSR  | (1 << PLLE);  //Sets the PLLE Bit to 1

    //Poll the PLOCK Bit until it is set, this is required, apparently
    //Not sure if it's strictly necessary but we will
    //Worrying that the code may block here and We'd never know!
    Mask=0;
    Mask = Mask | 1<<PLOCK;
    while ((PLLCSR & Mask) == 0)    
    {
      delay(10);
    }
    
    //Set PLL for Timer 1 to asynchronous mode
    //This means it uses the fast peripheral clock instead of the system clock
    PLLCSR = PLLCSR | (1 << PCKE); // Set the PCKE bit to 1

    //Set up TCCR1
    //PWM1A this is bit 6 on the ATTINY - set this to enable PWM on using OCR1A
    //COM1[A,B][0,1] set up the output pins for the GPIO
    // 0,0 Not connected
    // 0,1 Both OC1x and inverted mode coneected.
    // 1,0 Only OC1x connected
    // 1,1 Only OC1x(Bar) inverted mode connected
    // Here use 0,1 which connects to output PIN 3 on the ATTINY85 8-DIL Package
    //
    // The prescaler for Timer 1 when in Asynchronous mode is set by bits CS1[3:0]
    // Check out the data sheet because it's a comples interaction between this
    // And the value held in OCR1C to determine the frequency
    Mask = 255 & (
           0 << PWM1A &
           0 << COM1A1 &
           0 << COM1A0 &
           
 // This is if you want it on PB4, but I've had problems with that
 //          0 << COM1B1 &
 //          0 << COM1B0 &
           0 << CS13 &
           0 << CS12 &
           0 << CS11 &
           0 << CS10
           );
    TCCR1 = (TCCR1 & Mask) |
                    (
                      1 << PWM1A |
                      1 << COM1A1 |
                      0 << COM1A0 |
                      
// For some reason PB4 doesn't fire on my test rig
//                      1 << COM1B1 |
//                      0 << COM1B0 |
                      0 << CS13 |
                      1 << CS12 |
                      0 << CS11 |
                      1 << CS10
                      );
    
    //Set Frequency of PWM using OCR1C
    // The combinateion of CS13-10 [0101] and 199 in 0CR1C is 20Khz
    // The PWM fequency can be set from 20 - 500Khz in 10Khz steps
    // Refer to the Data sheet to work it out, it's not obvious
    // But ATMEL provides a look up table of value combinations
    OCR1C = 199;

    //Set up the compare register for the Fast PWM output compare
    OCR1A = 0b10000000; //50% Duty Cycle

    //This is commented out as it refers to the PB4 option which is not
    //Used (nor, for that matter, working)
    //
    //    OCR1B = 0b10000000; //50% Duty Cycle
    
}

/*
 * This is the main function for starting up the PWM for the charge controller
 * this is working of counter 0 and sets it's prescaler to the minimum meaining
 * that it runs at the chip clock divided by 256. Running at 1MHz this means a 
 * frequency of around 4Khz.
 * 
 * The duty cycle of the PWM can be manipulated by setting the compare register
 * OCR0A. 
 * 
 */

void startDrivePWM (void)
{
    byte Mask;  //Used to set up mask bits when setting register values
    
   //CONFIGURE PORT B  Pin 0 as OUTPUT. This is Pin 3 on the ATTiny
   DDRB = DDRB | 1 << DDB0; //Set 1=Output on  PortB Pin 0, ATTINY85 DIL Package PIN 5;

   
////////////////////////////////////////
//Waveform generation is controlled by WGM0[2:0] in TCCR0A and TCCR0B.
//Fast PWM is 011 (WGM2 is in TCCR0A, the rest in TCCR0B)
//
//CS0[2:0] in TCCR0B controls the clock prescaler
//This is the internal frequency (1Mhz) divided by 256 divided by Prescaler value
// 001 None
// 010 8
// 011 64
// 100 256
// 101 1024
// 110 and 111 are for external clock operations.
///////////////////////////////////////////////// 

    Mask = 255 & (
                    0 << WGM02 & 
                    0 << CS02 & 
                    0 << CS01 & 
                    0 << CS00
                   );
                   
    TCCR0B = (TCCR0B & Mask) |
                      ( 
                        0 <<WGM02 | 
                        0<<CS02 | 
                        0<<CS01 | 
                        1<<CS00
                      );

////////////////////////////////////////////////////
// Set up WGM0[1,2] to select fast PWM as described above
// COM0[A,B][0,1] are used to overide the GPIO pin assignment
// To connect to the and connect the PWM to PIN 5
// COM0A refers to OCR0A output
// 0,0 disconnected
// 0,1 Clear on Compare Match, set at Bottom (non inverting mode)
// 1,1 Set on Compare Match, clear at Bottom (inverting mode)
/////////////////////////////////////////////////////
    Mask = 255 & ( 
                    0 << WGM01 & 
                    0 << WGM00 & 
                    0 << COM0A1 & 
                    0 << COM0A1 &
                    0 << COM0B0 & 
                    0 << COM0B1
                    );
    TCCR0A = (TCCR0A & Mask) | 
                      (
                         1<<WGM01 | 
                         1<<WGM00 | 
                         1<<COM0A1 | 
                         0<<COM0A0 | 
                         0<<COM0B1 | 
                         0 <<COM0B0
                      );    
}


