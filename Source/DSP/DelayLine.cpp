#include "DelayLine.h"

#include <algorithm>

DelayLine::DelayLine()
: delaySamples		{0}
, delayMs			{0}
, feedback			{0}
, feedbackPct		{0}
, wetAmt			{0}
, wetAmtPct			{0}
, bufferSize		{0}
, readIndex			{0}
, writeIndex		{0}
, currentSampleRate {0}
, feedbackAccess	{false}
{
}

DelayLine::~DelayLine()
{
}

void DelayLine::updateParameters(
	float delayAmt, 
	float feedbackAmt, 
	float wetLevel, 
	float sampleRate)
{
	if(delayAmt != delayMs)
	{
		delayMs = delayAmt;
	}
	if(feedbackAmt != feedbackPct)
	{
		feedbackPct = feedbackAmt;
	}
	if(wetLevel != wetAmtPct)
	{
		wetAmtPct = wetLevel;
	}

	cookVariables(sampleRate);
}

void DelayLine::resetDelay()
{
	if(!buffer.hasBeenCleared())
	{
		buffer.clear();
	}

	writeIndex = readIndex = 0;
}

void DelayLine::prepareBuffer(float sampleRate)
{
	if(currentSampleRate != sampleRate)
	{
		currentSampleRate = sampleRate;
		bufferSize = 2 * sampleRate;


		buffer.setSize(1, bufferSize);
	}
	
	resetDelay();
	cookVariables(sampleRate);
}

void DelayLine::process(float* audioBuffer, float numSamples)
{
	float xn, yn;

	for(int i = 0; i < numSamples; ++i)
	{
		xn = audioBuffer[i];
		yn = buffer.getSample(0, readIndex);

		if(readIndex == writeIndex && delaySamples < 1.0f)
		{
			yn = xn;
		}

		int readIndex_1 = readIndex - 1;
		if(readIndex_1 < 0)
		{
			readIndex_1 = bufferSize - 1;
		}

		float yn_1 = buffer.getSample(0, readIndex_1);

		float fractionalDelay = delaySamples - static_cast<int>(delaySamples);
		float interp = linterp({0, yn}, {1, yn_1}, fractionalDelay);

		if(delaySamples == 0)
		{
			yn = xn;
		}
		else 
		{
			yn = interp;
		}
		
		if(!feedbackAccess)
		{
			buffer.setSample(0, writeIndex, xn + feedback * yn);
		}
		else
		{
			buffer.setSample(0, writeIndex, xn + feedbackIn * yn);
		}

		audioBuffer[i] = wetAmt * yn + (1.0f - wetAmt) * xn;

		writeIndex++;
		if(writeIndex >= bufferSize)
		{
			writeIndex = 0;
		}

		readIndex++;
		if(readIndex >= bufferSize)
		{
			readIndex = 0;
		}
	}
}

void DelayLine::cookVariables(float sampleRate)
{
	feedback = feedbackPct / 100.0f;
	wetAmt = wetAmtPct / 100.0f;
	delaySamples = delayMs * (sampleRate / 1000.0f);

	readIndex = writeIndex - static_cast<int>(delaySamples);
	if(readIndex < 0)
	{
		readIndex += bufferSize;
	}
}

float DelayLine::getFeedbackOut() const
{
	return feedback * buffer.getSample(0, readIndex);
}

void DelayLine::setFeedback(float feedbackValue)
{
	if(feedbackAccess)
	{
		feedbackIn = feedbackValue;
	}
}

void DelayLine::setFeedbackAccessible(bool accessible)
{
	feedbackAccess = accessible;
}

float DelayLine::linterp(std::array<float, 2> dataPoint1, std::array<float, 2> dataPoint2, float distance)
{
	float denom = dataPoint2[0] - dataPoint1[0];
	if(denom == 0)
	{
		return dataPoint1[1];
	}

	float fractionalPos = (distance - dataPoint1[0]) / denom;
	float interp = fractionalPos * dataPoint2[1] + (1 - fractionalPos) * dataPoint1[1];

	return interp;
}
