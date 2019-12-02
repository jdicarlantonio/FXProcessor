#include <stdio.h>

#include "MainComponent.h"

MainComponent::MainComponent()
: audioSetupComp (
    deviceManager,
    0,
    256,
    0,
    256,
    false,
    false,
    false,
    false
  )
, odBlend(0.5f)
, odVol(1.0f)
, distDrive(0.5f)
, distBlend(0.5f)
, distTone(900.0f)
, distVol(1.0f)
{
    // set up gui
    addAndMakeVisible(audioSetupComp);
    addAndMakeVisible(diagnosticsBox);

    diagnosticsBox.setMultiLine(true);
    diagnosticsBox.setReturnKeyStartsNewLine(true);
    diagnosticsBox.setReadOnly(true);
    diagnosticsBox.setScrollbarsShown(true);
    diagnosticsBox.setCaretVisible(false);
    diagnosticsBox.setPopupMenuEnabled(true);
    diagnosticsBox.setColour(TextEditor::backgroundColourId, Colour (0x32ffffff));
    diagnosticsBox.setColour(TextEditor::outlineColourId,    Colour (0x1c000000));
    diagnosticsBox.setColour(TextEditor::shadowColourId,     Colour (0x16000000));

    cpuUsageLabel.setText("CPU Usage", dontSendNotification);
    cpuUsageText.setJustificationType(Justification::right);
    addAndMakeVisible(&cpuUsageLabel);
    addAndMakeVisible(&cpuUsageText);

    setSize(760, 360);

    // audio device initialization
    setAudioChannels(2, 2);
    deviceManager.addChangeListener(this);

    // setup raspberry pi GPIO
    wiringPiSetup();
    pinMode(SWITCH1, INPUT);
    pullUpDnControl(SWITCH1, PUD_UP);
    pinMode(SWITCH2, INPUT);
    pullUpDnControl(SWITCH2, PUD_UP);
   
    // set up serial communication
    if((serialPort = serialOpen("/dev/serial0", 9600)) < 0)
    {
        DBG("Error opening serial port\n");
    }

    startTimer(50);
}

MainComponent::~MainComponent()
{
    deviceManager.removeChangeListener(this);
    shutdownAudio();
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    delayLine.updateParameters(
        386.0f,
        50.0f,
        35.0f,
        sampleRate
    );
    delayLine.prepareBuffer(sampleRate);

    for(int i = 0; i < NUM_BANDS; ++i)
    {
        eqFilter[i].reset();
    }
}

static constexpr float onethird = 1.0f / 3.0f;
static constexpr float twothird = 2.0f / 3.0f;

float MainComponent::overdrive(float sample, float blend, float vol)
{
    float outSample = sample;
    
    if(sample >= 0.0f && sample < onethird) 
    {
        outSample *= 2.0f; 
    }

    if(sample >= onethird && sample < twothird)
    {
        outSample = 3.0f - powf((2.0f - (3.0f * sample)), 2.0f);
        outSample /= 3.0f;
    }

    if(sample >= twothird && sample <= 1.0f)
    {
        outSample = 1.0f;
    }

    outSample = (blend * outSample + (1 - blend) * sample) * vol;

    return outSample;
}

//static constexpr float PI = 3.14159;

float MainComponent::distortion(float sample, float drive, float blend, float tone, float vol)
{
    float outSample;
    float temp = sample;

    temp *= drive * tone;

    outSample = (((2.0f / PI ) * atan(temp) * blend) + (sample * (1.0f - blend))) * vol;
    
    return outSample;
}

// only gonna do mono for now
#define channel 0

void MainComponent::getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill)
{
    // get current device
    auto* device = deviceManager.getCurrentAudioDevice();

    auto activeInputChannels = device->getActiveInputChannels();
    auto activeOutputChannels = device->getActiveOutputChannels();

    auto maxInputChannels = activeInputChannels.countNumberOfSetBits();
    auto maxOutputChannels = activeOutputChannels.countNumberOfSetBits();

    if(serialDataAvail(serialPort))
    {
        updateFXParam(); 
    }

    for(int i = 0; i < NUM_BANDS; ++i)
    {
        eqFilter[i].calculateCoefficients(
            device->getCurrentSampleRate(),
            1500.0f,
            -4.0f,
            2.0f
        );
    }

    if((!activeOutputChannels[0]) || maxInputChannels == 0) 
    {
        bufferToFill.buffer->clear(0, bufferToFill.startSample, bufferToFill.numSamples);
    }
    else
    {
        if(!activeInputChannels[0])
        {
            bufferToFill.buffer->clear(0, bufferToFill.startSample, bufferToFill.numSamples);
        }
        else
        {
            auto* audioData = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
            
            if(digitalRead(SWITCH1) || digitalRead(SWITCH2))
            {
                for(auto sample = 0; sample < bufferToFill.numSamples; ++sample)
                {
                    if(digitalRead(SWITCH1) == HIGH)
                    { 
                        audioData[sample] = overdrive(audioData[sample], odBlend, odVol);
                    }
                    if(digitalRead(SWITCH2) == HIGH)
                    {
                        audioData[sample] = distortion(audioData[sample], distDrive, distBlend, distTone, distVol);
                    }
                }
            }

//            delayLine.process(audioData, bufferToFill.numSamples);

            for(int i = 0; i < NUM_BANDS; ++i)
            {
                eqFilter[i].process(audioData, bufferToFill.numSamples);
            }
        }
    }

    /* 
     * This is still here for reference

    // handle processing and what not
    for(auto channel = 0; channel < maxOutputChannels; ++channel)
    {
        if((!activeOutputChannels[channel]) || maxInputChannels == 0) 
        {
            bufferToFill.buffer->clear(channel, bufferToFill.startSample, bufferToFill.numSamples);
        }
        else
        {
            // in case there is more output channels than input
            auto actualInputChannel = channel % maxInputChannels;

            if(!activeInputChannels[channel])
            {
                bufferToFill.buffer->clear(channel, bufferToFill.startSample, bufferToFill.numSamples);
            }
            else
            {
                auto* input = bufferToFill.buffer->getWritePointer(actualInputChannel, bufferToFill.startSample);
                auto* output = bufferToFill.buffer->getWritePointer(channel, bufferToFill.startSample);

                if(digitalRead(SWITCH1) || digitalRead(SWITCH2))
                {
                    for(auto sample = 0; sample < bufferToFill.numSamples; ++sample)
                    {
                        if(digitalRead(SWITCH1) == HIGH)
                        { 
                            output[sample] = overdrive(input[sample], odBlend, odVol);
                        }
                        if(digitalRead(SWITCH2) == HIGH)
                        {
                            output[sample] = distortion(input[sample], distDrive, distBlend, distTone, distVol);
                        }
                    }
                }
                
                delayLine.process(output, bufferToFill.numSamples);
            }
        }
    }
    */
}

void MainComponent::updateFXParam()
{
    serialData = serialGetchar(serialPort);

    // debugging
//    printf("%c", serialData);
//    fflush(stdout);

    // overdrive params
    switch(serialData)
    {
        case 'q':
        {
            odVol += 0.25f;
            printf("odVol: %.4f\n", odVol);
            fflush(stdout);
            break;
        }
        case 'a':
        {
            odVol -= 0.25f;
            printf("odVol: %.4f\n", odVol);
            fflush(stdout);
            break;
        }
        case 'w':
        {
            odBlend += 0.125;
            printf("odBlend: %.4f\n", odBlend);
            fflush(stdout);
            break;
        }
        case 's':
        {
            odBlend -= 0.125;
            printf("odBlend: %.4f\n", odBlend);
            fflush(stdout);
            break;
        }
    }

    // distortion params
    switch(serialData)
    {
        case 'o':
        {
            distVol += 0.25f;
            printf("distVol: %.4f\n", distVol);
            fflush(stdout);
            break;
        }
        case 'l':
        {
            distVol -= 0.25f;
            printf("distVol: %.4f\n", distVol);
            fflush(stdout);
            break;
        }
        case 'i':
        {
            distBlend += 0.125;
            printf("distBlend: %.4f\n", distBlend);
            fflush(stdout);
            break;
        }
        case 'k':
        {
            distBlend -= 0.125;
            printf("distBlend: %.4f\n", distBlend);
            fflush(stdout);
            break;
        }
        case 'u':
        {
            distTone += 25.0;
            printf("distTone: %.4f\n", distTone);
            fflush(stdout);
            break;
        }
        case 'j':
        {
            distTone -= 25.0;
            printf("distTone: %.4f\n", distTone);
            fflush(stdout);
            break;
        }
        case 'y':
        {
            distDrive += 0.125;
            printf("distDrive: %.4f\n", distDrive);
            fflush(stdout);
            break;
        }
        case 'h':
        {
            distDrive -= 0.125;
            printf("distDrive: %.4f\n", distDrive);
            fflush(stdout);
            break;
        }
    }
    
}

void MainComponent::releaseResources()
{
}

//==============================================================================
void MainComponent::paint (Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    g.fillRect(getLocalBounds().removeFromRight(proportionOfWidth(0.4f)));
}

void MainComponent::resized()
{
    auto rect = getLocalBounds();

    audioSetupComp.setBounds(rect.removeFromLeft(proportionOfWidth(0.6f)));
    rect.reduce(10, 10);

    auto topLine(rect.removeFromTop(20));
    cpuUsageLabel.setBounds(topLine.removeFromLeft(topLine.getWidth() / 2));
    cpuUsageText.setBounds(topLine);
    rect.removeFromTop(20);

    diagnosticsBox.setBounds(rect);
}

void MainComponent::changeListenerCallback(ChangeBroadcaster*)
{
    // device changed; show new info
    dumpDeviceInfo();
}

String MainComponent::getListOfActiveBits(const BigInteger& b)
{
    StringArray bits;

    for(auto i = 0; i <= b.getHighestBit(); ++i)
    {
        if(b[i])
        {
            bits.add(String(i));
        }
    }

    return bits.joinIntoString(", ");
}

void MainComponent::timerCallback()
{
    auto cpu = deviceManager.getCpuUsage() * 100;
    cpuUsageText.setText(String(cpu, 6) + " %", dontSendNotification);
}

void MainComponent::dumpDeviceInfo()
{
    logMessage("------------------------------------------");
    logMessage(
        "Current audio device type: " 
        + (deviceManager.getCurrentDeviceTypeObject() != nullptr
        ? deviceManager.getCurrentDeviceTypeObject()->getTypeName()
        : "<none>")
    );

    if(auto* device = deviceManager.getCurrentAudioDevice())
    {
        logMessage("Current audio device: " + device->getName().quoted());
        logMessage("Sample rate: " + String(device->getCurrentSampleRate()) + " Hz");
        logMessage("Block size: " + String(device->getCurrentBufferSizeSamples()) + " samples");
        logMessage("Bit depth: " + String(device->getCurrentBitDepth()));
        logMessage("Input channel names: " + device->getInputChannelNames().joinIntoString(", "));
        logMessage("Active input channels: " + getListOfActiveBits(device->getActiveInputChannels()));
        logMessage("Output channel names: " + device->getOutputChannelNames().joinIntoString(", "));
        logMessage("Active output channels: " + getListOfActiveBits(device->getActiveOutputChannels()));
    }
    else
    {
        logMessage("No audio device open");
    }
}

void MainComponent::logMessage(const String& m)
{
    diagnosticsBox.moveCaretToEnd();
    diagnosticsBox.insertTextAtCaret(m + newLine);
}
