/**********************************************************************************************************************
# Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
# following conditions are met:
#  * Redistributions of code must retain the copyright notice, this list of conditions and the following disclaimer.
#  * Neither the name of NVIDIA CORPORATION nor the names of its contributors may be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT
# SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
# OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********************************************************************************************************************/

cbuffer PerFrameCB
{
	//uint gATrousDepth;
	uint2 gTexDim;
	uint neighborDist;
}

// Input buffer
Texture2D<float4>   gWorldNormTex;

 // Internal buffers
Texture2D<float>   gVarianceTex;
Texture2D<float4>  gColorTex;

float getLuminance(float3 color) {
	return 0.2126f * color.r + 0.7152f * color.g + 0.0722f * color.b;
}


struct GBuffer
{
	float4 filteredColor : SV_Target0;
	float variance : SV_Target1;
};

// Applies a 3x3 Gaussian filter on variance
// Returns the current pixel's variance value after Gaussian blur
float filterVariance(int2 pixPos) {
	const float kernelGaussian[9] = {
		0.0625,		0.125,		0.0625,
		0.125,		0.25,			0.125,
		0.0625,		0.125,		0.0625
	};

	float weightSum = 0.f;
	float variance = 0.f;

	for (int x = -1; x <= 1; x++) {
		for (int y = -1; y <= 1; y++) {
			int kernelIdx = (y + 1) * 3 + (x + 1);
			int2 neighborPixPos = pixPos + int2(x, y);

			if (neighborPixPos.x >= 0 && neighborPixPos.y >= 0 && neighborPixPos.x < gTexDim.x && neighborPixPos.y < gTexDim.y) {
				float neighborVar = gVarianceTex[neighborPixPos];
				float neighborWeight = kernelGaussian[kernelIdx];
				weightSum += neighborWeight;
				variance += neighborWeight * neighborVar;
			}
		}
	}

	return weightSum > 0.001f ? 1.f / weightSum * variance : variance;
}

GBuffer main(float2 texC : TEXCOORD, float4 pos : SV_Position) : SV_Target0
{
	int2 pixPos = (int2)pos.xy;

	const float kernelATrous[25] = {
		0.0625f,	0.0625f,	0.0625f,	0.0625f,	0.0625f,
		0.0625f,	0.25f,		0.25f,		0.25f,		0.0625f,
		0.0625f,	0.25f,		0.375f,		0.25f,		0.0625f,
		0.0625f,	0.25f,		0.25f,		0.25f,		0.0625f,
		0.0625f,	0.0625f,	0.0625f,	0.0625f,	0.0625f
	};

	uint gATrousDepth = 1;

	float4 normPlusDepth = gWorldNormTex[pixPos];
	float4 color = gColorTex[pixPos];

	// Perform ATrousWavelet filtering
	float4 colorSum = float4(0.f);
	float varianceSum = 0.f;
	float weightSum = 0.f;
	float weightSquaredSum = 0.f;
	float luminance = getLuminance(color.xyz);
	
	float variance = gVarianceTex[pixPos];
	float filteredVariance = filterVariance(pixPos); // 4 is recommened var in paper, add epsilon to avoid division by 0
	float denomWeightL = 4 * sqrt(filteredVariance) + 0.001f;

	for (int x = -2; x <= 2; x++) {
		for (int y = -2; y <= 2; y++) {
			int2 neighborPixPos = neighborDist * int2(x, y) + int2(pixPos);
			int kernelIdx = (y + 2) * 5 + (x + 2);
			float kernelVal = kernelATrous[kernelIdx];

			// Make sure the position of the neighbor is within range
			if (neighborPixPos.x >= 0 && neighborPixPos.x < gTexDim.x && neighborPixPos.y >= 0 && neighborPixPos.y < gTexDim.y) {
				float4 neighborNormPlusDepth = gWorldNormTex[neighborPixPos];
				float4 neighborColor = gColorTex[neighborPixPos];
				float neighborLuminance = getLuminance(neighborColor.xyz);
				float neighborVariance = gVarianceTex[neighborPixPos];

				float weightZ = exp(-abs(normPlusDepth.w - neighborNormPlusDepth.w));

				// 128 is recommended var in paper
				float weightN = pow(max(0, dot(normPlusDepth.xyz, neighborNormPlusDepth.xyz)), 128);

				float weightL = exp(-abs(luminance - neighborLuminance) / denomWeightL); // luminance weight					

				float weight = kernelVal * weightZ * weightN * weightL;
				weightSum += weight;
				//weightSquaredSum = weight * weight;
				varianceSum += weight * weight * neighborVariance;
				colorSum += neighborColor * weight;
			}
		}
	}

	if (weightSum > 0.001f) {
		color = colorSum / weightSum;
		variance = varianceSum / weightSum * weightSum;
	}

	GBuffer gBufOut;
	gBufOut.filteredColor = color;
	gBufOut.variance = variance;

	return gBufOut;
}
