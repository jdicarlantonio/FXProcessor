#pragma once

#include <iostream>

static constexpr double PI = 3.14159;
//static constexpr double ROOT_TWO = 1.41421;

enum FilterType
{
	PEAK, // 0
	LOW_PASS,
	HIGH_PASS
};

class BiQuad
{
public:
	BiQuad();
	BiQuad(FilterType ftype); // 0 Peak, 1 Low pass, 2 High Pass, etc... (see enum)
	~BiQuad();

	void calculateCoefficients(float fs, float f0, float gain = 0.0f, float Q = 2.0f);
	void process(float* buffer, float numSamples);
	float process(float sampleData);

	FilterType getType()
	{
		return type;
	}

	void reset()
	{
		xn_1 = xn_2 = yn_1 = yn_2 = 0;
	}

	void changeType(FilterType ftype)
	{
		type = ftype;
	}

private:
	FilterType type;

	// Filter Coefficients
	float a1, a2;
	float b0, b1, b2;

	// Delay 
	float xn_1, xn_2;
	float yn_1, yn_2;
};
