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

float4 main(float2 texC : TEXCOORD, float4 pos : SV_Position) : SV_Target0
{
  uint2 pixPos = (uint2)pos.xy;
  float4 worldPos = gWorldPosTex[pixPos];

  // Compute previous pixel position using the current world position
  // https://docs.google.com/presentation/d/1YkDE7YAqoffC9wUmDxFo9WZjiLqWI5SlQRojOeCBPGs/edit#slide=id.g2492ec6f45_0_342
  float4 prevViewPos = mul(worldPos, gPrevViewProjMatrix); // the order due to row-major  https://stackoverflow.com/questions/16578765/hlsl-mul-variables-clarification
  float4 prevScreenPos = prevViewPos / prevViewPos.w;
  int2 prevPixPos = int2 (
    (prevScreenPos.x + 1.f) / 2.f * gTexDim.x,
    (1.f - prevScreenPos.y) / 2.f * gTexDim.y
    );

  bool backProjIsValid = isBackProjectionValid(prevPixPos);

  if (!backProjIsValid) {
    return float4(1.f, 1.f, 0.f, 1.f);
  }

  return gRawColorTex[pixPos];
}
