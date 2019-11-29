#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

class MainComponent   
    : public AudioAppComponent
    , public ChangeListener
    , private Timer
{
public:
    MainComponent();
    ~MainComponent();

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void paint (Graphics& g) override;
    void resized() override;

private:
    void changeListenerCallback(ChangeBroadcaster*) override;
    static String getListOfActiveBits(const BigInteger& b);
    void timerCallback() override;
    void dumpDeviceInfo();
    void logMessage(const String& m);

private:
    AudioDeviceSelectorComponent audioSetupComp; // for allowing choice of device

    // diagnostic information
    Label cpuUsageLabel;
    Label cpuUsageText;
    TextEditor diagnosticsBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
