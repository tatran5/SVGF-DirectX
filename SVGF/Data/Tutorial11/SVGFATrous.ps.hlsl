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
	uint gATrousDepth;
	uint2 gTexDim;
	float gKernelGaussian[3] = {
		0.0625f,	0.0125f,	0.25f
	};
	float gAKernelTrous[25] = {
		0.0625f,	0.0625f,	0.0625f,	0.0625f,	0.0625f,
		0.0625f,	0.25f,		0.25f,		0.25f,		0.0625f,
		0.0625f,	0.25f,		0.375f,		0.25f,		0.0625f,
		0.0625f,	0.25f,		0.25f,		0.25f,		0.0625f,
		0.0625f,	0.0625f,	0.0625f,	0.0625f,	0.0625f
	};
}

Texture2D<float>    gPrevHistoryLength;
Texture2D<float>    gVariance;
Texture2D<float>    gDepth ;

float4 main(float2 texC : TEXCOORD, float4 pos : SV_Position) : SV_Target0
{
	uint2 pixPos = (uint2)pos.xy;

	// ------------------------------------------------------------------
	// Filter variance with 3x3 gaussian blur
	// ------------------------------------------------------------------
	//float varianceFiltered = gKernelVariance[pixPos];
	//float weightSum = 0.f;
	//float varianceSum = 0.f;

	//for (int x = -1; x <= 1; x++) {
	//	for (int y = -1; y <= 1; y++) {
	//		int kernelIdx = -(abs(x) + abs(y)) + 2;
	//		int2 neighborPixPos = (int2)pos.xy + int2(x, y);
	//		if (neighborPixPos.x >= 0 && neighborPixPos.y >= 0 &&
	//			neighborPixPos.x < gTexDim.x && neighborPixPos.y < gTexDim.y) {
	//			float neighborVariance = gKernelVariance[neighborPixPos];
	//			float neighborWeight = gKernelGaussian[kernelIdx];
	//			weightSum += neighborWeight;
	//			varianceSum += neighborWeight * neighborVariance;
	//		}
	//	}
	//}

	//if (weightSum > 0.001f) {
	//	varianceFiltered = varianceSum / weightSum;
	//}

	//// ------------------------------------------------------------------
	//// ATrous wavelet filtering
	//// ------------------------------------------------------------------
	//int distBetweenNeighbors = 1;
	//float clipSpaceDepth = 
	//for (uint level = 0; level < gATrousDepth; level++) {
	//	for (int x = -2; x <= 2; x++) {
	//		for (int y = -2; y <= 2; y++) {
	//			int2 neighborPixPos = distBetweenNeighbors * int2(x, y) + int2(pixPos);
	//			if (neighborPixPos.x >= 0 && neighborPixPos.x < gTexDim.x &&
	//				neighborPixPos.y >= 0 && neighborPixPos.y < gTexDim.y) {

	//				// Find wz
	//			
	//				wz = exp()
	//			}
	//		}
	//	}
	//	distBetweenNeighbors *= 2;
	//}

}
