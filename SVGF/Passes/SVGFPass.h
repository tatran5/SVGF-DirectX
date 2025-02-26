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
#include "../SharedUtils/RenderPass.h"
#include "../SharedUtils/SimpleVars.h"
#include "../SharedUtils/FullscreenLaunch.h"

class SVGFPass : public ::RenderPass, inherit_shared_from_this<::RenderPass, SVGFPass>
{
public:
	using SharedPtr = std::shared_ptr<SVGFPass>;
	using SharedConstPtr = std::shared_ptr<const SVGFPass>;

	static SharedPtr create(const std::string& outputTexName, const std::string& rawColorTexName);
	virtual ~SVGFPass() = default;

protected:
	SVGFPass(const std::string& outputTexName, const std::string& rawColorTexName);

	// Implementation of SimpleRenderPass interface
	bool initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager) override;
	void initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene) override;
	void initFBO();

	void execute(RenderContext* pRenderContext) override;
	void executeTemporalPlusVariance(
		RenderContext* pRenderContext,
		Texture::SharedPtr pRawColorTex,
		Texture::SharedPtr pWorldPosTex,
		Texture::SharedPtr pWorldNormTex);
	void executeATrous(
		RenderContext* pRenderContext, 
		Texture::SharedPtr pWorldNormTex,
		Texture::SharedPtr pOutputTex);

	void renderGui(Gui* pGui) override;
	void resize(uint32_t width, uint32_t height) override;
	void stateRefreshed() override;

	// Override some functions that provide information to the RenderPipeline class
	bool appliesPostprocess() override { return true; }
	bool hasAnimation() override { return false; }

	// A helper utility to determine if the current scene (if any) has had any camera motion
	bool hasCameraMoved();

	// Information about the rendering textures
	std::string mRawColorTexName;
	std::string mOutputTexName;
	uint2				mTexDim;

	// State for our accumulation shader
	FullscreenLaunch::SharedPtr   mpTemporalPlusVarianceShader;
	FullscreenLaunch::SharedPtr   mpATrousShader;
	GraphicsState::SharedPtr      mpGfxState;
	Texture::SharedPtr            mpLastFrame;
	Fbo::SharedPtr                mpInternalFbo;

	// Internal FBOs

	// Fbos for temporal plus variance (TPV) shader; has textures: prevIntegratedColor, moment, variance
	Fbo::SharedPtr								mpTPVFbo;
	Fbo::SharedPtr								mpPrevTPVFbo;
	Fbo::SharedPtr								mpATrousFbo[2];

	// We stash a copy of our current scene.  Why?  To detect if changes have occurred.
	Scene::SharedPtr              mpScene;
	mat4                          mpPrevViewProjMatrix;

	// Is our accumulation enabled?
	bool                          mDoAccumulation = true;
	
	// GUI fields (also used as input variables in the pass
	bool mDoSVGF = true;
	int mATrousIteration = 1;
	float mATrousSigmaZ = 1; // tunes the weight for depth 
	float mATrousSigmaN = 128; // tunes the weight for normal
	float mATrousSigmaL = 4; // tunes the weight for luminance

	// How many frames have we accumulated so far?
	uint32_t                      mAccumCount = 0;
};
