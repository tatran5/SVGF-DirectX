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

// This class takes as input a shader buffer to accumulate from.  This buffer is copied
//     into an internal resource and every frame is updated to accumulate the current
//     frame with prior frames, the the accumulated frame is output back to overwrite the
//     input.  On camera motion (and UI changes in other passes), this accumulated result
//     is cleared.

#pragma once
#include "../SharedUtils/RenderPass.h"
#include "../SharedUtils/FullscreenLaunch.h"

class SVGFPass : public ::RenderPass, inherit_shared_from_this<::RenderPass, SVGFPass>
{
public:
    using SharedPtr = std::shared_ptr<SVGFPass>;

	static SharedPtr create(const std::string &bufferToAccumulate);
	virtual ~SVGFPass() = default;

protected:
	SVGFPass(const std::string &bufferToAccumulate);

    // Implementation of SimpleRenderPass interface
	bool initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager) override;
	void initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene) override;
    void execute(RenderContext* pRenderContext) override;
    void renderGui(Gui* pGui) override;
    void resize(uint32_t width, uint32_t height) override;
	void stateRefreshed() override;

	// The RenderPass class defines various methods we can override to specify this pass' properties. 
	bool appliesPostprocess() override { return true; }

    // Information about the input rendering texture we're using
	std::string                   mInputColorBufferName;

	// Shaders
	FullscreenLaunch::SharedPtr   mpShaderIntegrateColor;
	FullscreenLaunch::SharedPtr   mpShaderVarianceEstimation;
	FullscreenLaunch::SharedPtr   mpShaderVarianceFiltering;
	FullscreenLaunch::SharedPtr   mpShaderFinalColor;

	GraphicsState::SharedPtr      mpGfxState;
	Texture::SharedPtr            mpLastFrame;

	// Internal frame buffers
	Fbo::SharedPtr                mpInternalFboPrevIntColor;
	Fbo::SharedPtr                mpInternalFboPrevIntMoments;
	Fbo::SharedPtr                mpInternalFboHistoryLength;

	// How many frames have we accumulated so far?
	uint32_t                      mAccumCount = 0;
};
