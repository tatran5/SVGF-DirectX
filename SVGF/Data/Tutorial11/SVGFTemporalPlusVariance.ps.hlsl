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
	float4x4 gPrevViewProjMat;
}


Texture2D<float4>   gWorldPos;
Texture2D<float4>		gWorldNorm;

//Texture2D<float4>   gPrevWorldPos;
//Texture2D<float4>		gPrevWorldNorm;

Texture2D<float4>		gRawColor;

Texture2D<float4>		gMoment;
Texture2D<float4>		gIntegratedColor;
Texture2D<float4>		gLength;

RWTexture2D<float4>   gOutput;

bool isBackProjectValid(int2 prevPixPos, uint2 texDim) {
	
	// Invalid if outside screen
	if (any(prevPixPos < int2(1, 1)) || any(prevPixPos >= texDim)) return false;

	// Valid if normals are not too different
	// TODO 
	/*float3 prevNorm = gPrevWorldPos[prevPixPos].xyz;
	if (distance(prevNorm, curNorm) / (fwidthNormal + 1e-2) > 1) return false;*/

	// TODO: depth comparison

	return true;
}

float4 main(float2 texC : TEXCOORD, float4 pos : SV_Position) : SV_Target0
{
	
	uint2 pixPos = (uint2)pos.xy;	
	float4 worldPos = gWorldPos[pixPos];

	int2 texDim = getTextureDims(gRawColor, 0);

	// Find the position of previous pixel coordinate with the same world position
	// https://docs.google.com/presentation/d/1YkDE7YAqoffC9wUmDxFo9WZjiLqWI5SlQRojOeCBPGs/edit#slide=id.g2492ec6f45_0_342
	float4 prevViewSp = mul(gPrevViewProjMat, gWorldPos); // mul(x, y) = x * y where both are matrices
	float4 prevScreenSp = prevViewSpace / prevViewSpace.w;
	int2 prevPixPos = int2(
		(prevScreenSp.x + 1.f) / 2.f * texDim.x,
		(1.f - prevScreenSp.y) / 2.f * texDim.y);

	bool backProjIsValid = isBackProjectValid(prevPixPos, texDim);
	if (backProjIsValid) {
		return gRawColor[pixelPos];
	}

	// gOutput[pixPos] = gRawColor[pixPos];

	//   float4 curColor = gCurFrame[pixelPos];
	//   float4 prevColor = gLastFrame[pixelPos];

	 //return (gAccumCount * prevColor + curColor) / (gAccumCount + 1);
	
	return float4(0.f);

}
