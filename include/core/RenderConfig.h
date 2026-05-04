#pragma once

struct MomentShadow {
	float bias = 1.0f;
	float scale = 0.999f;
	float bleedReduction = 0.0f;
	float visibilityScale = 1.0f;
	bool useDirectScaledMoments = false;
	bool enable = false;
	int blurRadius = 2;
	float depthBias = 0.0f;
	float normalBias = 0.0f;
};

struct IBLConfig {
	int numSamples = 40;
	bool enable = true;
	bool diffuseOnly = false;
	bool displayIrrN = false;
	bool enableMIS = true;
	float intensity = 1.0f;
};

struct SSAOConfig {
	float radius = 1.0f;
	int numSamples = 15;
	float bias = 0.001f;
	float scale = 1.0f;
	float contrast = 1.0f;
	bool enable = false;
	bool enableBlur = false;
	int blurRadius = 2;
	float blurVariance = 0.01f;
};

struct SSRConfig {
	bool enable = false;
	int maxSteps = 64;
	float stepSize = 0.1f;
	float thickness = 0.5f;
	int binarySearchSteps = 8;
};

struct SSDOConfig {
	float radius = 0.01f;
	int numSamples = 35;
	float bias = 0.001f;
	float strength = 1.0f;
	float contrast = 1.0f;
	float indirectRadius = 0.1f;
	float distanceClamp = 0.25f;
	bool enable = false;
	bool enableIndirect = false;
	int blurRadius = 5;
	int indirectBlurRadius = 5;
	float blurVariance = 0.01f;
	float lod = 8.5f;
	int doMethod = 0;
};
