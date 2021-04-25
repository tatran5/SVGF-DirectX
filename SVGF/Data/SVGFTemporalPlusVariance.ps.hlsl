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
  uint gAccumCount;
  float4x4 gPrevViewProjMatrix;
  uint  gTexWidth;
  uint  gTexHeight;
  uint2 gTexDim;
  float gAlpha;
  float gAlphaMoments;
}

// Input buffers
Texture2D<float4>   gRawColorTex; 
Texture2D<float4>   gWorldPosTex;
Texture2D<float4>   gWorldNormTex;

// Internal buffers
Texture2D<float4>   gPrevIntegratedColorTex;
Texture2D<float2>   gPrevMoments;
Texture2D<float>    gPrevHistoryLength;

float getLuminance(float3 color) {
  return 0.2126f * color.r + 0.7152f * color.g + 0.0722f * color.b;
}

bool isBackProjectionValid(int2 prevPixPos) {
  if (any(prevPixPos < int2(0, 0)) || any(prevPixPos >= gTexDim)) return false;

  // TODO: normal comparison

  // TODO: depth comparison

  return true;
}

// 2x2 bilinear tap filter (bilinear interpolation)
// For all of the valid previous neighbor samples (including the previous pixel),
// To find the corresponding value, add up the value contribution from those samples
// (with their weights factored in using bilinear interpolation) and redistribute the value
// by dividing the weight sum
bool tapFilter2x2(float2 prevPixPos, out float4 prevIntegratedColor, out float2 prevMoments, out float prevHistoryLength) {
  // Set everything to 0 just in case
  prevIntegratedColor = float4(0.f);
  prevMoments         = float2(0.f);
  
  bool sampleIsConsistent[4] = { false, false, false, false }; // Track whick back project is valid
  int2 sampleOffsets[4] = { int2(0, 0), int2(1, 0), int2(1, 1), int2(0, 1) };
  
  float2 prevPixPosFrac = frac(prevPixPos.xy);
  float weightSum = 0.f;
  float sampleWeights[4] = {
    (1 - prevPixPosFrac.x) * (1 - prevPixPosFrac.y), // (0, 0)
          prevPixPosFrac.x  * (1 - prevPixPosFrac.y), // (1, 0)
          prevPixPosFrac.x  * prevPixPosFrac.y,       // (1, 1)
    (1 - prevPixPosFrac.x) * prevPixPosFrac.y        // (0, 1)
  };
    
  for (int sampleIdx = 0; sampleIdx < 4; sampleIdx++) {
    int2 prevSamplePixPos = int2(prevPixPos) + sampleOffsets[sampleIdx];
    if (isBackProjectionValid(prevSamplePixPos)) {
      prevIntegratedColor += sampleWeights[sampleIdx] * gPrevIntegratedColorTex[prevSamplePixPos];
      prevMoments         += sampleWeights[sampleIdx] * gPrevMoments[prevSamplePixPos];
      prevHistoryLength   += sampleWeights[sampleIdx] * gPrevHistoryLength[prevSamplePixPos];
      
      weightSum += sampleWeights[sampleIdx];
    }
  }

  // Redistribute weights since we might not use all taps
  if (weightSum > 0.01f) { // in case division by 0 and whether previous data is reliable
    prevIntegratedColor /= weightSum;
    prevMoments         /= weightSum;
    prevHistoryLength   /= weightSum;
  }

  return weightSum > 0.01f;
}

// 3x3 uniform tap filter (bilinear interpolation)
// For all of the valid previous neighbor samples (including the previous pixel),
// To find the corresponding value, add up the value contribution from those samples
// and redistribute the value uniformly by dividing the number of samples contributing
bool tapFilter3x3(float2 prevPixPos, out float4 prevIntegratedColor, out float2 prevMoments, out float prevHistoryLength) {
  // Set everything to 0 just in case
  prevIntegratedColor = float4(0.f);
  prevMoments         = float2(0.f);

  float weightSum = 0.f;

  for (int x = -1; x <= 1; x++) {
    for (int y = -1; y <= 1; y++) {
      int2 prevSamplePixPos = int2(prevPixPos) + int2(x, y);
      if (isBackProjectionValid(prevSamplePixPos)) {
        prevIntegratedColor += gPrevIntegratedColorTex[prevSamplePixPos];
        prevMoments         += gPrevMoments[prevSamplePixPos];
        prevHistoryLength += gPrevHistoryLength[prevSamplePixPos];

        weightSum++;
      }
    }
  }
 
  if (weightSum > 0) {
    prevIntegratedColor /= weightSum;
    prevMoments         /= weightSum;
    prevHistoryLength   /= weightSum;
  }

  return weightSum > 0;
}

 

struct GBuffer
{
  // SV_TargetX means the textured is stored in the xth position in our FBO
  // This should corresponds to the FBO setup in the .cpp file, and the FBO
  // set as output of this shader by using setFbo function
  float4 integratedColor   : SV_Target0; 
  float2 integratedMoments : SV_Target1;
  float  historyLength     : SV_Target2;
  float  variance         : SV_Target3;
};


GBuffer main(float2 texC : TEXCOORD, float4 pos : SV_Position)
{
  uint2 pixPos = (uint2)pos.xy;
  float4 rawColor = gRawColorTex[pixPos];
  float4 worldPos = gWorldPosTex[pixPos];

  float4 integratedColor   = float4(0.f);
  float2 integratedMoments = float2(0.f);
  float2 rawMoments      = float2(0.f);
  float  historyLength   = float(0.f);
  float  variance = float(0.f);
  float  alpha = gAlpha;
  float  alphaMoments = gAlphaMoments;

  // Compute previous pixel position using the current world position
  // https://docs.google.com/presentation/d/1YkDE7YAqoffC9wUmDxFo9WZjiLqWI5SlQRojOeCBPGs/edit#slide=id.g2492ec6f45_0_342
  float4 prevViewPos = mul(worldPos, gPrevViewProjMatrix); // the order due to row-major  https://stackoverflow.com/questions/16578765/hlsl-mul-variables-clarification
  float4 prevScreenPos = prevViewPos / prevViewPos.w;
  float2 prevPixPos = float2 (
    (prevScreenPos.x + 1.f) / 2.f * gTexDim.x,
    (1.f - prevScreenPos.y) / 2.f * gTexDim.y
    );

  // Perform filter. If 2x2 fails, then try 3x3
  float4 prevIntegratedColor = 0.f;
  float2 prevMoments = 0.f;
  float  prevHistoryLength = 0.f;

  // Apply tap linear to get the weighted integrated color and  moment from previous frames
  if (!tapFilter2x2(prevPixPos, prevIntegratedColor, prevMoments, prevHistoryLength) && 
      !tapFilter3x3(prevPixPos, prevIntegratedColor, prevMoments, prevHistoryLength)) {
    historyLength = 1.f;
    alpha         = 1.f;
    alphaMoments  = 1.f;
  }
  else {
    historyLength = min(32.f, prevHistoryLength + 1.f);
    alpha         = max(gAlpha, 1.f / historyLength);
    alphaMoments  = max(gAlphaMoments, 1.f / historyLength);
  }
  
  float luminance = getLuminance(rawColor.xyz);
  rawMoments = float2(luminance, luminance * luminance);

  integratedColor   = lerp(prevIntegratedColor, rawColor, alpha);
  integratedMoments = lerp(prevMoments, rawMoments, alphaMoments);
  variance          = max(0.f, integratedMoments.x - integratedMoments.y * integratedMoments.y);
  
  // Dump out our G buffer channels
  // bogus values just for testing
  GBuffer gBufOut;
  gBufOut.integratedColor   = integratedColor;
  gBufOut.integratedMoments = integratedMoments;
  gBufOut.historyLength     = historyLength;
  gBufOut.variance          = variance;

  return gBufOut;
}
