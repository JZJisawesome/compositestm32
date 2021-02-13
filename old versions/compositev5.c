//FIXME frame sync should be double the frequency and twice as many pulses
//See the issue here: http://martin.hinner.info/vga/pal.html
/* Library to display composite video from a framebuffer on an STM32F103C8T6/B */
#include "bluepill.h"
#include "composite.h"

/* Static Variables */

static const uint8_t* frameBuffer;//Pointer to framebuffer

//NOTE: Odd fields are even frames (262 lines) and even fields are odd frames (263 lines). 
//When interlacing, even lines are displayed on even fields (on odd frames) and odd lines
//are displayed on odd fields (on even frames)
static volatile uint_fast16_t currentLine = 0;//0 to 261 or 0 to 262 inclusive
static volatile bool evenFrame = true;//Starting on an even frame (0) which is an odd field (1)

/* Useful Macros */

#define disableVideo() {SPI1_CR1 &= ~(1 << 6);}//Disable SPI (forces PA7 low)
#define enableVideo() {SPI1_CR1 |= 1 << 6;}//Enable SPI (PA7 can now be whatever)
#define syncEnable() {GPIOB_BRR = 1 << 8;}//Pull PB8 low
#define syncDisable() {GPIOB_BSRR = 1 << 8;}//Pull PB8 high

/* Functions */

//Public Functions
//FIXME frame sync should be double the frequency and twice as many pulses
void Composite_init(const uint8_t* fb)//Pointer to framebuffer
{
    //Store pointer to framebuffer
    frameBuffer = fb;
    
    //Pin Configuration
    GPIOA_CRL = (GPIOA_CRL & 0x0FFFFFFF) | 0xB0000000;//PA7 as 50mhz AF push-pull output
    GPIOB_CRH = (GPIOB_CRH & 0xFFFFFFF0) | 0x3;//PB8 as 50mhz push-pull output
    GPIOA_BRR = 1 << 7;//Ensure PA7 is normally low so that disableVideo works properly
    
    //Setup SPI
    //Enable NSS software input and hold high to keep in master mode & MSBFIRST
    //& Leave prescaler section as 0b000 & Master mode
    const uint32_t spiSettings = 0b0000001100000100;
    //The bits that determine the prescaler are SPI1_CR1[5:3]. Shift SPI prescaler value and use
    const uint32_t shiftedPrescalerValue = COMPOSITE_SPI_PRESCALER_VALUE << 3;
    SPI1_CR1 = spiSettings | shiftedPrescalerValue;//Store settings and prescaler value
    
    SPI1_CR2 = 0x0002;//Enable dma request when transmit buffer is empty
    
    //Setup DMA (Channel 3)
    DMA_CCR3 = 0x2090;//3/4 priority, 8 bit accesses, memory postincrement, memory to peripheral
    DMA_CNDTR3 = COMPOSITE_BYTES_PER_LINE;
    DMA_CPAR3 = 0x4001300C;//Address of SPI1_DR, which we are streaming pixel data to
    DMA_CMAR3 = (uint32_t)(frameBuffer);//1 line drawn will start at address of frameBuffer
    
    //Setup horizontal line counter
    //Useful Reference:vivonomicon.com/2018/05/20/bare-metal-stm32-programming-part-5-timer-peripherals-and-the-system-clock/
    TIM4_PSC = 0;//72Mhz timer increment rate
    TIM4_ARR = 4575;//Use reload register to reset counter at rate of 15734hz
    
    TIM4_CCMR1 = 0x1818;//Set compare channel 1 and 2 to enable flag on match
    TIM4_CCMR2 = 0x0018;//Set compare channel 3 to enable flag on match
    TIM4_CCR1 = 98;//Set oc1ref after 1.5us
    TIM4_CCR2 = 409;//Set oc2ref high 4.7us after that (6.2us after timer reset)
    TIM4_CCR3 = 760;//Set oc2ref high 4.7us after that (10.9us after timer reset)
    //NOTE: raw math says 107, 445, 784. Extra tuning done afterwards to perfect timings
    
    TIM4_EGR = 0x0001;//Generate update even to initialize things
    
    TIM4_DIER = 0x000F;//Enable channel 1, 2, 3, and counter reset (uif) interrupts
    NVIC_ISER0 = 1 << 30;//Enable timer 4 interrupt in the nvic
    TIM4_CR1 = 0x0001;//Enable counter (upcounting)
}

uint_fast16_t getCurrentLine()
{
    return currentLine;
}

bool isEvenFrame()//AKA is odd field
{
    return evenFrame;
}

//Private Functions
//FIXME frame sync should be double the frequency and twice as many pulses
__attribute__ ((interrupt ("IRQ"))) void __ISR_TIM4()
{
    //NOTE: Odd fields are even frames (262 lines) and even fields are odd frames (263 lines). 
    //When interlacing, even lines are displayed on even fields (on odd frames) and odd lines
    //are displayed on odd fields (on even frames)
    
    uint_fast16_t timerStatus = TIM4_SR;//Save value of TIM4_SR for speed (atomicity unneeded)
    TIM4_SR = 0;//Clear all flags "the fast way" (instead of clearing individually in switch)
    
    switch (timerStatus & 0x000F)
    {
        case 1://Check if it was a uif interrupt
        {
            //Disable sync now, if the last line was an inverted vblank line. Really this should 
            //be done earlier to get a 4.7 rather than a 1.5 us pulse (same as the front porch), 
            //but this is simpler and still appears to work
            syncDisable();
            
            //Stop DMA and setup for next line
            disableVideo();//Disable SPI to stop drawing white if it is still
            DMA_CCR3 &= ~(1);//Disable DMA ch3 to change settings & save bandwidth during hblank
            
            const uint_fast16_t nextLine = currentLine + 1;
            if (nextLine > 20)//After hblank, the line will be an active line (offset by 1)
            {
                DMA_CNDTR3 = COMPOSITE_BYTES_PER_LINE;//Reset transfer counter
                
                const uint32_t activeLine = nextLine - 21;//Relative to actual screen top
                uint32_t offset;
                
                #ifdef COMPOSITE_INTERLACING
                    offset = activeLine * (COMPOSITE_BYTES_PER_LINE * 2);//Every other line
                    
                    if (evenFrame)//Odd lines on odd fields (even frames)
                        offset += COMPOSITE_BYTES_PER_LINE;
                #else
                    offset = activeLine * COMPOSITE_BYTES_PER_LINE;//Skip bytes in previous lines
                #endif
                    
                const uint8_t const* lineAddress = frameBuffer + offset;
                DMA_CMAR3 = (uint32_t)lineAddress;
            }
            break;
        }
        case 1 << 1://Check if it was a channel 1 interrupt
        {
            syncEnable();//Front porch finished, time for sync
            break;
        }
        case 1 << 2://Check if it was a channel 2 interrupt
        {
            //TODO figure out why these have these are 1 less than they should be
            //(currentLine < 6) | (currentLine > 11) makes more sense, but causes
            //7 reg, 6 inv, 5 reg instead of 6 reg, 6 inv, 6 reg
            if ((currentLine < 5) | (currentLine > 10))//Non-inverting sync region
                syncDisable();
            
            break;
        }
        case 1 << 3://Check if it was a channel 3 interrupt
        {
            ++currentLine;//Increment the current line count
            
            //Even frames, whether we are interlacing or not, are 262 scanlines; odd are 263
            //Technically this is backwards but it doesn't really matter
            if (((currentLine == 262) & (evenFrame)) | (currentLine == 263))
            {//TODO fix DMA reading out of bounds when there is an extra line
                currentLine = 0;//Start new frame
                evenFrame = !evenFrame;//Used for managing interlacing and next frame detection
            }
            else if (currentLine > 20)//Active area (after 21 lines of vsync)
            {//FIXME should the value above be 21 instead of 20? (22 lines of vsync?)
                enableVideo();//Re-enable SPI now that blanking is finished
                DMA_CCR3 |= 0x0001;//Enable DMA channel 3 for the current line
            }
            
            break;
        }
    }//End of switch
}
