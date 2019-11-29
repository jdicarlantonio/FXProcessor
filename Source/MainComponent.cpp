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
    // so far nothing to do here ...
}

void MainComponent::getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill)
{
    // get current device
    auto* device = deviceManager.getCurrentAudioDevice();

    auto activeInputChannels = device->getActiveInputChannels();
    auto activeOutputChannels = device->getActiveOutputChannels();

    auto maxInputChannels = activeInputChannels.countNumberOfSetBits();
    auto maxOutputChannels = activeOutputChannels.countNumberOfSetBits();

    // handle processing and what not
    for(auto channel = 0; channel < maxOutputChannels; ++channel)
    {
        if((!activeOutputChannels[channel]) || maxInputChannels == 0) 
        {
            bufferToFill.buffer->clear(channel, bufferToFill.startSample, bufferToFill.numSamples);
        }
        else
        {
            auto actualInputChannel = channel % maxInputChannels;

            if(!activeInputChannels[channel])
            {
                bufferToFill.buffer->clear(channel, bufferToFill.startSample, bufferToFill.numSamples);
            }
            else
            {
                auto* input = bufferToFill.buffer->getReadPointer(actualInputChannel, bufferToFill.startSample);
                auto* output = bufferToFill.buffer->getWritePointer(channel, bufferToFill.startSample);

                for(auto sample = 0; sample < bufferToFill.numSamples; ++sample)
                {
                    output[sample] = input[sample];
                }
            }
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
