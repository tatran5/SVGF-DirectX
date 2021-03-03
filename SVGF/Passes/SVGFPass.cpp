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

#include "SVGFPass.h"

namespace {
	const char* kTemporalPlusVarianceShader = "Tutorial11\\SVGFTemporalPlusVariance.ps.hlsl";
	
	// Names of input buffers
	const char* kWorldPos = "WorldPosition";
	const char* kWorldNorm = "WorldNormal";

	// Names of internal buffers
	const char* kInternalPrevIntegratedColor = "PrevIntegratedColor";
	const char* kInternalPrevMoment = "PrevMoment";
};

enum TPVTextureLocation {
	IntegratedColor = 0,
	Moments				  = 1,
	HistoryLength		= 2,
	Variance				= 3
};

SVGFPass::SharedPtr SVGFPass::create(const std::string& outputTexName, const std::string& rawColorTexName) {
	return SharedPtr(new SVGFPass(outputTexName, rawColorTexName));
}

SVGFPass::SVGFPass(const std::string& outputTexName, const std::string& rawColorTexName)
	: ::RenderPass("SVGF Pass", "SVGF Options")
{
	mOutputTexName = outputTexName;
	mRawColorTexName = rawColorTexName;
}

bool SVGFPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	if (!pResManager) return false;

	// Stash our resource manager; ask for the texture the developer asked us to accumulate
	mpResManager = pResManager;
	mpResManager->requestTextureResources({ 
		// Field textures
		mOutputTexName,
		mRawColorTexName,
		// Input buffer textures
		kWorldPos,
		kWorldNorm,
		// Internal buffer textures
		kInternalPrevIntegratedColor,
		kInternalPrevMoment
		});

	// Create our graphics state and accumulation shader
	mpGfxState = GraphicsState::create();
	mpTemporalPlusVarianceShader = FullscreenLaunch::create(kTemporalPlusVarianceShader);

	return true;
}



void SVGFPass::initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene)
{
	// Reset accumulation.
	mAccumCount = 0;

	// When our renderer moves around, we want to reset accumulation
	mpScene = pScene;

	// Grab a copy of the current scene's camera matrix (if it exists)
	if (mpScene && mpScene->getActiveCamera())
		mpPrevViewProjMatrix = mpScene->getActiveCamera()->getViewProjMatrix();

	initFBO();
}

void SVGFPass::initFBO() {
	// Mimicking ResourceManager::createFbo
	Fbo::Desc TPVFboDesc;
	TPVFboDesc.setColorTarget(TPVTextureLocation::IntegratedColor, ResourceFormat::RGBA32Float);
	TPVFboDesc.setColorTarget(TPVTextureLocation::Moments, ResourceFormat::RG32Float);
	TPVFboDesc.setColorTarget(TPVTextureLocation::HistoryLength, ResourceFormat::R32Float);
	TPVFboDesc.setColorTarget(TPVTextureLocation::Variance, ResourceFormat::R32Float);

	mpPrevTPVFbo = FboHelper::create2D(mTexDim.x, mTexDim.y, TPVFboDesc);
	mpTPVFbo = FboHelper::create2D(mTexDim.x, mTexDim.y, TPVFboDesc);
}

void SVGFPass::resize(uint32_t width, uint32_t height)
{
	// Resize internal resources
	mpLastFrame = Texture::create2D(width, height, ResourceFormat::RGBA32Float, 1, 1, nullptr, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess | Resource::BindFlags::RenderTarget);

	// We need a framebuffer to attach to our graphics pipe state (when running our full-screen pass)
	mpInternalFbo = ResourceManager::createFbo(width, height, ResourceFormat::RGBA32Float);
	mpGfxState->setFbo(mpInternalFbo);

	// Whenever we resize, we'd better force accumulation to restart
	mAccumCount = 0;

	mTexDim = ivec2(width, height);
}

void SVGFPass::renderGui(Gui* pGui)
{
	// Print the name of the buffer we're accumulating from and into.  Add a blank line below that for clarity
	pGui->addText((std::string("Accumulating buffer:   ") + mOutputTexName).c_str());
	pGui->addText("");

	// Add a toggle to enable/disable temporal accumulation.  Whenever this toggles, reset the
	//     frame count and tell the pipeline we're part of that our rendering options have changed.
	if (pGui->addCheckBox(mDoAccumulation ? "Accumulating samples temporally" : "No temporal accumulation", mDoAccumulation))
	{
		mAccumCount = 0;
		setRefreshFlag();
	}

	// Display a count of accumulated frames
	pGui->addText("");
	pGui->addText((std::string("Frames accumulated: ") + std::to_string(mAccumCount)).c_str());
}

void SVGFPass::execute(RenderContext* pRenderContext)
{
	// Input textures
	Texture::SharedPtr pRawColorTex = mpResManager->getTexture(mRawColorTexName);
	Texture::SharedPtr pWorldPosTex = mpResManager->getTexture(kWorldPos);
	Texture::SharedPtr pWorldNormTex = mpResManager->getTexture(kWorldNorm);
	Texture::SharedPtr pOutputTex = mpResManager->getTexture(mOutputTexName);

	executeTemporalPlusVariance(pRenderContext, pRawColorTex, pWorldPosTex, pWorldNormTex);

	// We've accumulated our result.  Copy that back to the input/output buffer
	// TEST ONLY
	pRenderContext->blit(mpTPVFbo->getColorTexture(TPVTextureLocation::IntegratedColor)->getSRV(), pOutputTex->getRTV());

	// Update fields to be used in next iteration
	std::swap(mpPrevTPVFbo, mpTPVFbo);
	mpPrevViewProjMatrix = mpScene->getActiveCamera()->getViewProjMatrix();
	
}

void SVGFPass::executeTemporalPlusVariance(RenderContext* pRenderContext,
	Texture::SharedPtr pRawColorTex, Texture::SharedPtr pWorldPosTex,	Texture::SharedPtr pWorldNormTex) 
{
	// Internal textures
	Texture::SharedPtr pPrevIntegratedColor = mpPrevTPVFbo->getColorTexture(TPVTextureLocation::IntegratedColor);
	Texture::SharedPtr pPrevMoment					= mpPrevTPVFbo->getColorTexture(TPVTextureLocation::Moments);
	Texture::SharedPtr pPrevHistoryLength   = mpPrevTPVFbo->getColorTexture(TPVTextureLocation::HistoryLength);

	// Set shader parameters for our accumulation
	auto shaderVars = mpTemporalPlusVarianceShader->getVars();
	
	shaderVars["PerFrameCB"]["gPrevViewProjMatrix"] = mpPrevViewProjMatrix;
	shaderVars["PerFrameCB"]["gTexDim"]							= mTexDim;
	shaderVars["PerFrameCB"]["gAlpha"]							= 0.2f;
	shaderVars["PerFrameCB"]["gAlphaMoments"]				= 0.2f;

	shaderVars["gRawColorTex"]  = pRawColorTex;
	shaderVars["gWorldPosTex"]  = pWorldPosTex;
	shaderVars["gWorldNormTex"] = pWorldNormTex;

	shaderVars["gPrevIntegratedColorTex"] = pPrevIntegratedColor;
	shaderVars["gPrevMoments"] = pPrevMoment;
	shaderVars["gPrevHistoryLength"] = pPrevHistoryLength;

	mpGfxState->setFbo(mpTPVFbo);
	mpTemporalPlusVarianceShader->execute(pRenderContext, mpGfxState);
}

void SVGFPass::stateRefreshed()
{
	// This gets called because another pass else in the pipeline changed state.  Restart accumulation
	mAccumCount = 0;
}
