#include "BiQuad.h"

#include <cmath>

BiQuad::BiQuad()
: type{FilterType::PEAK}
, xn_1{0}
, xn_2{0}
, yn_1{0}
, yn_2{0}
{}

BiQuad::BiQuad(FilterType ftype)
: type{ftype}
, xn_1{0}
, xn_2{0}
, yn_1{0}
, yn_2{0}
{}

BiQuad::~BiQuad()
{}

/*
 * Yet another implementation from the RBJ Audio EQ Cookbook
 * http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
 */

void BiQuad::calculateCoefficients(float fs, float f0, float gain, float Q)
{
	float omega = (2.0f * PI * f0) / fs;

	float sinomega = std::sin(omega);
	float cosomega = std::cos(omega);

	switch(type)
	{
		case FilterType::PEAK:
		{
			float amplitude = powf(10.0f, gain / 40.0f);
			float peakQ = Q * amplitude;
			float alpha = sinomega / (2.0f * peakQ);
		
			// recipricoal of a0 coefficient to minimize division
			float a0R = 1 / (1 + alpha/amplitude);

			b0 = (1 + alpha * amplitude) * a0R;
			b1 = ((-2) * cosomega) * a0R;
			b2 = (1 - alpha * amplitude) * a0R;
			a1 = b1;
			a2 = (1 - alpha/amplitude) * a0R;
			

			break;
		}
		case FilterType::LOW_PASS:
		{
			float alpha = sinomega / (2.0f * Q);

			float a0R = 1 / (1 + alpha);

			b0 = ((1 - cosomega) / 2) * a0R;
			b1 = (1 - cosomega) * a0R;
			b2 = b0;
			a1 = (-2 * cosomega) * a0R;
			a2 = (1 - alpha) * a0R;

			break;	
		}	
		case FilterType::HIGH_PASS:
		{
			float alpha = sinomega / (2.0f * Q);
	
			float a0R = 1 / (1 + alpha);

			b0 = ((1 + cosomega) / 2) * a0R;
			b1 = (-1 - cosomega) * a0R;
			b2 = b0;
			a1 = (-2 * cosomega) * a0R;
			a2 = (1 - alpha) * a0R;

			break;	
		}	
	}
}

void BiQuad::process(float* buffer, float numSamples)
{
	float xn, yn;

	for(int i = 0; i < numSamples; ++i)
	{
		xn = buffer[i];
		yn = b0*xn + b1*xn_1 + b2*xn_2 - a1*yn_1 - a2*yn_2;

		buffer[i] = yn;

		xn_2 = xn_1;
		xn_1 = xn;
		yn_2 = yn_1;
		yn_1 = yn;	
	}
}

float BiQuad::process(float sampleData)
{
	float yn, xn = sampleData;

	yn = b0*xn + b1*xn_1 + b2*xn_2 - a1*yn_1 - a2*yn_2;

	xn_2 = xn_1;
	xn_1 = xn;
	yn_2 = yn_1;
	yn_1 = yn;

	return yn;
}
