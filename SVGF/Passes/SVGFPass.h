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

#pragma once

#include "../SharedUtils/FullscreenLaunch.h"
#include "../SharedUtils/RenderPass.h"
#include "../SharedUtils/RayLaunch.h"

class SVGFPass : public ::RenderPass, inherit_shared_from_this<::RenderPass, SVGFPass>
{
public:
  using SharedPtr = std::shared_ptr<SVGFPass>;
  using SharedConstPtr = std::shared_ptr<const SVGFPass>;

  static SharedPtr create(const std::string& outputTexName, const std::string& rawColorTexName);
  virtual ~SVGFPass() = default;

protected:
  SVGFPass(const std::string& outputTexName, const std::string& rawColorTexName);

  // Implementation of RenderPass interface
  bool initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager) override;
  void execute(RenderContext* pRenderContext) override;

  // Rendering state
  RayLaunch::SharedPtr                    mpRays;                 ///< Our wrapper around a DX Raytracing pass
  RtScene::SharedPtr                      mpScene;                ///< Our scene file (passed in from app)  

  // State for our shaders
  GraphicsState::SharedPtr      mpGfxState;
  FullscreenLaunch::SharedPtr   mpTemporalPlusVarianceShader;


  // Texture related
  std::string mRawColorTexName;
  std::string mOutputTexName;

// Various internal parameters
  uint32_t                                mFrameCount = 0x1337u;  ///< A frame counter to vary random numbers over time
};
