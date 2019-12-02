#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

#if defined(JUCE_LINUX) && defined(__arm__)
#define RASPBERRY_PI 1
extern "C" {
#include <wiringPi.h>
#include <wiringSerial.h>
#include <mcp23008.h>
}
#endif

#include "DSP/DelayLine.h"

// switch gpio mapped to wiringPi
#define SWITCH1 3
#define SWITCH2 4

static constexpr float PI = 3.14159265;

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

    float overdrive(float sample, float blend, float vol);
    float distortion(float sample, float drive, float blend, float tone, float vol);

private:
    void updateFXParam();

    void changeListenerCallback(ChangeBroadcaster*) override;
    static String getListOfActiveBits(const BigInteger& b);
    void timerCallback() override;
    void dumpDeviceInfo();
    void logMessage(const String& m);

private:
    AudioDeviceSelectorComponent audioSetupComp; // for allowing choice of device

    // DSP stuff
    DelayLine delayLine;

    // effect parameters
    // eventually effects will be their own class this is just for a quick prototype
    float odBlend;
    float odVol;
    float distDrive;
    float distBlend;
    float distTone;
    float distVol;

    // serial stuff
    int serialPort;
    char serialData;

    // diagnostic information
    Label cpuUsageLabel;
    Label cpuUsageText;
    TextEditor diagnosticsBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
