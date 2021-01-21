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
	// Where is our shader located?
	const char* kShaderIntegratedColor = "Tutorial12\\SVGFintegratedColor.ps.hlsl";
	const char* kShaderVarianceEstimation = "Tutorial12\\SVGFvarianceEstimation.ps.hlsl";
	const char* kShaderVarianceFiltering = "Tutorial12\\SVGFvarianceFiltering.ps.hlsl";
	const char* kShaderFinalColor = "Tutorial12\\SVGFfinalColor.ps.hlsl";

	// Names of internal frame buffer
	const char* kInternalBufferPrevIntColor = "PreviousIntegratedColor";
	const char* kInternalBufferPrevIntMoments = "PreviousIntegratedMoments";
	const char* kInternalBufferHistoryLength = "HistoryLength";
	
	// Names of input buffers
	const char* kInputWorldPosition = "WorldPostion";
	const char* kInputWorldNormal = "WorldNormal";
	const char* kInputViewDepth = "ViewDepth";
};

// Input should be color of the current frame
// Define our constructor methods
SVGFPass::SharedPtr SVGFPass::create(const std::string& inputColorBufferName)
{
	return SharedPtr(new SVGFPass(inputColorBufferName));
}

SVGFPass::SVGFPass(const std::string& inputColorBufferName)
	: ::RenderPass("SVGF Pass", "SVGF Options")
{
	mInputColorBufferName = inputColorBufferName;
}

bool SVGFPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	// Stash our resource manager; ask for the texture the developer asked us to accumulate
	mpResManager = pResManager;
	mpResManager->requestTextureResources({
		mInputColorBufferName,
		kInternalBufferPrevIntColor,
		kInternalBufferPrevIntMoments,
		kInternalBufferHistoryLength
		});

	// Set the default scene to load
	mpResManager->setDefaultSceneName("Data/pink_room/pink_room.fscene");

	// Create our graphics state and an accumulation shader
	mpGfxState = GraphicsState::create();
	
	mpShaderIntegrateColor = FullscreenLaunch::create(kShaderIntegratedColor);
	mpShaderVarianceEstimation = FullscreenLaunch::create(kShaderVarianceEstimation);
	mpShaderVarianceFiltering = FullscreenLaunch::create(kShaderVarianceFiltering);
	mpShaderFinalColor = FullscreenLaunch::create(kShaderFinalColor);
	
	return true;
}

void SVGFPass::initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene)
{
	// Reset accumulation on loading a new scene
	mAccumCount = 0;
}

void SVGFPass::resize(uint32_t width, uint32_t height)
{
	// Create / resize a texture to store the previous frame.
//    Parameters: width, height, texture format, texture array size, #mip levels, initialization data, how we expect to use it.
	mpLastFrame = Texture::create2D(width, height, ResourceFormat::RGBA32Float, 1, 1, nullptr, ResourceManager::kDefaultFlags);

	// We need a framebuffer to attach to our graphics pipe state (when running our full-screen pass).  We can ask our
//    resource manager to create one for us, with specified width, height, and format and one color buffer.
	mpInternalFbo = ResourceManager::createFbo(width, height, ResourceFormat::RGBA32Float);
	mpGfxState->setFbo(mpInternalFbo);

	// Whenever we resize, we'd better force accumulation to restart
	mAccumCount = 0;
}

void SVGFPass::renderGui(Gui* pGui)
{
	// Print the name of the buffer we're accumulating from and into.  Add a blank line below that for clarity
	pGui->addText((std::string("Accumulating buffer:   ") + mAccumChannel).c_str());
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
	// Grab the texture to accumulate
	Texture::SharedPtr inputTexture = mpResManager->getTexture(mAccumChannel);

	// If our input texture is invalid, or we've been asked to skip accumulation, do nothing.
	if (!inputTexture || !mDoAccumulation) return;


	// Set shader parameters for our accumulation pass
	auto shaderVars = mpAccumShader->getVars();
	shaderVars["PerFrameCB"]["gAccumCount"] = mAccumCount++;
	shaderVars["gLastFrame"] = mpLastFrame;
	shaderVars["gCurFrame"] = inputTexture;

	// Execute the accumulation shader
	mpAccumShader->execute(pRenderContext, mpGfxState);

	// We've accumulated our result.  Copy that back to the input/output buffer
	pRenderContext->blit(mpInternalFbo->getColorTexture(0)->getSRV(), inputTexture->getRTV());

	// Also keep a copy of the current accumulation for use next frame 
	pRenderContext->blit(mpInternalFbo->getColorTexture(0)->getSRV(), mpLastFrame->getRTV());
}

void SVGFPass::stateRefreshed()
{
	// This gets called because another pass else in the pipeline changed state.  Restart accumulation
	mAccumCount = 0;
}
