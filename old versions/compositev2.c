//TODO in a future version, make delays non-blocking
//Maybe abuse PWM or UART or SPI? Or just set timer interrupts whenever a delay is needed
//Could make use of timer synchronization
//BE SURE TO BUILD WITH -O3
//PB4 is video
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
#define HSYNC_AMOUNT 222//4.7us
#define BACKPORCH_AMOUNT 222//4.7us
#define FRONTPORCH_AMOUNT 71//1.5us
#define VSYNC_AMOUNT 2811//59.55662896us
#define ACTIVE_AMOUNT 2486//52.65662896us
#define LINE_AMOUNT 3000//63.55662896us
#define HBLANK_AMOUNT 515//10.9us

#include "bluepill.h"

volatile uint_fast16_t currentLine = 0;//0 to 261 inclusive
volatile bool currentLineUpdated = true;//Current line just updated by the ISR//Starts as true to handle line 0

//Customizable functions
void renderLine(uint_fast8_t activeLine);//Called after hsync in active region
void processing();//Called after line is updated; in active or vblank region

//Screen Management Functions/Macros
void configureHorizontalLineTimer();
void displayLoop();
#define displayWhite() {GPIOB_BSRR = 1 << 5;}//Pull PB5 high
#define displayBlack() {GPIOB_BRR = 1 << 5;}//Pull PB5 low

//Obselete functions
void configureSystick();
void delayMicroseconds(uint32_t time);

void main()
{
    GPIOB_CRL = 0x00300000;//PB8 as 50mhz output (push-pull)
    GPIOB_CRH = 0x00000003;//PB8 as 50mhz output (push-pull)
    GPIOB_BSRR = 1 << 5;//Prep PB4 to be switched between input and output
    
    configureHorizontalLineTimer();
    
    displayLoop();//Never exits
}

/* Customization Functions */
//On an active line, the time to execute renderLine + processing must be <= 52.65662896us
//On an inactive line, processing must take <= 52.65662896us (renderLine not called)
//On lines 6 to 11 (inverted vblank), processing must take <= 4us
//TODO improve processing time in future versions

#include "images.h"

void renderLine(uint_fast8_t activeLine)//Only called on active lines
{
    //testing
    //__delayInstructions(500);
    //displayWhite();
    
    /*
    uint32_t squish = activeLine * 3;
    uint32_t doubleSquish = squish << 1;
    __delayInstructions(squish);//Diagonal bar (activeLine*2)
    displayWhite();
    __delayInstructions(1);
    displayBlack();
    __delayInstructions(2000 - doubleSquish);//Diagonal bar
    displayWhite();
    __delayInstructions(1);
    displayBlack();
    */
    
    /*
    uint32_t value = 100;
    for (uint32_t i = 0; i < (activeLine / 5); ++i)
    {
        //Base is 10/9 (1.1111)
        value = value * 10;
        value = value / 9;
    }
    __delayInstructions(value / 15);
    displayWhite();
    __delayInstructions(10);
    displayBlack();
    */
    /*
    __delayInstructions((ACTIVE_AMOUNT * 25) / (activeLine + 25));
    displayWhite();
    __delayInstructions(10);
    displayBlack();
    */
    
    //Framebuffer Demo 1
    /*
    uint_least8_t verticalPixel = (activeLine - 10) / 1;//Was 10
    for (int i = 0; i < 31; ++i)
    {
        if (framebuffer[verticalPixel][i])//Pixel is on
            displayWhite();
        else//Pixel is off
            displayBlack();
            
        //__delayInstructions(5);//Was 50
    }
    */
    
    //Framebuffer Demo 2 (64 by 64)
    /*
    uint_least8_t verticalPixel = (activeLine - 10) / 4;//Shift pixels down a bit and scale
    uint32_t offset = verticalPixel * 8;
    for (int i = 0; i < 8; ++i)//Make 64 by 64
    {
        for (int j = 0; j < 8; ++j)
        {
            if (demoImage2[offset + i] & (0x80 >> j))//Pixel is on
                displayWhite();
            else//Pixel is off
                displayBlack();
            
            __delayInstructions(4);//Scale
        }
    }
    */
    
    //Framebuffer Demo 3 (128 by 128)
    /*
    uint_least8_t verticalPixel = activeLine - 10;//Shift pixels down a bit
    uint32_t offset = verticalPixel * 16;
    for (int i = 0; i < 16; ++i)//Make 128 by 128
    {
        for (int j = 0; j < 8; ++j)
        {
            if (demoImage3[offset + i] & (0x80 >> j))//Pixel is on
                displayWhite();
            else//Pixel is off
                displayBlack();
        }
    }
    */
    
    //Framebuffer Demo 4 (256 by 256)
    
    /*
    const uint8_t const* image = demoImage4;
    
    uint_least8_t verticalPixel = activeLine;//Shift pixels down a bit
    uint32_t offset = verticalPixel * 32;
    for (int i = 0; i < 32; ++i)//Make 256 by 256
    {
        for (int j = 0; j < 8; ++j)
        {
            if (image[offset + i] & (0x80 >> j))//Pixel is on
                displayWhite();
            else//Pixel is off
                displayBlack();
        }
    }
    */
    
    //Framebuffer Demo 5 (320 by 256)
    //Note: not quite 256 pixels vertically; be careful
    const uint8_t const* image = demoImage5;
    //const uint8_t const* image = demoImage6;

    //Offset needed because it is a 1d array
    uint32_t offset = activeLine * 40;//1 line = 1 vertical pixel
    for (int i = 0; i < 40; ++i)//Make 320 by 256
    {
        for (int j = 0; j < 8; ++j)
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
    displayBlack();//Cut off image to avoid streaks at the right side
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
            displayBlack();//Ensure display is black in blanking regions
            
            if ((currentLine > 5) && (currentLine < 12))//Vsync lines (inverted)
            {
                GPIOB_BRR = 1 << 8;//Pull PB8 low
                __delayInstructions(VSYNC_AMOUNT);//Keep low for longer because "inverted"
                GPIOB_BSRR = 1 << 8;//Let PB8 float high
            }
            else//Hsync for non-inverted vblank lines and active region
            {
                __delayInstructions(FRONTPORCH_AMOUNT);
                GPIOB_BRR = 1 << 8;//Pull PB8 low
                __delayInstructions(HSYNC_AMOUNT);
                GPIOB_BSRR = 1 << 8;//Let PB8 float high
                __delayInstructions(BACKPORCH_AMOUNT);
            }
            
            if (currentLine > 17)//Inside of the active region
                renderLine((uint_fast8_t)(currentLine - 18));//18 lines of vblank
        }
        
        processing();
    }
}

//Horizontal Line Timer (15734hz; Increments currentLine and updates currentLineUpdated)

void configureHorizontalLineTimer()
{
    //Useful: https://vivonomicon.com/2018/05/20/bare-metal-stm32-programming-part-5-timer-peripherals-and-the-system-clock/
    TIM4_PSC = 0;//72Mhz
    TIM4_ARR = 4576;//Use reload register to trigger interrupt at 15734hz
    TIM4_EGR |= 0x0001;//Generate update even to initialize things
    
    TIM4_DIER = 0x0001;//Enable UIE interrupt
    NVIC_ISER0 = 1 << 30;//Enable timer 4 interrupt in the nvic
    TIM4_CR1 |= 0x0001;//Enable counter (upcounting)
}

__attribute__ ((interrupt ("IRQ"))) void __ISR_TIM4()//15734hz
{
    TIM4_SR &= 0xFFFE;//Clear the UIE interrupt bit in the timer
    
    ++currentLine;//Increment the current line count
    currentLineUpdated = true;//Current line was just updated
    
    if (currentLine == 262)//Detect end of frame
        currentLine = 0;//Start new frame
}

/* Old code */

//SysTick Microsecond Timer (Used for timing during a line)
//NOTE: UNUSED BECAUSE NOT SMALL ENOUGH TIMING
/*
void delayMicroseconds(uint32_t time)//TODO make more accurate (implement in asm?)
{
    SYST_VAL = 0;//Reset the counter (ensures we're not in the middle of the count)
    
    for (uint32_t i = 0; i < time; ++i)
        while (!(SYST_CTRL & 0x00010000));//Wait for COUNTFLAG to be set
}

void configureSystick()
{
    SYST_RVR = 8;//COUNTFLAG will be set at 1Mhz (1us)
    SYST_CTRL = 0b001;//Use the "external" HCLK/8=9mhz clock for the counter and enable it
}
*/
