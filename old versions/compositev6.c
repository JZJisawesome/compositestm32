//TODO debug why there is a bit of a double image
/* Library to display composite video from a framebuffer on an STM32F103C8T6/B */
#include "bluepill.h"
#include "composite.h"

/* Constants */
//Step Numbers (all values are inclusive) (F1=Field 1, F2=Field 2)
//Note that during VBlank, 1 step is half the time of a step during the active region
#define RESET_VALUE -1//-1 because step is increment every time the counter resets
#define FRAME_BEGIN 0

#define F1_BEGIN 0
#define F1_VSYNC_BEGIN 0
#define F1_VSYNC_INV_BEGIN 6
#define F1_VSYNC_INV_END 11
#define F1_VSYNC_END 17
#define F1_ACTIVE_BEGIN 18
#define F1_VISIBLE_BEGIN 28
#define F1_VISIBLE_END 269
#define F1_ACTIVE_END 269
#define F1_END 269

#define F2_BEGIN 270//*Technically 271, but a partial line is useless anyways
#define F2_VSYNC_BEGIN 270
#define F2_VSYNC_INV_BEGIN 277
#define F2_VSYNC_INV_END 282
#define F2_VSYNC_END 287
#define F2_ACTIVE_BEGIN 288
#define F2_VISIBLE_BEGIN 299//*Slightly less than 299, but a partial line is useless anyways
#define F2_VISIBLE_END 540
#define F2_ACTIVE_END 540
#define F2_END 540

#define FRAME_END 540

//Timer Values//TODO increase TIMER_1,TIMER_2, and TIMER_3 when in TIMER_PSC_VBLANK
#define TIMER_PSC_VBLANK 0  //Used during vblank steps
#define TIMER_PSC_ACTIVE 1  //Used during active steps
#define TIMER_RELOAD 2288   //15734hz for active steps; double that freq for vblank steps
#define TIMER_1 49          //1.5us after TIMER_RELOAD (active steps);1.2us vblank/inv vb steps
#define TIMER_2_ACTIVE 225  //4.7us after TIMER_1 (active steps)
#define TIMER_2_VB          //2.3us after TIMER_1 (vblank steps)//TODO
#define TIMER_2_VB_INV      //27.1us after TIMER_1 (inverted vblank steps)//TODO
#define TIMER_3 379         //4.7us after TIMER_2_REG (active steps);inverted vblank dosn't care

/* Static Variables */

static const uint8_t* frameBuffer;//Pointer to framebuffer
static volatile uint_fast16_t step = RESET_VALUE;//0 to 540

/* Useful Macros */
//GPIO/DMA/SPI management
#define disableSPI() do {SPI1_CR1 &= ~(1 << 6);} while(0)//Disable SPI (forces PA7 low)
#define disableDMA() do {DMA_CCR3 &= ~(1);} while(0)//Disable DMA (saves bus/mem bandwidth)
#define enableSPI() do {SPI1_CR1 |= 1 << 6;} while(0)//Enable SPI (PA7 can now be high or low)
#define enableDMA() do {DMA_CCR3 |= 1;} while(0)//Enable DMA (transfer will begin b/c TXE==1)
#define disableVideo() do {disableSPI(); disableDMA();} while(0)
#define enableVideo() do {enableSPI(); enableDMA();} while(0)
#define syncEnable() do {GPIOB_BRR = 1 << 8;} while(0)//Pull PB8 low
#define syncDisable() do {GPIOB_BSRR = 1 << 8;} while(0)//Pull PB8 high

//Timer management (these take effect at next timer counter reset)
#define setTimerToVBlankMode() do {TIM4_PSC = TIMER_PSC_VBLANK;} while(0)
#define setTimerToActiveMode() do {TIM4_PSC = TIMER_PSC_ACTIVE;} while(0)

//Region determining macros (useful)
#define isInVisibleRegion() (((step >= F1_VISIBLE_BEGIN) && (step <= F1_VISIBLE_END)) || \
                             ((step >= F2_VISIBLE_BEGIN)/*&& (step <= F2_VISIBLE_END)*/))

#define isInInvSyncRegion() (((step >= F1_VSYNC_INV_BEGIN) && (step <= F1_VSYNC_INV_END)) || \
                             ((step >= F2_VSYNC_INV_BEGIN) && (step <= F2_VSYNC_INV_END)))

#define nextStepBeginsActiveRegion() (((step + 1) == F1_ACTIVE_BEGIN) || \
                                      ((step + 1) == F2_ACTIVE_BEGIN))

#define nextStepBeginsVBlankRegion() (((step + 1) == F1_VSYNC_BEGIN) || \
                                      ((step + 1) == F2_VSYNC_BEGIN))

/* Functions */

//Public Functions
void Composite_init(const uint8_t* fb)//Pointer to framebuffer
{
    //Store pointer to framebuffer
    frameBuffer = fb;
    
    //Pin Configuration
    GPIOA_CRL = (GPIOA_CRL & 0x0FFFFFFF) | 0xB0000000;//PA7 as 50mhz AF push-pull output
    GPIOB_CRH = (GPIOB_CRH & 0xFFFFFFF0) | 0x3;//PB8 as 50mhz push-pull output
    
    //SPI Configuration
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
    TIM4_PSC = TIMER_PSC_VBLANK;//1 during active region, 0 during Vblank in order to double frequency
    TIM4_ARR = TIMER_RELOAD;//Use reload register to reset counter at rate of 15734hz when PSC==1
    
    TIM4_CCMR1 = 0x1818;//Set compare channel 1 and 2 to enable flag on match
    TIM4_CCMR2 = 0x0018;//Set compare channel 3 to enable flag on match
    TIM4_CCR1 = TIMER_1;//Set oc1ref after 1.5us
    TIM4_CCR2 = TIMER_2_ACTIVE;//Set oc2ref high 4.7us after that (6.2us after timer reset)
    TIM4_CCR3 = TIMER_3;//Set oc2ref high 4.7us after that (10.9us after timer reset)
    //NOTE: raw math says 
    
    TIM4_EGR = 0x0001;//Generate update even to initialize things
    
    TIM4_DIER = 0x000F;//Enable channel 1, 2, 3, and counter reset (uif) interrupts
    NVIC_ISER0 = 1 << 30;//Enable timer 4 interrupt in the nvic
    TIM4_CR1 = 0x0001;//Enable counter (upcounting)
}

uint_fast16_t getCurrentStep()
{
    return step;
}

//Private Functions

__attribute__ ((interrupt ("IRQ"))) void __ISR_TIM4()
{
    uint_fast16_t timerStatus = TIM4_SR;//Save value of TIM4_SR for speed (atomicity unneeded)
    TIM4_SR = 0;//Clear all flags "the fast way" (instead of clearing individually in switch)
    
    switch (timerStatus & 0x000F)
    {
        case 1://Timer count reset to 0: Start of front porch (and of step)
        {
            ++step;//Increment step
            //TODO move overflow behaviour here
            //if (step == FRAME_END)
            //    step = 0;
            //else
            //    ++step;
            
            syncDisable();//Disable extended sync if left on
            
            //Disable video pin for the front porch and rest of hblank/vblank
            disableVideo();
            
            //Setup DMA and SPI for the current line if it is an active (non-vblank) line
            if (isInVisibleRegion())
            {
                DMA_CNDTR3 = COMPOSITE_BYTES_PER_LINE;//Reset transfer counter
                
                //Setup initial DMA memory address
                uint32_t visibleLine;//Relative to start of visible region
                uint32_t offset;//Relative to start of frameBuffer
                
                //Determine value of visibleLine based on current field and determine offset
                if (step <= F1_END)//In field 1
                {
                    visibleLine = step - F1_VISIBLE_BEGIN;//Relative to F1_VISIBLE_BEGIN
                    
                    #ifdef COMPOSITE_INTERLACING
                        offset = visibleLine * (COMPOSITE_BYTES_PER_LINE * 2);//Even lines
                    #endif
                }
                else//Field 2
                {
                    visibleLine = step - F2_VISIBLE_BEGIN;//Relative to F2_VISIBLE_BEGIN
                    
                    #ifdef COMPOSITE_INTERLACING//If we're interlacing (and it's field 2)
                        offset = visibleLine * (COMPOSITE_BYTES_PER_LINE * 2);
                        offset += COMPOSITE_BYTES_PER_LINE;//Odd lines
                    #endif
                }
                
                #ifndef COMPOSITE_INTERLACING//If we're not interlacing
                    offset = visibleLine * COMPOSITE_BYTES_PER_LINE;
                #endif
                
                //Set initial DMA memory address based on frameBuffer pointer and offset
                const uint8_t const* lineAddress = frameBuffer + offset;
                DMA_CMAR3 = (uint32_t)lineAddress;
            }
            break;
        }
        case 1 << 1://Timer compare match 1: Start of sync pulse (end of front porch)
        {
            syncEnable();
            break;
        }
        case 1 << 2://Timer compare match 2: End of sync pulse
        {
            if (!isInInvSyncRegion())
                syncDisable();
            //Else sync lasts longer (is reset at beginning of next front porch) (inverted sync)
            break;
        }
        case 1 << 3://Timer compare match 3: End of back porch, start of image, end of step
        {
            //TODO move this to the first case where step is incremented
            if (step == FRAME_END)//Last step of frame (also last stepof field 2)
                step = RESET_VALUE;//Reset step count for next frame
            
            if (nextStepBeginsVBlankRegion())//Next step will be start of field 1 or 2 vblank
                setTimerToVBlankMode();//Vblank requires double the frequency
            
            if (nextStepBeginsActiveRegion())//Next step will begin the active region
                setTimerToActiveMode();//Requires the regular frequency (15734hz)
            
            if (isInVisibleRegion())//Current line is in the visible (drawing) region
                enableVideo();
            break;
        }
    }
}
