//File for testing composite initially; will eventually create serialcomposite.c for creating a neat serial display!
//USE -O3 optimization!!!!!
//Systick runs at 9mhz. For 15.7khz ntsc, systick interrupt once every 573.25 systick clock cycles
//Inspiration from:https://www.youtube.com/watch?v=PZsWqOuJFKI&ab_channel=BenHeckHacks

#define HSYNC_AMOUNT_LOW 170//4us
#define BACKPORCH_AMOUNT 275//4us (a bit more than that)
#define FRONTPORCH_AMOUNT 85//2us
#define VSYNC_AMOUNT_LOW 2850//59.667us

#include "bluepill.h"

volatile uint_fast16_t currentLine = 0;//0 to 261 inclusive
volatile bool currentLineUpdated;//Current line just updated by the ISR (time to modify pb4)

void configureSystick();//Sets up the isr
void displayLoop();

void main()
{
    GPIOB_CRL = 0x00400000;//PB4 as input; set as output for white, as input for black
    GPIOB_BSRR = 1 << 5;//Output hard 1 when set as output, float when set as input
    
    
    GPIOB_CRH = 0x00000007;//PB8 as 50mhz output (open collector)
    
    configureSystick();//Configures the ISR
    
    displayLoop();
}

const bool framebuffer[23][28] =
{
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},
    {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}
};

void displayLoop()
{
    while (true)
    {
        if (currentLineUpdated)//Time to update PB4 for line
        {
            currentLineUpdated = false;//We're handling the line update now; no need to do again
            
            if (currentLine > 17)//Drawing region
            {
                /* NOTE: A line is about 2000 __delayInstructions worth across
                 * On: GPIOB_CRL = 0x00300000;//TODO don't just affect all pb pins
                 * Off: GPIOB_CRL = 0x00400000;//TODO don't just affect all pb pins
                 */
                
                //Vertical bar test
                //GPIOB_CRL = 0x00300000;//TODO don't just affect all pb pins
                //__delayInstructions(500);
                
                //Vertical bar x2 test
                /*
                if (currentLine < 40)
                {
                    GPIOB_CRL = 0x00300000;//TODO don't just affect all pb pins
                    __delayInstructions(500);
                }
                else
                {
                    __delayInstructions(500);
                    GPIOB_CRL = 0x00300000;//TODO don't just affect all pb pins
                    __delayInstructions(100);
                }
                */
                
                //Diagonal bar test
                /*
                __delayInstructions(currentLine << 3);//Diagonal bar (currentLine*8)
                GPIOB_CRL = 0x00300000;//TODO don't just affect all pb pins
                __delayInstructions(50);
                */
                
                //Diagonal bar test 2
                /*
                uint32_t squish = currentLine << 2;
                __delayInstructions(squish);//Diagonal bar
                GPIOB_CRL = 0x00300000;//TODO don't just affect all pb pins
                __delayInstructions(50);
                GPIOB_CRL = 0x00400000;//TODO don't just affect all pb pins
                __delayInstructions(2000 - squish);//Diagonal bar
                GPIOB_CRL = 0x00300000;//TODO don't just affect all pb pins
                __delayInstructions(50);
                */
                
                //Diagonal bar test 3
                /*
                uint32_t squish = currentLine * 3;
                uint32_t doubleSquish = squish << 1;
                __delayInstructions(squish);//Diagonal bar (currentLine*2)
                GPIOB_CRL = 0x00300000;//TODO don't just affect all pb pins
                __delayInstructions(1);
                GPIOB_CRL = 0x00400000;//TODO don't just affect all pb pins
                __delayInstructions(2000 - doubleSquish);//Diagonal bar
                GPIOB_CRL = 0x00300000;//TODO don't just affect all pb pins
                __delayInstructions(1);
                */
                
                //Division Test
                //GPIOB_CRL = 0x00300000;//TODO don't just affect all pb pins
                //__delayInstructions(60000 / currentLine);
                
                //Division Test 2
                /*
                GPIOB_CRL = 0x00300000;//TODO don't just affect all pb pins
                if (currentLine < 100)
                {
                    __delayInstructions(60000 / currentLine);
                }
                else
                {
                    __delayInstructions(10000 / ((currentLine - 100) << 2));
                }
                */
                
                //Division test 3
                //__delayInstructions(60000 / currentLine);
                //GPIOB_CRL = 0x00300000;//TODO don't just affect all pb pins
                //__delayInstructions(10);
                
                //Vertical Line pattern
                /*for (int i = 0; i < 10; ++i)
                {
                    GPIOB_CRL = 0x00300000;//TODO don't just affect all pb pins
                    __delayInstructions(100);
                    GPIOB_CRL = 0x00400000;//TODO don't just affect all pb pins
                    __delayInstructions(100);
                }
                */
                
                //Exponential test (limited by integer division)
                /*
                uint32_t shiftValue = 1;
                for (int i = 0; i < (currentLine / 23); ++i)
                {
                    shiftValue = shiftValue << 1;
                }
                __delayInstructions(shiftValue);
                GPIOB_CRL = 0x00300000;//TODO don't just affect all pb pins
                __delayInstructions(10);
                */
                
                //Exponential test 2 (overcomes inter div limit by using base value closer to 1)
                /*
                uint32_t value = 100;
                for (int i = 0; i < (currentLine / 5); ++i)
                {
                    //Base is 10/9 (1.1111)
                    value = value * 10;
                    value = value / 9;
                }
                __delayInstructions((value - 120) / 15);
                GPIOB_CRL = 0x00300000;//TODO don't just affect all pb pins
                __delayInstructions(10);
                */
                
                //Framebuffer test
                uint_least8_t verticalPixel = (currentLine - 30) / 10;
                for (int i = 0; i < 28; ++i)
                {
                    if (framebuffer[verticalPixel][i])//Pixel is on
                        GPIOB_CRL = 0x00300000;//TODO don't just affect all pb pins
                    else//Pixel is off
                        GPIOB_CRL = 0x00400000;//TODO don't just affect
                        
                    __delayInstructions(50);
                }
            }
            else
                GPIOB_CRL = 0x00400000;//Turn white off//TODO don't just affect all pb pins
        }
        else
            GPIOB_CRL = 0x00400000;//Turn white off if line ends early//TODO don't just affect all pb pins
    }
}

void configureSystick()
{
    SYST_RVR = 573;//Isr will fire at 15734hz
    SYST_CVR = 0;//Reset the counter
    SYST_CSR = 0b011;//Use the "external" HCLK/8=9mhz clock for the counter, enable the __ISR_SysTick interrupt and the counter itself
}

__attribute__ ((interrupt ("IRQ"))) void __ISR_SysTick()//Runs at 15734hz
{
    //HSYNC and VSYNC Handling//TODO try to do less of this in the isr
    if (currentLine < 6)//Vertical blanking interval 1st black region
    {
        __delayInstructions(FRONTPORCH_AMOUNT);
        GPIOB_BRR = 1 << 8;//Pull PB8 low
        __delayInstructions(HSYNC_AMOUNT_LOW);
        GPIOB_BSRR = 1 << 8;//Let PB8 float high
        __delayInstructions(BACKPORCH_AMOUNT);
    }
    else if (currentLine < 12)//Vertical blanking interval inverted region
    {
        GPIOB_BRR = 1 << 8;//Pull PB8 low
        __delayInstructions(VSYNC_AMOUNT_LOW);//Keep low for longer
        GPIOB_BSRR = 1 << 8;//Let PB8 float high
    }
    else//Vertical blanking interval 2nd black region and hsync for drawing region
    {
        __delayInstructions(FRONTPORCH_AMOUNT);
        GPIOB_BRR = 1 << 8;//Pull PB8 low
        __delayInstructions(HSYNC_AMOUNT_LOW);
        GPIOB_BSRR = 1 << 8;//Let PB8 float high
        __delayInstructions(BACKPORCH_AMOUNT);
    }
    
    ++currentLine;//Increment the current line count
    currentLineUpdated = true;//Current line was just updated
    
    if (currentLine == 262)//Detect end of frame
        currentLine = 0;//Start new frame
}
