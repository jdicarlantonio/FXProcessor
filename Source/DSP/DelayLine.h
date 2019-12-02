#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

#include <array>

class DelayLine
{
public:
	DelayLine();
	~DelayLine();

	void updateParameters(
		float delayAmt, 
		float feedbackAmt, 
		float wetLevel,
		float sampleRate);

	void resetDelay();
	void prepareBuffer(float sampleRate);
	void process(float* audioBuffer, float numSamples);
	
	void cookVariables(float sampleRate);

	float getFeedbackOut() const;
	void setFeedback(float feedbackValue);
	void setFeedbackAccessible(bool accessible);

private:
	float linterp(std::array<float, 2> dataPoint1, std::array<float, 2> dataPoint2, float distance);
	
private:
	bool feedbackAccess;
	float feedbackIn;

	float delayMs;
	float feedbackPct;
	float wetAmtPct;	

	float delaySamples;
	float feedback;
	float wetAmt;

	AudioSampleBuffer buffer;
	int bufferSize;

	int writeIndex;
	int readIndex;

	int currentSampleRate;
};
