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
  uint gTexWidth;
  uint gTexHeight;
  uint2 gTexDim;
}

Texture2D<float4>   gRawColorTex; 
Texture2D<float4>   gWorldPosTex;
Texture2D<float4>   gWorldNormTex;

bool isBackProjectionValid(int2 prevPixPos) {
  if (any(prevPixPos < int2(0, 0)) || any(prevPixPos >= gTexDim)) return false;

  // TODO: normal comparison

  // TODO: depth comparison

  return true;
}


void bilinearTapFilter(float sqrtTapSize, float2 prevPixPos, out uint consistentSampleCount, out float prevLuminance, out float prevMoment) {
  // Make sure that the offsets of the following 
  // (we will only use 2x2 and 3x3 offset when this function is called though)
  // 2x2: 0 -> 1,   3x3: -1 -> 1,   4x4: -1 -> 2,   5x5: -2 -> 2,   etc.
  int startOffset = -floor((sqrtTapSize - 1.f) / 2.f);
  int endOffset = floor(sqrtTapSize / 2.f);

  for (int x = startOffset; x < endOffset; x++) {
    for (int y = startOffset; y < endOffset; y++) {

    }
  }
}

float4 main(float2 texC : TEXCOORD, float4 pos : SV_Position) : SV_Target0
{
  uint2 pixPos = (uint2)pos.xy;
  float4 worldPos = gWorldPosTex[pixPos];

  // Compute previous pixel position using the current world position
  // https://docs.google.com/presentation/d/1YkDE7YAqoffC9wUmDxFo9WZjiLqWI5SlQRojOeCBPGs/edit#slide=id.g2492ec6f45_0_342
  float4 prevViewPos = mul(worldPos, gPrevViewProjMatrix); // the order due to row-major  https://stackoverflow.com/questions/16578765/hlsl-mul-variables-clarification
  float4 prevScreenPos = prevViewPos / prevViewPos.w;
  float2 prevPixPos = float2 (
    (prevScreenPos.x + 1.f) / 2.f * gTexDim.x,
    (1.f - prevScreenPos.y) / 2.f * gTexDim.y
    );
  float2 prevPixPosFrac = frac(prevPixPos.xy);

  // Perform bilinear tap filter. If 2x2 fails, then try 3x3
  int consistentSamplesCount = 0;
  bool samplesAreConsistent[4] = {false, false, false, false}; // Track whick back project is valid
  int2 sampleOffsets[4] = {int2(0, 0), int2(1, 0), int2(1, 1), int2(0, 1)};
  for (int sampleIdx = 0; sampleIdx < 4; sampleIdx++) {
    int2 prevSamplePixPos = int2(prevPixPos) + sampleOffsets[sampleIdx];
    if (isBackProjectionValid(prevSamplePixPos)) {
      samplesAreConsistent[sampleIdx] = true;
      consistentSamplesCount++;
    }
  }

  if (consistentSamplesCount > 0) {
    // If there are some consistent samples within the 2x2 bilinear tap filter, 
    // perform bilinear interpolation
    float bilinearWeights[4] = {
      (1 - x) * (1 - y), // (0, 0)
           x * (1 - y), // (1, 0)
           x * y,       // (1, 1)
      (1 - x) * y       // (0, 1)
    };

  } else { 
    // If no consistent sample within the 2x2 bilinear tap filter, 
    // then perform 3x3 filter to find suitable samples elsewhere
    for (int x = -1; x <= 1; x++) {
      for (int y = -1; y <= 1; x++) {
        int2 prevSamplePixPos = int2(prevPixPos)+ int2(x, y);
        bool backProjIsValid = isBackProjectionValid(prevSamplePixPos);
        if (backProjIsValid) {
          consistentSamplesCount++;
        }
      }
    }
  }

  return gRawColorTex[pixPos];
}
