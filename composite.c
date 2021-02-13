//TODO debug why there is a bit of a double image
/* Library to display composite video from a framebuffer on an STM32F103C8T6/B */
#include "bluepill.h"
#include "composite.h"

/* Constants */

//Step numbers (all values are inclusive) (F1=Field 1, F2=Field 2)
//Note that during VBlank, 1 step is half the time of a step during the active region
#define FRAME_BEGIN 0

#define F1_BEGIN 0
#define F1_VSYNC_BEGIN 0
#define F1_VSYNC_INV_BEGIN 6
#define F1_VSYNC_INV_END 11
#define F1_VSYNC_REBEGIN 12
#define F1_VSYNC_END 17
#define F1_ACTIVE_BEGIN 18
#define F1_VISIBLE_BEGIN 28
#define F1_VISIBLE_END 269
#define F1_ACTIVE_END 269
#define F1_END 269

#define F2_BEGIN 270//Technically 271, but a partial line is useless anyways
#define F2_VSYNC_BEGIN 270
#define F2_VSYNC_INV_BEGIN 277
#define F2_VSYNC_INV_END 282
#define F2_VSYNC_REBEGIN 283
#define F2_VSYNC_END 287
#define F2_ACTIVE_BEGIN 288
#define F2_VISIBLE_BEGIN 299//Slightly less than 299, but a partial line is useless anyways
#define F2_VISIBLE_END 540
#define F2_ACTIVE_END 540
#define F2_END 540

#define FRAME_END 540
#define F1_BEGIN_NO_OVERFLOW 541        //Used to make some checks simpler
#define F1_VSYNC_BEGIN_NO_OVERFLOW 541  //Used to make some checks simpler

//Timer values; NOTE: May need manual tuning for a particular microcontroller
#define TIMER_PSC_VBLANK 0  //Used during vblank steps
#define TIMER_PSC_ACTIVE 1  //Used during active steps
#define TIMER_RELOAD 2288   //15734hz for active steps; double thatfor vblank steps (31468hz)
#define TIMER_1 53          //1.5us after TIMER_RELOAD (active steps);1.2us vblank/inv vb steps
#define TIMER_2_ACTIVE 222  //4.7us after TIMER_1 (active steps)
#define TIMER_2_VB 166      //2.3us after TIMER_1 (vblank steps)
#define TIMER_2_VB_INV 1951 //27.1us after TIMER_1 (inverted vblank steps)
#define TIMER_3 392         //4.7us after TIMER_2_REG (active steps);inverted vblank dosn't care

//Common configuration constants (In one place to allow for easy reuse by macros & init code)
//SPI_CR1
#define SPI_BARE_SETTINGS 0b0000001100000100//NSS software input and MSBFIRST and Master mode
#define SPI_BARE (SPI_BARE_SETTINGS | (COMPOSITE_SPI_PRESCALER_VALUE << 3))
#define SPI_ENABLE (SPI_BARE | (1 << 6))//Enables SPI peripheral
#define SPI_DISABLE SPI_BARE//Does not set SPI enable bit unlike SPI_ENABLE
//DMA_CCR3
#define DMA_BARE 0x2090//3/4 priority, 8 bit access, memory postincrement, memory to peripheral
#define DMA_ENABLE (DMA_BARE | 1)//Enables DMA channel 3
#define DMA_DISABLE DMA_BARE//Does not set channel enable bit unlike DMA_ENABLE

/* Static Variables */

static const uint8_t* frameBuffer;//Pointer to framebuffer//TODO must this be volatile?
static volatile uint_fast16_t step = -1;//0 to 540

/* Useful Macros */

//GPIO/DMA/SPI management
#define disableSPI() do {SPI1_CR1 = SPI_DISABLE;} while(0)//Disable SPI (forces PA7 low)
#define disableDMA() do {DMA_CCR3 = DMA_DISABLE;} while(0)//Disable DMA (saves bus/mem bandwidth)
#define enableSPI() do {SPI1_CR1 = SPI_ENABLE;} while(0)//Enable SPI (PA7 can now change)
#define enableDMA() do {DMA_CCR3 = DMA_ENABLE;} while(0)//Enable DMA (transfer will begin b/c TXE==1)
#define disableVideo() do {disableSPI(); disableDMA();} while(0)
#define enableVideo() do {enableSPI(); enableDMA();} while(0)
#define syncEnable() do {GPIOB_BRR = 1 << 8;} while(0)//Pull PB8 low
#define syncDisable() do {GPIOB_BSRR = 1 << 8;} while(0)//Pull PB8 high

/* Public Functions */

void Composite_init(const uint8_t* fb)//Pointer to framebuffer
{
    //Store pointer to framebuffer
    frameBuffer = fb;
    
    //Pin Configuration
    GPIOA_CRL = (GPIOA_CRL & 0x0FFFFFFF) | 0xB0000000;//PA7 as 50mhz AF push-pull output
    GPIOB_CRH = (GPIOB_CRH & 0xFFFFFFF0) | 0x00000003;//PB8 as 50mhz push-pull output
    
    //SPI Configuration
    SPI1_CR1 = SPI_DISABLE;
    SPI1_CR2 = 1 << 1;//Enable dma request when transmit buffer is empty
    
    //DMA Configuration (Channel 3)
    DMA_CCR3 = DMA_DISABLE;//DMA starts disabled
    DMA_CPAR3 = (uint32_t)(&SPI1_DR);//Streaming pixel data to SPI1_DR
    
    //Timer Configuration
    //Useful Reference:vivonomicon.com/2018/05/20/bare-metal-stm32-programming-part-5-timer-peripherals-and-the-system-clock/
    TIM4_PSC = TIMER_PSC_VBLANK;//Starting at the vertical blanking frequency
    TIM4_ARR = TIMER_RELOAD;//Set reload register value
    
    TIM4_CCMR1 = 0x1818;//Set compare channel 1 and 2 to enable flag on match
    TIM4_CCMR2 = 0x0018;//Set compare channel 3 to enable flag on match
    TIM4_CCR1 = TIMER_1;//Set compare value for channel 1
    TIM4_CCR2 = TIMER_2_VB;//Set compare value for channel 2 (starting in VBlank)
    TIM4_CCR3 = TIMER_3;//Set compare value for channel 1
    
    TIM4_EGR = 1;//Generate update event to initialize things
    
    TIM4_DIER = 0x000F;//Enable compare channel 1, 2, and 3, and counter reset (uif) interrupts
    NVIC_ISER0 = 1 << 30;//Enable timer 4 interrupt in the nvic
    TIM4_CR1 = 1;//Enable timer (upcounting)
}

void Composite_setFramebuffer(const uint8_t* fb)//Pointer to framebuffer
{
    //Store pointer to framebuffer
    frameBuffer = fb;
}

uint_fast16_t Composite_getCurrentStep()
{
    return step;
}

/* Private Functions
 * Timer Reset: Disable video for front porch, increment step, detect if visible region
 * Timer CMP 1: Enable sync; if visible region, configure DMA for current step/line number
 * Timer CMP 2: Disable sync (back porch); configure timer settings for next step
 * Timer CMP 3: Enable video and DMA to start drawing pixels for the current line
 * Timer ARR value hit: Apply new timer frequency and compare values and reset timer
*/

__attribute__ ((interrupt ("IRQ"))) void __ISR_TIM4()
{
    static bool isVisibleStep;//Used to evaluate current region faster in t cmp interrupt 3
    uint_fast16_t timerStatus = TIM4_SR;//Save value of TIM4_SR for speed (atomicity unneeded)
    TIM4_SR = 0;//Clear all flags "the fast way" (instead of clearing individually in switch)
    
    switch (timerStatus & 0x000F)
    {
        case 1://Timer count reset to 0: Start of front porch (and of step)
        {
            //Disable SPI and DMA for the front porch and rest of hblank/vblank
            disableVideo();
            
            //Determine the current step and update
            if (step == FRAME_END)
                step = 0;//Wrap around to first step
            else
                ++step;//Increment step
            
            //Determine if this step is visible (if DMA will need configuration before t cmp 3)
            //TODO disable/enable tc3 on the fly instead of using isVisibleStep
            isVisibleStep = (((step >= F1_VISIBLE_BEGIN)  && (step <= F1_VISIBLE_END)) ||
                             ((step >= F2_VISIBLE_BEGIN)/*&& (step <= F2_VISIBLE_END)*/));
                
            break;
        }
        case 1 << 1://Timer compare match 1: Start of sync pulse (end of front porch)
        {
            syncEnable();
            
            //Sets up DMA for the current step if it is a visible line step (non-vblank)
            if (isVisibleStep)
            {
                DMA_CNDTR3 = COMPOSITE_BYTES_PER_LINE;//Reset transfer counter
                
                //Setup initial DMA memory address
                uint32_t visibleLine;//Relative to start of visible region
                uint32_t offset;//Relative to start of frame buffer
                const bool inField1 = step <= F1_END;
                
                //Determine value of visibleLine based on current field
                if (inField1)//In field 1
                    visibleLine = step - F1_VISIBLE_BEGIN;//Relative to F1_VISIBLE_BEGIN
                else
                    visibleLine = step - F2_VISIBLE_BEGIN;//Relative to F2_VISIBLE_BEGIN
                
                visibleLine /= COMPOSITE_LINE_DIVISOR;//Repeat lines if specified by constant
                
                //Determine offset into framebuffer
                #ifdef COMPOSITE_INTERLACING
                    offset = visibleLine * (COMPOSITE_BYTES_PER_LINE * 2);//Even lines
                
                    if (!inField1)//If we're in field 2
                        offset += COMPOSITE_BYTES_PER_LINE;//Make odd lines by adding to even
                #else//Not interlacing
                    offset = visibleLine * COMPOSITE_BYTES_PER_LINE;
                #endif
                
                //Set initial DMA memory address based on frame buffer pointer and offset
                const uint8_t const* lineAddress = frameBuffer + offset;
                DMA_CMAR3 = (uint32_t)lineAddress;
            }
            
            break;
        }
        case 1 << 2://Timer compare match 2: End of sync pulse (start of back porch for hblank)
        {
            //Note; depending on the current step, the compare value will change and this
            //interrupt will fire at a different amount of time after the timer compare 1 one
            syncDisable();
            
            //Configures timer prescaler and second timer compare value for the next step
            //Remember that update even occurs every time the timer is reloaded with 0
            uint_fast16_t dumbNextStep = step + 1;//Does not take overflow of 540 into account
            switch (dumbNextStep)//Prescaler
            {
                //Next step will be the start of a VBlank region
                case F1_VSYNC_BEGIN_NO_OVERFLOW://VBlank region of field 1
                case F2_VSYNC_BEGIN://VBlank region of field 2
                {
                    TIM4_PSC = TIMER_PSC_VBLANK;//Double the timer's frequency (31468hz)
                    break;
                }
                //Next step will be the start of an active region
                case F1_ACTIVE_BEGIN:
                case F2_ACTIVE_BEGIN:
                {
                    TIM4_PSC = TIMER_PSC_ACTIVE;//Regular frequency (15734hz)
                    break;
                }
            }
            
            switch (dumbNextStep)//Timer compare value 2
            {
                //Next step will be the start of a non-inverted vblank region
                case F1_VSYNC_BEGIN_NO_OVERFLOW://1st non-inverted vblank region of field 1
                case F1_VSYNC_REBEGIN://2nd non-inverted vblank region of field 1
                case F2_VSYNC_BEGIN://1st non-inverted vblank region of field 2
                case F2_VSYNC_REBEGIN://2nd non-inverted vblank region of field 2
                {
                    TIM4_CCR2 = TIMER_2_VB;//Adjust timer compare int 2 for 2.3us sync pulse
                    break;
                }
                //Next step will be the start of an inverted vblank region
                case F1_VSYNC_INV_BEGIN://Inverted vblank region of field 1
                case F2_VSYNC_INV_BEGIN://Inverted vblank region of field 2
                {
                    TIM4_CCR2 = TIMER_2_VB_INV;//Adjust timer cmp int 2 for 27.1us sync pulse
                    break;
                }
                //Next step will be the start of an active region
                case F1_ACTIVE_BEGIN:
                case F2_ACTIVE_BEGIN:
                {
                    TIM4_CCR2 = TIMER_2_ACTIVE;//Adjust timer compare int 2 for 4.7us sync pulse
                    break;
                }
            }
            
            break;
        }
        case 1 << 3://Timer compare match 3: End of back porch, start of image
        {
            //Start drawing the image for the line. We can assume that this line is a visible 
            //line because otherwise the timer compare 3 interrupt would not have fired
            //TODO disable/enable tc3 on the fly as explained above instead of this
            //TODO would save an if statement and avoid timer compare interupt 3 during vblank
            if (isVisibleStep)//Remove this
                enableVideo();//Just keep this
            
            break;
        }
    }
}
