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
, delayMS(0.0f)
, feedback(0.0f)
, wet(0.0f)
, lowVol(0.0f)
, highVol(0.0f)
, lowFreq(100.0f)
, highFreq(1000.0f)
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
    pinMode(SWITCH3, INPUT);
    pullUpDnControl(SWITCH3, PUD_UP);
    pinMode(SWITCH4, INPUT);
    pullUpDnControl(SWITCH4, PUD_UP);
   
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
        delayMS,
        feedback,
        wet,
        sampleRate
    );
    delayLine.prepareBuffer(sampleRate);

    lowBand.reset();
    highBand.reset();
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
        
        delayLine.updateParameters(
            delayMS,
            feedback,
            wet,
            device->getCurrentSampleRate()
        );

        lowBand.calculateCoefficients(device->getCurrentSampleRate(), lowFreq, lowVol);
        highBand.calculateCoefficients(device->getCurrentSampleRate(), highFreq, highVol);
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

            if(digitalRead(SWITCH3))
            {
                lowBand.process(audioData, bufferToFill.numSamples);
                highBand.process(audioData, bufferToFill.numSamples);
            }
           
            if(digitalRead(SWITCH4))
            {
                delayLine.process(audioData, bufferToFill.numSamples);
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


// this is not final! 
// this is just for a quick prototype to test and show a functioning product
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
   
    // delay param
    switch(serialData)
    {
        case 'e':
        {
            delayMS += 100.0;
            printf("delayMS: %.4f\n", delayMS);
            fflush(stdout);

            break;
        }
        case 'd':
        {
            delayMS -= 100.0;
            printf("delayMS: %.4f\n", delayMS);
            fflush(stdout);
            break;
        }
        case 'r':
        {
            feedback += 1.0;
            printf("feedback: %.4f\n", feedback);
            fflush(stdout);
            break;
        }
        case 'f':
        {
            feedback -= 1.0;
            printf("feedback: %.4f\n", feedback);
            fflush(stdout);
            break;
        }
        case 't':
        {
            wet += 1.0;
            printf("wet: %.4f\n", wet);
            fflush(stdout);
            break;
        }
        case 'g':
        {
            wet -= 1.0;
            printf("wet: %.4f\n", wet);
            fflush(stdout);
            break;
        }
    }

    // eq param
    switch(serialData)
    {
        case 'x':
        {
            lowVol += 0.5;
            printf("lowVol: %.4f\n", lowVol);
            fflush(stdout);
            break;
        }
        case 'z':
        {
            lowVol -= 0.5;
            printf("lowVol: %.4f\n", lowVol);
            fflush(stdout);
            break;
        }
        case 'm':
        {
            highVol += 0.5;
            printf("highVol: %.4f\n", highVol);
            fflush(stdout);
            break;
        }
        case 'n':
        {
            highVol -= 0.5;
            printf("highVol: %.4f\n", highVol);
            fflush(stdout);
            break;
        }
        case '1':
        {
            lowFreq = 100; 
            printf("lowFreq: %.4f\n", lowFreq);
            fflush(stdout);
            break;
        }
        case '2':
        {
            lowFreq = 200; 
            printf("lowFreq: %.4f\n", lowFreq);
            fflush(stdout);
            break;
        }
        case '3':
        {
            lowFreq = 300; 
            printf("lowFreq: %.4f\n", lowFreq);
            fflush(stdout);
            break;
        }
        case '4':
        {
            lowFreq = 400; 
            printf("lowFreq: %.4f\n", lowFreq);
            fflush(stdout);
            break;
        }
        case '7':
        {
            highFreq = 700; 
            printf("highFreq: %.4f\n", highFreq);
            fflush(stdout);
            break;
        }
        case '8':
        {
            highFreq = 800; 
            printf("highFreq: %.4f\n", highFreq);
            fflush(stdout);
            break;
        }
        case '9':
        {
            highFreq = 900; 
            printf("highFreq: %.4f\n", highFreq);
            fflush(stdout);
            break;
        }
        case '0':
        {
            highFreq = 1000; 
            printf("highFreq: %.4f\n", highFreq);
            fflush(stdout);
            break;
        }
        case '-':
        {
            highFreq = 2000; 
            printf("highFreq: %.4f\n", highFreq);
            fflush(stdout);
            break;
        }
        case '=':
        {
            highFreq = 3000; 
            printf("highFreq: %.4f\n", highFreq);
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
