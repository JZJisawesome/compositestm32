//Uses SPI, DMA, and interrupts to display composite video. Almost 0 cpu usage needed
//Max effective image size of H,V: 720,482
//Storage and display Size H,V: 928, 482. The horizontal is compressed because of spi
//SPI prescaler granularity is low, so in order to get a proper image, stretch horizontal pixels
//from 720 to 928, so that they are compressed back to 720 when displayed
//BE SURE TO BUILD WITH -O3
//PA7 is video
//PB8 is sync

/* Schematic (ALL OUTPUTS ARE PUSH-PULL)
 * Video pin -> anode-diode-cathode -> 195 ohm resistor -> Output
 * Sync pin -> anode-diode-cathode -> 650 ohm resistor -> Output
 * Inside monitor: Output -> 75 ohm resistor -> ground
 */

/* Voltage Levels:
 * 
 * Video low, Sync low: 0v: hsync/vsync
 * Video low, Sync high: 0.3v: Front/back borch and black pixel
 * Video high, sync low: 1v: White pixel
*/

/* Resistance Calculations
 * Remember 75ohm resistance to ground inside of display from input
 * 
 * Scenario: Syncing (want 0v)
 *      Video and sync low, diodes off, output pulled down by 75 ohm input impedance. Get 0v
 * 
 * Scenario: Drawing white pixel (want 1v)
 *      Remember 75ohm input impedance. Set video high, and sync low. Sync diode becomes high Z.
 *      Video voltage before diode (3.3v) lowered by 0.7v, now 2.6v
 *      Ratio: (1v/2.6v) = (75/R_video)
 *      Rearrange: R_video = 75(2.6) = 195 ohms
 * 
 * Scenario: Front/back porch and black pixel (want 0.3v)
 *      Remember 75ohm input impedance. Set video low, and sync high. Video diode is high Z.
 *      Sync voltage before diode (3.3v) lowered by 0.7v, now 2.6v
 *      Ratio: (0.3v/2.6v) = (75/R_sync)
 *      Rearrange and solve: R_sync = 650 ohms
*/

#include "bluepill.h"
//#include "newimages.h"
#include "about.h"

//Settings ( (with interlacing and SPI_PRESCALER_VALUE 16))
#define BYTES_PER_LINE 116//Can be max of 58 (464 pixels horizontal) with SPI_PRESCALER_VALUE=4
#define INTERLACING//Double vertical pixels from 241 to 482
#define SPI_PRESCALER_VALUE 4//4 is the max
//const uint8_t const* framebufferPointer = newImage3;
const uint8_t const* framebufferPointer = about;

/* Code stuffs */

volatile uint_fast16_t currentLine = 0;//0 to 261 inclusive
volatile bool evenFrame = true;//Start on the first frame; Useful for managing interlacing

//Screen Management Functions/Macros
void configureVideo();//Sets up DMA and SPI
void configureHorizontalLineTimer();
//FIXME for disable and enable video, only affect PA7, not other pins
#define disableVideo() {GPIOA_CRL = 0x30000000;}//Pull PA7 low by setting as regular output
#define enableVideo() {GPIOA_CRL = 0xB0000000;}//Give control back to spi peripheral
#define syncEnable() {GPIOB_BRR = 1 << 8;}//Pull PB8 low
#define syncDisable() {GPIOB_BSRR = 1 << 8;}//Pull PB8 high

//Obselete functions
void configureSystick();
void delayMicroseconds(uint32_t time);

void main()
{
    //GPIOA_CRL = 0x30000000;//PA7 as 50mhz output (push-pull)
    GPIOA_CRL = 0xB0000000;//PA7 as 50mhz output (alternate function push-pull)
    GPIOB_CRH = 0x00000003;//PB8 as 50mhz output (push-pull)
    GPIOA_BRR = 1 << 7;//Make PA7 low by default if not set as alternate function
    
    configureVideo();
    configureHorizontalLineTimer();
}

/* Screen Management */

void configureVideo()//Sets up DMA and SPI
{
    //Setup SPI
    //testing
    //SPI1_CR1 = 0b0000001101101100;//Enable software input of nss pin held high to keep in master mode, msbfirst, enable spi, Baud rate = fPCLK/64, Master, CPOL=0, CPHA=0
    //SPI1_CR1 = 0b0000001101011100;//Enable software input of nss pin held high to keep in master mode, msbfirst, enable spi, Baud rate = fPCLK/16, Master, CPOL=0, CPHA=0
    //SPI1_CR1 = 0b0000001101010100;//Enable software input of nss pin held high to keep in master mode, msbfirst, enable spi, Baud rate = fPCLK/8, Master, CPOL=0, CPHA=0
    //SPI1_CR1 = 0b0000001101001100;//Enable software input of nss pin held high to keep in master mode, msbfirst, enable spi, Baud rate = fPCLK/4, Master, CPOL=0, CPHA=0
    //SPI1_CR1 = 0b0000001101000100;//Enable software input of nss pin held high to keep in master mode, msbfirst, enable spi, Baud rate = fPCLK/2, Master, CPOL=0, CPHA=0
    
    SPI1_CR2 = 0x0002;//Enable dma request when transmit buffer is empty
    
    //Setup DMA (Channel 3)
    DMA_CCR3 = 0x3090;//Max priority, 8 bit accesses, memory increment, memory to peripheral
    DMA_CNDTR3 = BYTES_PER_LINE;
    DMA_CPAR3 = 0x4001300C;//Writing to SPI1_DR
    DMA_CMAR3 = framebufferPointer;//Start at framebufferPointer
}

//Horizontal Line Timer (15734hz)
//Handles vertical and horizontal syncing, and updates currentLine

void configureHorizontalLineTimer()
{
    //Useful: https://vivonomicon.com/2018/05/20/bare-metal-stm32-programming-part-5-timer-peripherals-and-the-system-clock/
    TIM4_PSC = 0;//72Mhz
    TIM4_ARR = 4575;//Use reload register to reset counter at rate of 15734hz
    
    TIM4_CCMR1 = 0x1818;//Set compare channel 1 and 2 to enable flag on match
    TIM4_CCMR2 = 0x0018;//Set compare channel 3 to enable flag on match
    TIM4_CCR1 = 98;//Set oc1ref after 1.5us
    TIM4_CCR2 = 409;//Set oc2ref high 4.7us after that (6.2us after timer reset)
    TIM4_CCR3 = 760;//Set oc2ref high 4.7us after that (10.9us after timer reset)
    //Note: raw math says 107, 445, 784. Extra tuning done after to perfect timings on my
    //particular board
    
    TIM4_EGR |= 0x0001;//Generate update even to initialize things
    
    TIM4_DIER = 0x000F;//Enable channel 1, 2, 3, and counter reset (uif) interrupts
    //TIM4_DIER = 0x080F;//Enable channel 1, 2, 3, and counter reset (uif) interrupts; dma ch3
    NVIC_ISER0 = 1 << 30;//Enable timer 4 interrupt in the nvic
    TIM4_CR1 |= 0x0001;//Enable counter (upcounting)
}

__attribute__ ((interrupt ("IRQ"))) void __ISR_TIM4()//15734hz
{
    if (TIM4_SR & (1 << 0))//Check if it was a uif interrupt
    {
        TIM4_SR &= ~(1 << 0);//Clear flag
        //Disable sync now, if the last line was an inverted vblank line. Really this should be
        //done earlier to get a 4.7 rather than a 1.5 us pulse, but this is simpler and still
        //works
        syncDisable();
        
        disableVideo();//Stop drawing white if it was left on
        DMA_CCR3 &= 0xFFFE;//Disable DMA ch3 to change settings and save bandwidth during hblank
        
        if (currentLine > 19)//After hblank, the line will be an active line (offset by 1)
        {
            DMA_CNDTR3 = BYTES_PER_LINE;//Reset transfer counter
            
            uint32_t activeLine = currentLine - 20;//activeLine 0 is the first line and so on
            uint32_t offset, lineAddress;
            
            #ifdef INTERLACING
                offset = activeLine * (BYTES_PER_LINE * 2);//Jump by 2 lines
                lineAddress = framebufferPointer + offset;
                
                if (evenFrame)
                    lineAddress += BYTES_PER_LINE;//Line in between even lines
            #else
                offset = activeLine * BYTES_PER_LINE;
                lineAddress = framebufferPointer + offset;
            #endif
            
            DMA_CMAR3 = lineAddress;
        }
    }
    else if (TIM4_SR & (1 << 1))//Check if it was a channel 1 interrupt
    {
        TIM4_SR &= ~(1 << 1);//Clear flag
        syncEnable();//Front porch finished, time for sync
    }
    else if (TIM4_SR & (1 << 2))//Check if it was a channel 2 interrupt
    {
        TIM4_SR &= ~(1 << 2);//Clear flag
        //TODO figure out why these have these are 1 less than they should be
        //(currentLine < 6) | (currentLine > 11) makes more sense, but causes
        //7 reg, 6 inv, 5 reg instead of 6 reg, 6 inv, 6 reg
        if ((currentLine < 5) | (currentLine > 10))//Non-inverting sync region
            syncDisable();
    }
    else if (TIM4_SR & (1 << 3))//Channel 3 interrupt: Update line counter
    {
        TIM4_SR &= ~(1 << 3);//Clear flag
        
        ++currentLine;//Increment the current line count
        
        //Even frames, whether we are interlacing or not, are 262 scanlines; Odd frames are 263
        //Technically this is backwards but it dosen't really matter
        if (((currentLine == 262) & (evenFrame)) | (currentLine == 263))
        {
            currentLine = 0;//Start new frame
            evenFrame = !evenFrame;//Used for managing interlacing and next frame detection
        }
        else if (currentLine > 20)//Active area (after 21 lines of vsync)
        {
            enableVideo();
            DMA_CCR3 |= 0x0001;//Enable DMA channel 3 for the current line
        }
    }
}
