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
#include "PatchRegistry.h"
#include "ProgramManager.h"
#include "cmsis_os.h"
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

/* Boot menu (see acdcBootSelect() below): while led_hold is set, the boot code
 * owns the LEDs and onLoop() leaves them alone. ACDC_LED_HOLD_BOOT holds until
 * the boot code releases it, any other value counts down once per onLoop()
 * (2 ms). Lives in .late_bss like the other state added by this fork: no other
 * object in RAM moves. .late_bss is not zeroed at boot, onSetup() sets it. */
static constexpr uint16_t ACDC_LED_HOLD_BOOT = 0xffff;
struct AcdcBootState {
  volatile uint16_t led_hold;
};
static AcdcBootState acdc_boot __attribute__((section(".late_bss")));

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
  acdc_boot.led_hold = ACDC_LED_HOLD_BOOT; // released by acdcBootSelect()
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
  if(acdc_boot.led_hold){ // boot menu or slot display owns the LEDs
    if(acdc_boot.led_hold != ACDC_LED_HOLD_BOOT)
      acdc_boot.led_hold = acdc_boot.led_hold - 1;
    return;
  }
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
  if(acdc_boot.led_hold){ // boot menu or slot display owns the LEDs
    if(acdc_boot.led_hold != ACDC_LED_HOLD_BOOT)
      acdc_boot.led_hold = acdc_boot.led_hold - 1;
    return;
  }
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

/* --- Boot menu: choose the patch slot at power-on ---------------------------
 *
 * Power on with knobs 1, 2 and 3 fully left and knob 4 fully right:
 *   knob 1   selects among the occupied slots 1..15. It only counts once it
 *            has been moved, until then the remembered slot stays selected.
 *   knob 4   away from the right stop: confirm. The slot starts and is
 *            remembered for the next power-on.
 *   knob 3   away from the left stop, or 30 s without confirming: cancel.
 *            The remembered slot starts, nothing is written.
 * The input LEDs show the slot number in binary (LED 1 = 1, 2 = 2, 3 = 4,
 * 4 = 8). The output LEDs are lit while the menu is open and blink while
 * confirming would change the slot. Without the knob pattern the remembered
 * slot starts and its number is shown for one second.
 *
 * The slot is remembered in a 4 byte system resource in the patch flash
 * (QSPI). The application settings are not touched. Each change appends 256 B
 * and marks the old record as erased. This code never lets the storage run
 * into a defrag (that erases the whole flash): with less than
 * ACDC_SLOT_RESERVE free bytes the slot starts without being remembered and
 * the input LEDs flash red for a second.
 *
 * Runs in the manager task from bootstrap(), before the first patch starts.
 * The scheduler is cooperative (configUSE_PREEMPTION 0): onSetup() has
 * finished by then, and the main loop (MIDI, watchdog) only runs while this
 * code sleeps in vTaskDelay(). The only code added to the periodic path is the
 * led_hold test in onLoop(). All of this is ordinary flash code, on purpose
 * not in .late_text (which is copied to ITCM). */
#define ACDC_SLOT_RESOURCE "ACDC.slot"
static constexpr uint8_t ACDC_MENU_SLOTS = 15;         // 4 LEDs
static constexpr int32_t ACDC_KNOB_LOW = 205;          // below 5 %: at the left stop
static constexpr int32_t ACDC_KNOB_HIGH = 3890;        // above 95 %: at the right stop
static constexpr int32_t ACDC_KNOB_LEAVE = 205;        // another 5 %: moved away from a stop
static constexpr int32_t ACDC_KNOB_MOVE = 250;         // knob 1 counts as moved
static constexpr uint32_t ACDC_MENU_TIMEOUT_MS = 30000;
static constexpr uint16_t ACDC_SHOW_LOOPS = 1000 / MAIN_LOOP_SLEEP_MS; // slot display, 1 s
static constexpr uint32_t ACDC_LED_ON = 2048;          // like the start-up chaser
static constexpr uint32_t ACDC_LED_DIM = 512;
static constexpr size_t ACDC_SLOT_RESERVE = 64 * 1024; // never write below this much free storage
static constexpr uint8_t ACDC_BOOT_HOST = 0;           // acdcBootSelect(): a host took over

extern uint16_t adc_values[NOF_ADC_VALUES];

static void acdcBootLeds(uint8_t slot, uint32_t out){
  for(size_t i=0; i<4; ++i){
    setLed(i+1, (slot & (1 << i)) ? ACDC_LED_ON : NO_COLOUR);
    setLed(i+5, out);
  }
}

/* @return remembered slot 1..15, 0 without a valid record */
static uint8_t acdcReadSlot(){
  Resource* res = storage.getResourceByName(ACDC_SLOT_RESOURCE);
  uint8_t data[4] = {};
  if(res == NULL || res->getDataSize() != sizeof(data))
    return 0;
  storage.readResource(res->getHeader(), data, 0, sizeof(data));
  if(data[2] != 'S' || data[3] != 'L' || data[1] != (uint8_t)~data[0] ||
     data[0] < 1 || data[0] > ACDC_MENU_SLOTS)
    return 0;
  return data[0];
}

static bool acdcStoreSlot(uint8_t slot){
  if(storage.getFreeSize(RESOURCE_PORT_MAPPED) < ACDC_SLOT_RESERVE)
    return false; // writeResource() would defrag, i.e. erase the whole flash
  uint32_t buffer[(sizeof(ResourceHeader) + 4) / sizeof(uint32_t)] = {};
  uint8_t* data = (uint8_t*)buffer + sizeof(ResourceHeader);
  data[0] = slot;
  data[1] = ~slot;
  data[2] = 'S';
  data[3] = 'L';
  storage.writeResource(ACDC_SLOT_RESOURCE, (uint8_t*)buffer, 4,
                        RESOURCE_SYSTEM_RESOURCE | RESOURCE_PORT_MAPPED);
  registry.init(); // writeResource() rebuilt the resource index the registry points into
  return acdcReadSlot() == slot;
}

/* @return slot to start, or ACDC_BOOT_HOST */
static uint8_t acdcBootMenu(const uint8_t* slots, size_t count, uint8_t stored, int32_t* knob){
  const TickType_t start = xTaskGetTickCount();
  const int32_t entry = knob[0];
  const int32_t width = 4096 / (int32_t)count;
  const int32_t hyst = width / 8 > 16 ? width / 8 : 16;
  bool moved = false;
  int32_t sel = 0;
  for(;;){
    vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_SLEEP_MS));
    for(size_t i=0; i<4; ++i)
      knob[i] += ((int32_t)adc_values[i] - knob[i]) / 8;
    const TickType_t elapsed = xTaskGetTickCount() - start;
    if(owl.getOperationMode() == LOAD_MODE)
      return ACDC_BOOT_HOST; // Program Change or upload from a host: its request is pending
    int32_t zone = knob[0] / width;
    if(zone > (int32_t)count - 1)
      zone = count - 1;
    if(!moved){
      if(knob[0] - entry > ACDC_KNOB_MOVE){
        moved = true;
        sel = zone;
      }
    }else if(zone != sel && (knob[0] < sel * width - hyst || knob[0] >= (sel + 1) * width + hyst)){
      sel = zone;
    }
    const uint8_t shown = moved ? slots[sel] : stored;
    const bool dark = shown != stored && (elapsed & 0x100); // blinks: confirming changes the slot
    acdcBootLeds(shown, dark ? NO_COLOUR : ACDC_LED_DIM);
    if(knob[3] < ACDC_KNOB_HIGH - ACDC_KNOB_LEAVE)
      return shown;  // confirm
    if(knob[2] > ACDC_KNOB_LOW + ACDC_KNOB_LEAVE)
      return stored; // cancel
    if(elapsed >= pdMS_TO_TICKS(ACDC_MENU_TIMEOUT_MS))
      return stored;
  }
}

/* Called once from bootstrap(). Blocks while the boot menu is open.
 * @return slot to start (1..15, DEFAULT_PROGRAM without any patch), or
 * ACDC_BOOT_HOST when a host started something itself while the menu was open */
uint8_t acdcBootSelect(){
  acdc_boot.led_hold = ACDC_LED_HOLD_BOOT;
  uint8_t slots[ACDC_MENU_SLOTS];
  size_t count = 0;
  for(uint8_t slot=1; slot<=ACDC_MENU_SLOTS; ++slot)
    if(registry.hasPatch(slot))
      slots[count++] = slot;
  uint8_t stored = acdcReadSlot();
  if(stored == 0 || !registry.hasPatch(stored))
    stored = (count == 0 || registry.hasPatch(DEFAULT_PROGRAM)) ? DEFAULT_PROGRAM : slots[0];

  // knob positions, mean over 16 ms (the ADC has been running since Owl::setup())
  int32_t knob[4] = {};
  for(size_t n=0; n<8; ++n){
    vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_SLEEP_MS));
    for(size_t i=0; i<4; ++i)
      knob[i] += adc_values[i];
  }
  for(size_t i=0; i<4; ++i)
    knob[i] /= 8;

  uint8_t slot = stored;
  if(count > 0 && knob[0] < ACDC_KNOB_LOW && knob[1] < ACDC_KNOB_LOW &&
     knob[2] < ACDC_KNOB_LOW && knob[3] > ACDC_KNOB_HIGH){
    slot = acdcBootMenu(slots, count, stored, knob);
    if(slot == ACDC_BOOT_HOST){
      acdc_boot.led_hold = 0;
      return ACDC_BOOT_HOST;
    }
    if(slot != stored && !acdcStoreSlot(slot)){
      for(size_t i=0; i<4; ++i){
        setLed(i+1, RED_COLOUR);
        setLed(i+5, NO_COLOUR);
      }
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }
  acdcBootLeds(count > 0 ? slot : 0, NO_COLOUR);
  acdc_boot.led_hold = ACDC_SHOW_LOOPS;
  return slot;
}

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
