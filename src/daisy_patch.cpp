#include "daisy_patch.h"

using namespace daisy;

// Hardware Definitions
#define PIN_ENC_CLICK 0
#define PIN_ENC_B 11
#define PIN_ENC_A 12
#define PIN_OLED_DC 9
#define PIN_OLED_RESET 30
#define PIN_MIDI_OUT 13
#define PIN_MIDI_IN 14
#define PIN_GATE_OUT 17
#define PIN_GATE_IN_1 20
#define PIN_GATE_IN_2 19
#define PIN_SAI_SCK_A 28
#define PIN_SAI2_FS_A 27
#define PIN_SAI2_SD_A 26
#define PIN_SAI2_SD_B 25
#define PIN_SAI2_MCLK 24

#define PIN_AK4556_RESET 29

#define ADC_CHN_CTRL_1 DSY_ADC_PIN_CHN10
#define ADC_CHN_CTRL_2 DSY_ADC_PIN_CHN15
#define ADC_CHN_CTRL_3 DSY_ADC_PIN_CHN4
#define ADC_CHN_CTRL_4 DSY_ADC_PIN_CHN7

const float kAudioSampleRate = DSY_AUDIO_SAMPLE_RATE;

void DaisyPatch::DaisyPatch::Init()
{
    // Configure Seed first
    seed.Configure();
    block_size_ = 48;
    InitAudio();
    seed.Init();
    InitDisplay();
    InitCvOutputs();
    InitEncoder();
    InitGates();
    InitDisplay();
    InitMidi();
    InitControls();
    // Reset AK4556
    dsy_gpio_write(&ak4556_reset_pin_, 0);
    DelayMs(10);
    dsy_gpio_write(&ak4556_reset_pin_, 1);
}

void DaisyPatch::DelayMs(size_t del)
{
    dsy_system_delay(del);
}
void DaisyPatch::SetAudioBlockSize(size_t size)
{
    block_size_ = size;
    dsy_audio_set_blocksize(DSY_AUDIO_INTERNAL, block_size_);
    dsy_audio_set_blocksize(DSY_AUDIO_EXTERNAL, block_size_);
}
void DaisyPatch::StartAudio(dsy_audio_callback cb)
{
    dsy_audio_set_callback(DSY_AUDIO_INTERNAL, cb);
    dsy_audio_start(DSY_AUDIO_INTERNAL);
}

void DaisyPatch::ChangeAudioCallback(dsy_audio_callback cb)
{
    dsy_audio_set_callback(DSY_AUDIO_INTERNAL, cb);
}
void DaisyPatch::StartAdc()
{
    dsy_adc_start();
}
float DaisyPatch::AudioSampleRate()
{
    return kAudioSampleRate;
}
size_t DaisyPatch::AudioBlockSize()
{
    return block_size_;
}
float DaisyPatch::AudioCallbackRate()
{
    return kAudioSampleRate / block_size_;
}
void DaisyPatch::UpdateAnalogControls()
{
    for(size_t i = 0; i < CTRL_LAST; i++)
    {
        controls[i].Process();
    }
}
float DaisyPatch::GetCtrlValue(Ctrl k)
{
    return (controls[k].Value());
}

void DaisyPatch::DebounceControls()
{
    encoder.Debounce();
}

// Private Function Implementations
// set SAI2 stuff -- run this between seed configure and init
void DaisyPatch::InitAudio()
{
    seed.sai_handle.init                   = DSY_AUDIO_INIT_BOTH;
    seed.sai_handle.device[DSY_SAI_2]      = DSY_AUDIO_DEVICE_AK4556;
    seed.sai_handle.samplerate[DSY_SAI_2]  = DSY_AUDIO_SAMPLERATE_48K;
    seed.sai_handle.bitdepth[DSY_SAI_2]    = DSY_AUDIO_BITDEPTH_24;
    seed.sai_handle.a_direction[DSY_SAI_2] = DSY_AUDIO_TX;
    seed.sai_handle.b_direction[DSY_SAI_2] = DSY_AUDIO_RX;
    seed.sai_handle.sync_config[DSY_SAI_2] = DSY_AUDIO_SYNC_MASTER;

    ak4556_reset_pin_.pin  = seed.GetPin(PIN_AK4556_RESET);
    ak4556_reset_pin_.mode = DSY_GPIO_MODE_OUTPUT_PP;
    ak4556_reset_pin_.pull = DSY_GPIO_NOPULL;
    dsy_gpio_init(&ak4556_reset_pin_);

    dsy_audio_set_blocksize(DSY_AUDIO_INTERNAL, block_size_);
    dsy_audio_set_blocksize(DSY_AUDIO_EXTERNAL, block_size_);
}
void DaisyPatch::InitControls()
{
    // Set order of ADCs based on CHANNEL NUMBER
    uint8_t channel_order[CTRL_LAST] = {
        ADC_CHN_CTRL_1,
        ADC_CHN_CTRL_2,
        ADC_CHN_CTRL_3,
        ADC_CHN_CTRL_4,
    };
    // NUMBER OF CHANNELS
    seed.adc_handle.channels = CTRL_LAST;
    // Fill the ADCs active channel array.
    for(uint8_t i = 0; i < CTRL_LAST; i++)
    {
        seed.adc_handle.active_channels[i] = channel_order[i];
    }
    // Set Oversampling to 32x
    seed.adc_handle.oversampling = DSY_ADC_OVS_32;
    // Init ADC
    dsy_adc_init(&seed.adc_handle);

    for(size_t i = 0; i < CTRL_LAST; i++)
    {
        controls[i].Init(dsy_adc_get_rawptr(i), AudioCallbackRate(), true);
    }
}

void DaisyPatch::InitDisplay()
{
    dsy_gpio_pin pincfg[OledDisplay::NUM_PINS];
    pincfg[OledDisplay::DATA_COMMAND] = seed.GetPin(PIN_OLED_DC);
    pincfg[OledDisplay::RESET]        = seed.GetPin(PIN_OLED_RESET);
    display.Init(pincfg);
}

void DaisyPatch::InitMidi()
{
    midi.Init(MidiHandler::MidiInputMode::INPUT_MODE_UART1,
              MidiHandler::MidiOutputMode::OUTPUT_MODE_UART1);
}

void DaisyPatch::InitCvOutputs()
{
    dsy_dac_init(&seed.dac_handle, DSY_DAC_CHN_BOTH);
    dsy_dac_write(DSY_DAC_CHN1, 0);
    dsy_dac_write(DSY_DAC_CHN2, 0);
}

void DaisyPatch::InitEncoder()
{
    encoder.Init(seed.GetPin(PIN_ENC_A),
                 seed.GetPin(PIN_ENC_B),
                 seed.GetPin(PIN_ENC_CLICK),
                 AudioCallbackRate());
}

void DaisyPatch::InitGates()
{
    // Gate Output
    gate_output.pin  = seed.GetPin(PIN_GATE_OUT);
    gate_output.mode = DSY_GPIO_MODE_OUTPUT_PP;
    gate_output.pull = DSY_GPIO_NOPULL;
    dsy_gpio_init(&gate_output);

    // Gate Inputs
    dsy_gpio_pin pin;
    pin = seed.GetPin(PIN_GATE_IN_1);
    gate_input[GATE_IN_1].Init(&pin);
    pin = seed.GetPin(PIN_GATE_IN_2);
    gate_input[GATE_IN_2].Init(&pin);
}
