#include "Owl.h"
#include "device.h"
#include "errorhandlers.h"
#include "Pin.h"
#include "ApplicationSettings.h"
#include "OpenWareMidiControl.h"
#include "message.h"
#include "Codec.h"
#include "flash.h"
#include "Storage.h"
#include "ServiceCall.h"
#ifdef USE_USBD_AUDIO_TX_PRE_FX
#include <string.h>
#include "usbd_audio.h"
#include "CircularBuffer.h"
#endif

#define XIBECA_PIN3  GPIOD, GPIO_PIN_2
#define XIBECA_PIN4  GPIOG, GPIO_PIN_10
#define XIBECA_PIN5  GPIOC, GPIO_PIN_9
#define XIBECA_PIN6  GPIOC, GPIO_PIN_8
#define XIBECA_PIN7  GPIOC, GPIO_PIN_13
#define XIBECA_PIN8  GPIOI, GPIO_PIN_11
#define XIBECA_PIN9  GPIOC, GPIO_PIN_14
#define XIBECA_PIN10 GPIOA, GPIO_PIN_0
#define XIBECA_PIN11 GPIOH, GPIO_PIN_4
#define XIBECA_PIN12 GPIOC, GPIO_PIN_15
#define XIBECA_PIN13 GPIOA, GPIO_PIN_7
#define XIBECA_PIN14 GPIOA, GPIO_PIN_3

#define XIBECA_PIN17 GPIOG, GPIO_PIN_12
#define XIBECA_PIN18 GPIOG, GPIO_PIN_11
#define XIBECA_PIN19 GPIOA, GPIO_PIN_15
#define XIBECA_PIN20 GPIOA, GPIO_PIN_1

#define XIBECA_PIN21 GPIOC, GPIO_PIN_6
#define XIBECA_PIN22 GPIOC, GPIO_PIN_7
#define XIBECA_PIN23 GPIOD, GPIO_PIN_12
#define XIBECA_PIN24 GPIOD, GPIO_PIN_13

Pin led_in1(XIBECA_PIN13);
Pin led_in2(XIBECA_PIN14);
Pin led_in3(XIBECA_PIN19);
Pin led_in4(XIBECA_PIN20);

Pin led_clip1(XIBECA_PIN10); // PA0
Pin led_clip2(XIBECA_PIN7);  // PC13
Pin led_clip3(XIBECA_PIN8);  // PI11
Pin led_clip4(XIBECA_PIN5);  // PC9

// Pin led_out1(XIBECA_PIN21);
// Pin led_out2(XIBECA_PIN22);
// Pin led_out3(XIBECA_PIN23);
// Pin led_out4(XIBECA_PIN24);

// static constexpr float MINUS4DB = 0.630957f;
// static constexpr uint16_t ACDC_CLIPPING_LEVEL = (4096*MINUS4DB);
static constexpr uint16_t ACDC_CLIPPING_LEVEL = 4090;

#define ACDC_LED_OFF 1024
void setLed(uint8_t led, uint32_t rgb){
  uint32_t pwm = ACDC_LED_OFF - (__USAT(rgb>>2, 10)); // expects 12-bit parameter value
  switch(led){
  case 1:
    if(rgb == RED_COLOUR){
      led_clip1.low();
      // TIM3->CCR2 = ACDC_LED_OFF;
    }else{
      led_clip1.high();
      TIM3->CCR2 = pwm;
    }
    break;
  case 2:
    if(rgb == RED_COLOUR){
      led_clip2.low();
      // TIM2->CCR4 = ACDC_LED_OFF;
    }else{
      led_clip2.high();
      TIM2->CCR4 = pwm;
    }      
    break;
  case 3:
    if(rgb == RED_COLOUR){
      led_clip3.low();
      // TIM2->CCR1 = ACDC_LED_OFF;
    }else{
      led_clip3.high();
      TIM2->CCR1 = pwm;
    }
    break;
  case 4:
    if(rgb == RED_COLOUR){
      led_clip4.low();
      // TIM2->CCR2 = ACDC_LED_OFF;
    }else{
      led_clip4.high();
      TIM2->CCR2 = pwm;
    }
    break;
  case 5:
    TIM8->CCR1 = pwm;
    break;
  case 6:
    TIM8->CCR2 = pwm;
    break;
  case 7:
    TIM4->CCR1 = pwm;
    break;
  case 8:
    TIM4->CCR2 = pwm;
    break;
  }
}

void updateParameters(int16_t* parameter_values, size_t parameter_len, uint16_t* adc_values, size_t adc_len){
  parameter_values[0] = (parameter_values[0]*3 + adc_values[ADC_A])>>2;
  parameter_values[1] = (parameter_values[1]*3 + adc_values[ADC_B])>>2;
  parameter_values[2] = (parameter_values[2]*3 + adc_values[ADC_C])>>2;
  parameter_values[3] = (parameter_values[3]*3 + adc_values[ADC_D])>>2;
}

void initLed(){
  extern TIM_HandleTypeDef htim2;
  extern TIM_HandleTypeDef htim3;
  extern TIM_HandleTypeDef htim4;
  extern TIM_HandleTypeDef htim8;
  HAL_TIM_Base_Start(&htim2);
  HAL_TIM_Base_Start(&htim3);
  HAL_TIM_Base_Start(&htim4);
  HAL_TIM_Base_Start(&htim8);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1); // in3
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2); // in4
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4); // in2
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2); // in1
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1); // out3
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2); // out4
  HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1); // out1
  HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_2); // out2

  led_clip1.outputMode();
  led_clip2.outputMode();
  led_clip3.outputMode();
  led_clip4.outputMode();
}

#if 0
void initFlash(){
#define QSPIHandle hqspi
  extern QSPI_HandleTypeDef QSPIHandle;
  QSPIHandle.Instance = QUADSPI;
  /* QSPI clock = 480MHz / (1+9) = 48MHz */
  /* QSPI clock = 480MHz / (1+4) = 96MHz */
  QSPIHandle.Init.ClockPrescaler     = 9;
  /* QSPIHandle.Init.ClockPrescaler     = 4; // 4 and 5 don't work? 8 works  */
/* #define IS_QSPI_FIFO_THRESHOLD(THR)        (((THR) > 0U) && ((THR) <= 32U)) */
  QSPIHandle.Init.FifoThreshold      = 4;
  QSPIHandle.Init.SampleShifting     = QSPI_SAMPLE_SHIFTING_NONE;
  QSPIHandle.Init.FlashSize          = 22; // 2^(22+1) = 8M / 64Mbit
  QSPIHandle.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_1_CYCLE;
  QSPIHandle.Init.ClockMode          = QSPI_CLOCK_MODE_0;

  // QSPIHandle.Init.FlashID = QSPI_FLASH_ID_1;
  // QSPIHandle.Init.DualFlash = QSPI_DUALFLASH_DISABLE;

  /* Initialize QuadSPI ------------------------------------------------ */
  HAL_QSPI_DeInit(&QSPIHandle);
  if (HAL_QSPI_Init(&QSPIHandle) != HAL_OK)
    {
      Error_Handler();
    }
  if( HAL_QSPI_SetFifoThreshold(&QSPIHandle, 16) != HAL_OK)
    {
      Error_Handler();
    }
  // flash_init(&QSPIHandle);
}
#endif

void onSetup(){
  // codec.setHighPass(true);
  initLed();
  for(size_t i=1; i<=8; ++i)
    setLed(i, NO_COLOUR);
  for(size_t i=1; i<=8; ++i){
    for(size_t c=0; c<2048; c+=40){
      setLed(i, c);
      HAL_Delay(1);
    }
  }
  for(size_t i=1; i<=8; ++i){
    HAL_Delay(60);
    setLed(i, NO_COLOUR);
  }
  // codec.setHighPass(false);
  led_in1.outputMode();
  led_in1.afMode();
}

#ifdef USE_USBD_AUDIO_TX_PRE_FX
/* Input LEDs while the host records (TX stream active).
 *
 * The knobs then affect neither the recording (TX pre-FX) nor the outputs
 * (RX post-FX). The input LEDs show the raw input level instead of the patch
 * value and turn red for 300 ms after a sample reached -0.1 dBFS. Without TX
 * stream they show the patch value (PARAMETER_AA) as before.
 *
 * Measured every 2 ms (MAIN_LOOP_SLEEP_MS) on the codec DMA input buffer, both
 * halves (2 x AUDIO_BLOCK_SIZE frames, about 1/6 of the signal). Level: mean
 * square x 4096 like the default patch, smoothed with tau = 40 ms.
 *
 * Memory layout: on this hardware the position of code and data in memory
 * shifts the block synchronous noise lines (fs/8, USB SOF comb, mixing
 * products) by up to 2 and 4.5 dB, even without any functional change. So all
 * new code lives in .late_text (placed after everything else in flash) and its
 * state in .late_bss (top of RAM_D2), see Hardware/xibeca.ld. onLoop() is a
 * jump plus padding of exactly the original size, so every other symbol keeps
 * the address it has without this feature. */
static constexpr uint32_t ACDC_RAW_CLIP_HOLD_MS = 300;
static constexpr float ACDC_RAW_SMOOTHING = 0.951f;      // exp(-2 ms / 40 ms)
static constexpr int32_t ACDC_RAW_CLIP_LEVEL = 8293000;  // -0.1 dBFS of 24 bit full scale
static constexpr uint32_t ACDC_RAW_MAGIC = 0x4c45445aU;  // .late_bss is not zeroed at boot
extern "C" {
  extern int32_t codec_rxbuf[];
  extern uint16_t codec_blocksize;
}
// set while the host records (ProgramManager.cpp, USB TX start/stop callbacks)
extern CircularBuffer<audio_t>* volatile usbd_tx;

struct AcdcRawLedState {
  uint32_t magic;
  float levels[4];
  uint32_t clip_tick[4];
  uint8_t clip_hold[4];
  uint8_t was_raw;
};
static AcdcRawLedState acdc_raw __attribute__((section(".late_bss")));

__attribute__((noinline, section(".late_text")))
static uint8_t acdcMeasureRawInputs(){
  // SAI1 delivers 24 bit right-aligned without sign extension (SAI_DATASIZE_24)
  const size_t frames = 2 * codec_blocksize;
  const int32_t* src = codec_rxbuf;
  constexpr float mul = 1.0f/0x00800000;
  float sum[4] = {};
  uint8_t clip = 0;
  for(size_t i=0; i<frames; ++i){
    for(size_t ch=0; ch<4; ++ch){
      int32_t s = (int32_t)((uint32_t)*src++ << 8) >> 8;
      if(s >= ACDC_RAW_CLIP_LEVEL || s <= -ACDC_RAW_CLIP_LEVEL)
        clip |= 1<<ch;
      float f = s * mul;
      sum[ch] += f * f;
    }
  }
  for(size_t ch=0; ch<4; ++ch)
    acdc_raw.levels[ch] = acdc_raw.levels[ch] * ACDC_RAW_SMOOTHING + (sum[ch] / frames) * (1 - ACDC_RAW_SMOOTHING);
  return clip;
}

__attribute__((noinline, used, section(".late_text")))
static void acdcOnLoopLate(void){
  const bool raw = usbd_tx != NULL;
  if(acdc_raw.magic != ACDC_RAW_MAGIC || (raw && !acdc_raw.was_raw)){
    memset(&acdc_raw, 0, sizeof(acdc_raw));
    acdc_raw.magic = ACDC_RAW_MAGIC;
  }
  uint8_t clip = raw ? acdcMeasureRawInputs() : 0;
  acdc_raw.was_raw = raw;
  const uint32_t now = HAL_GetTick();
  for(size_t i=0; i<4; ++i){
    if(raw){
      if(clip & (1<<i)){
        acdc_raw.clip_tick[i] = now;
        acdc_raw.clip_hold[i] = 1;
      }else if(acdc_raw.clip_hold[i] && now - acdc_raw.clip_tick[i] >= ACDC_RAW_CLIP_HOLD_MS){
        acdc_raw.clip_hold[i] = 0;
      }
      if(acdc_raw.clip_hold[i]){
        setLed(i+1, RED_COLOUR);
      }else{
        float level = acdc_raw.levels[i]*4096;
        setLed(i+1, level < 4095.0f ? (uint32_t)level : 4095);
      }
    }else{
      acdc_raw.clip_hold[i] = 0;
      int16_t value = getParameterValue(PARAMETER_AA+i);
      setLed(i+1, value);
      if(value >= ACDC_CLIPPING_LEVEL)
        setLed(i+1, RED_COLOUR);
    }
    extern float tx_levels[4];
    setLed(i+5, tx_levels[i]*4096);
    setParameterValue(PARAMETER_BA+i, tx_levels[i]*4096);
  }
}

void onLoop(void){
  acdcOnLoopLate();
}

/* Pads the input section .text.onLoop to the size of the original onLoop()
 * (0x1d0 B) so that all following code keeps its address. Never called. */
#define ACDC_ONLOOP_PAD 0x1cc
#define ACDC_STR2(x) #x
#define ACDC_STR(x) ACDC_STR2(x)
extern "C" __attribute__((used, naked, noinline, section(".text.onLoop"))) void acdc_onloop_pad(void){
  __asm__ volatile(".space " ACDC_STR(ACDC_ONLOOP_PAD));
}
#else
void onLoop(void){  
  for(size_t i=0; i<4; ++i){
    int16_t value = getParameterValue(PARAMETER_AA+i);
    setLed(i+1, value);
    if(value >= ACDC_CLIPPING_LEVEL)
      setLed(i+1, RED_COLOUR);
    extern float tx_levels[4];
    setLed(i+5, tx_levels[i]*4096);
    setParameterValue(PARAMETER_BA+i, tx_levels[i]*4096);
  }
}
#endif

#ifdef USE_FAST_POW_RESOURCES
uint32_t fast_log_table_size = 0;
uint32_t fast_pow_table_size = 0;
float fast_log_table[16384] __attribute__ ((section (".d2data")));
uint32_t fast_pow_table[2048] __attribute__ ((section (".d2data")));
void onResourceUpdate(){
  Resource* res = storage.getResourceByName(SYSTEM_TABLE_LOG ".bin");
  if(res && res->isValid()){
    fast_log_table_size = std::min(res->getDataSize()/sizeof(float), 16384U);
    storage.readResource(res->getHeader(), fast_log_table, 0, fast_log_table_size*sizeof(float));
  }else{
    fast_log_table_size = 0;
  }
  res = storage.getResourceByName(SYSTEM_TABLE_POW ".bin");
  if(res && res->isValid()){
    fast_pow_table_size = std::min(res->getDataSize()/sizeof(uint32_t), 2048U);
    storage.readResource(res->getHeader(), fast_pow_table, 0, fast_pow_table_size*sizeof(uint32_t));
  }else{
    fast_pow_table_size = 0;
  }
  // debugMessage("log/pow", fast_log_table_size, fast_pow_table_size);
}
#else
#include "FastLogTable.h"
#include "FastPowTable.h"
#endif
