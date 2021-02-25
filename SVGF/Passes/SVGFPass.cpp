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

// Some global vars, used to simplify changing shader location & entry points
namespace {
	// Name of shader
	const char* kTemporalPlusVarianceShader = "Tutorial11\\SVGFTemporalPlusVariance.ps.hlsl";

	// Names of input buffers
	const char* kInputBufferWorldPosition = "WorldPosition";
	const char* kInputBufferWorldNormal = "World Normal";

	// Names of internal buffers
	const char* kInternalBufferMoment = "Moment"; // history moment
	const char* kInternalBufferColor = "Integrated Color"; // history color
	const char* kInternalBufferHistoryLength = "Length"; // history length
};

SVGFPass::SVGFPass(const std::string& outputTexName, const std::string& rawColorTexName)
	: ::RenderPass("SVGF Pass", "SVGF Options")
{
	mOutputTexName = outputTexName;
	mRawColorTexName = rawColorTexName;
}

SVGFPass::SharedPtr SVGFPass::create(const std::string& outputTexName, const std::string& rawColorTexName)
{
	return SharedPtr(new SVGFPass(outputTexName, rawColorTexName));
}


bool SVGFPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	// Stash a copy of our resource manager so we can get rendering resources
	mpResManager = pResManager;

	mpResManager->requestTextureResources({
		kInputBufferWorldPosition,
		kInputBufferWorldNormal
	});

	mpResManager->requestTextureResources({
		kInternalBufferMoment,
		kInternalBufferColor,
		kInternalBufferHistoryLength
	});

	// Create our graphics state and an accumulation shader
	mpGfxState = GraphicsState::create();
	mpTemporalPlusVarianceShader = FullscreenLaunch::create(kTemporalPlusVarianceShader);
	return true;
}

void SVGFPass::resize(uint32_t width, uint32_t height) {
	// We need a framebuffer to attach to our graphics pipe state (when running our full-screen pass).  We can ask our
	//    resource manager to create one for us, with specified width, height, and format and one color buffer.
	mpInternalFbo = ResourceManager::createFbo(width, height, ResourceFormat::RGBA32Float);
}

void SVGFPass::execute(RenderContext* pRenderContext)
{
	// Grab the input textures
	Texture::SharedPtr pRawColorTex = mpResManager->getTexture(mRawColorTexName);
	Texture::SharedPtr pWorldPositionTex = mpResManager->getTexture(kInputBufferWorldPosition);
	Texture::SharedPtr pWorldNormalTex = mpResManager->getTexture(kInputBufferWorldNormal);

	// Grab the output texture
	Texture::SharedPtr pOutputTex = mpResManager->getTexture(mOutputTexName);

	// Set shader parameters for this pass
	auto shaderVars = mpTemporalPlusVarianceShader->getVars();
	shaderVars["gRawColor"] = pRawColorTex;
	shaderVars["gOutput"] = pOutputTex;

	// Execute this shader
	mpGfxState->setFbo(mpInternalFbo);
	mpTemporalPlusVarianceShader->execute(pRenderContext, mpGfxState);

	// We've accumulated our result.  Copy that back to the input/output buffer
	//pRenderContext->blit(mpInternalFbo->getColorTexture(0)->getSRV(), pOutputTex->getRTV());
}


