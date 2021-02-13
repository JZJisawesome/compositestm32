//This version uses timer interrupts more effectively for more accurate sync line management
//IN FUTURE:use SPI and dma to make pixel output easier
//          Can start dma transfer after channel 3 compare match
//          Can use dma transfer finish interrupt to reload memory start address with next line
//          After transfer: DMA channel ? mem address = activeLine * something;
//BE SURE TO BUILD WITH -O3
//PB5 is video
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

//All in number of instructions passed to __delayInstructions (21.1851ns per instruction avg)
#define ACTIVE_AMOUNT 2486//52.65662896us
#define LINE_AMOUNT 3000//63.55662896us

#include "bluepill.h"

volatile uint_fast16_t currentLine = 0;//0 to 261 inclusive
volatile bool currentLineUpdated = true;//Current line just updated by the ISR//Starts as true to handle line 0
volatile bool evenFrame = true;//Start on the first frame; Useful for managing interlacing

//Customizable functions
void renderLine(uint_fast8_t activeLine);//Called after hsync in active region
void processing();//Called after line is updated; in active or vblank region

//Screen Management Functions/Macros
void configureHorizontalLineTimer();
void displayLoop();
#define displayWhite() {GPIOB_BSRR = 1 << 5;}//Pull PB5 high
#define displayBlack() {GPIOB_BRR = 1 << 5;}//Pull PB5 low
#define syncEnable() {GPIOB_BRR = 1 << 8;}//Pull PB8 low
#define syncDisable() {GPIOB_BSRR = 1 << 8;}//Pull PB8 high

//Obselete functions
void configureSystick();
void delayMicroseconds(uint32_t time);

void main()
{
    GPIOB_CRL = 0x00300000;//PB5 as 50mhz output (push-pull)
    GPIOB_CRH = 0x00000003;//PB8 as 50mhz output (push-pull)
    
    configureHorizontalLineTimer();
    
    displayBlack();//testing
    
    displayLoop();//Never exits
}

/* Customization Functions */
//On an active line, the time to execute renderLine must be <= 52.65662896us
//processing may take as long as it wants, provided that it does not starve renderLine
//on the following line and push it into the hblank region

#include "images.h"

void renderLine(uint_fast8_t activeLine)//Only called on active lines
{
    //testing
    //__delayInstructions(500);
    //displayWhite();
    //displayBlack();
    
    //FB test
    /*
    const uint8_t const* image = demoImage6;
    
    //Offset needed because it is a 1d array
    uint32_t offset = activeLine * 40;//1 line = 1 vertical pixel; 40 bytes per line
    for (uint32_t i = 0; i < 40; ++i)//Make 320 by 256
    {
        for (uint32_t j = 0; j < 8; ++j)
        {
            if (image[offset + i] & (0x80 >> j))//Pixel is on
            {
                displayWhite();
            }
            else//Pixel is off
            {
                displayBlack();
            }
        }
    }
    displayBlack();//Cut off image early to avoid streaks at the right side
    */
    
    //FB interlaced test
    const uint8_t const* image = demoImage7;
    
    //Offset needed because it is a 1d array
    
    //1 line = 1 vertical pixel; 40 bytes per line, interlaced 
    uint32_t offset = activeLine * 80;
    
    if (!evenFrame)
        offset += 40;
    
    for (uint32_t i = 0; i < 40; ++i)//Make 320 by 512
    {
        for (uint32_t j = 0; j < 8; ++j)
        {
            if (image[offset + i] & (0x80 >> j))//Pixel is on
            {
                displayWhite();
            }
            else//Pixel is off
            {
                displayBlack();
            }
        }
    }
    displayBlack();//Cut off image early to avoid streaks at the right side
}

void processing()//Called on all lines; after renderLine on active lines
{
    //Not used
}

/* Screen Management */

void displayLoop()
{
    while (true)
    {
        if (currentLineUpdated)//Time to write to display
        {
            currentLineUpdated = false;//We're handling it now
            
            if (currentLine > 17)//Inside of the active region
                renderLine((uint_fast8_t)(currentLine - 18));//18 lines of vblank
        }
        
        processing();
    }
}

//Horizontal Line Timer (15734hz)
//Handles vertical and horizontal syncing, and updates currentLine and currentLineUpdated

void configureHorizontalLineTimer()
{
    //Useful: https://vivonomicon.com/2018/05/20/bare-metal-stm32-programming-part-5-timer-peripherals-and-the-system-clock/
    TIM4_PSC = 0;//72Mhz
    TIM4_ARR = 4575;//Use reload register to reset counter at rate of 15734hz
    
    TIM4_CCMR1 = 0x1818;//Set compare channel 1 and 2 to enable flag on match
    TIM4_CCMR2 = 0x0018;//Set compare channel 3 to enable flag on match
    TIM4_CCR1 = 98;//Set oc1ref after 1.5us
    TIM4_CCR2 = 409;//Set oc2ref high 4.7us after that (6.2us after timer reset)
    TIM4_CCR3 = 698;//Set oc2ref high 4.7us after that (10.9us after timer reset)
    //Note: raw math says 107, 445, 784. Extra tuning done after to perfect timings on my
    //particular board
    
    TIM4_EGR |= 0x0001;//Generate update even to initialize things
    
    TIM4_DIER = 0x000F;//Enable channel 1, 2, 3, and counter reset (uif) interrupts
    NVIC_ISER0 = 1 << 30;//Enable timer 4 interrupt in the nvic
    TIM4_CR1 |= 0x0001;//Enable counter (upcounting)
}

__attribute__ ((interrupt ("IRQ"))) void __ISR_TIM4()//15734hz
{
    if (TIM4_SR & (1 << 0))//Check if it was a uif interrupt
    {
        TIM4_SR &= ~(1 << 0);//Clear flag
        displayBlack();//Stop drawing white if it was left on
        //Disable sync now, if the last line was an inverted vblank line. Really this should be
        //done earlier to get a 4.7 rather than a 1.5 us pulse, but this is simpler and still
        //works
        syncDisable();
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
        currentLineUpdated = true;//Current line was just updated
        
        if (currentLine == 262)//Detect end of frame
        {
            currentLine = 0;//Start new frame
            evenFrame = !evenFrame;//Useful for managing interlacing
        }
    }
}
