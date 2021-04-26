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
	const char* kTemporalPlusVarianceShader = "SVGFTemporalPlusVariance.ps.hlsl";
	const char* kATrousShader = "SVGFATrous.ps.hlsl";
	
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

enum ATrousTextureLocation {
	ATrousColor = 0,
	ATrousVariance = 1
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
	mpATrousShader = FullscreenLaunch::create(kATrousShader);
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

	Fbo::Desc ATrousFBO;
	ATrousFBO.setColorTarget(ATrousTextureLocation::ATrousColor, ResourceFormat::RGBA32Float);
	ATrousFBO.setColorTarget(ATrousTextureLocation::ATrousVariance, ResourceFormat::R32Float);
	mpATrousFbo[0] = FboHelper::create2D(mTexDim.x, mTexDim.y, ATrousFBO);
	mpATrousFbo[1] = FboHelper::create2D(mTexDim.x, mTexDim.y, ATrousFBO);
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
	int dirty = 0;
	dirty |= (int)pGui->addCheckBox(mDoSVGF ? "SVGF is on" : "SVGF is off", mDoSVGF);
	dirty |= (int)pGui->addIntVar("No. of iterations", mATrousIteration, 1, 5, 1);
	dirty |= (int)pGui->addFloatVar("Depth sigma", mATrousSigmaZ, 1, 10, 0.5);
	dirty |= (int)pGui->addFloatVar("Normal sigma", mATrousSigmaN, 1, 150, 1);
	dirty |= (int)pGui->addFloatVar("Luminance sigma", mATrousSigmaL, 1, 100, 0.5);

	if (dirty) setRefreshFlag();
}

void SVGFPass::execute(RenderContext* pRenderContext) {
	// Input textures
	Texture::SharedPtr pRawColorTex = mpResManager->getTexture(mRawColorTexName);
	Texture::SharedPtr pWorldPosTex = mpResManager->getTexture(kWorldPos);
	Texture::SharedPtr pWorldNormTex = mpResManager->getTexture(kWorldNorm);
	Texture::SharedPtr pOutputTex = mpResManager->getTexture(mOutputTexName);

	executeTemporalPlusVariance(pRenderContext, pRawColorTex, pWorldPosTex, pWorldNormTex);
	executeATrous(pRenderContext, pWorldNormTex, pOutputTex);

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

	// Set shader parameters for our calculation of integrated color and variance
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

void SVGFPass::executeATrous(RenderContext* pRenderContext, Texture::SharedPtr pWorldNormTex, Texture::SharedPtr pOutputTex) {
	// Internel textures
	Texture::SharedPtr pTPVIntegratedColor = mpTPVFbo->getColorTexture(TPVTextureLocation::IntegratedColor);
	Texture::SharedPtr pTPVVariance = mpTPVFbo->getColorTexture(TPVTextureLocation::Variance);

	// Ping pong textures
	Texture::SharedPtr pATrousColor[2];
	pATrousColor[0] = mpATrousFbo[0]->getColorTexture(ATrousTextureLocation::ATrousColor);
	pATrousColor[1] = mpATrousFbo[1]->getColorTexture(ATrousTextureLocation::ATrousColor);

	Texture::SharedPtr pATrousVariance[2];
	pATrousVariance[0] = mpATrousFbo[0]->getColorTexture(ATrousTextureLocation::ATrousVariance);
	pATrousVariance[1] = mpATrousFbo[1]->getColorTexture(ATrousTextureLocation::ATrousVariance);

	// Transfer results from previous shader to the first buffer of this shader to automate ping-pong process
	pRenderContext->blit(pTPVIntegratedColor->getSRV(), pATrousColor[0]->getRTV());
	pRenderContext->blit(pTPVVariance->getSRV(), pATrousVariance[0]->getRTV());

	int neighborDist = 1;
	int atrousDepth = 2;
	pATrousColor[0]->captureToFile(0, 0,
			"D:\\Academics\\497\\SVGF-DirectX\\testImages\\SVGFz" + std::to_string(mATrousSigmaZ) +
			"n" + std::to_string(mATrousSigmaN) +
			"l" + std::to_string(mATrousSigmaL) +
			"i" + std::to_string(0) + ".EXR",
			Bitmap::FileFormat::ExrFile);

	for (int i = 0; i < mATrousIteration; i++) {
		// Set shader parameters for our ATrous process
		auto shaderVars = mpATrousShader->getVars();
		shaderVars["PerFrameCB"]["gTexDim"] = mTexDim;
		shaderVars["PerFrameCB"]["gNeighborDist"] = neighborDist;
		shaderVars["PerFrameCB"]["sigmaZ"] = mATrousSigmaZ;
		shaderVars["PerFrameCB"]["sigmaN"] = mATrousSigmaN;
		shaderVars["PerFrameCB"]["sigmaL"] = mATrousSigmaL;
		shaderVars["gColorTex"] = pATrousColor[i % 2];
		shaderVars["gVarianceTex"] = pATrousVariance[i % 2];
		shaderVars["gWorldNormTex"] = pWorldNormTex;

		mpGfxState->setFbo(mpATrousFbo[(i + 1) % 2]);
		mpATrousShader->execute(pRenderContext, mpGfxState);
		
		if (i == 0) {
			// Save the filtered color to be used for next temporal filtering
			pRenderContext->blit(pATrousColor[1]->getSRV(), pTPVIntegratedColor->getRTV());
		}

		neighborDist *= 2;

		// Save result to a texture for testing
		pATrousColor[(i + 1) % 2]->captureToFile(0, 0,
			"D:\\Academics\\497\\SVGF-DirectX\\testImages\\SVGFz" + std::to_string(mATrousSigmaZ) +
				"n" + std::to_string(mATrousSigmaN) + 
				"l" + std::to_string(mATrousSigmaL) +
				"i" + std::to_string(i + 1) + ".EXR",
			Bitmap::FileFormat::ExrFile);
	}

	// Save the final result to output texture
	pRenderContext->blit(pATrousColor[mATrousIteration % 2]->getSRV(), pOutputTex->getRTV());
}


void SVGFPass::stateRefreshed()
{
	// This gets called because another pass else in the pipeline changed state.  Restart accumulation
	mAccumCount = 0;
}
