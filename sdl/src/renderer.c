#include "util.c"
#include "wayland.c"

typedef enum {
	RM_REGULAR,
	RM_SCREENSAVER,
	RM_BACKGROUND,
} RenderMode;

typedef enum {
	PS_PAUSED,
	PS_STEP,
	PS_RUNNING,
} PlayState;

typedef struct {
	SDL_GPUDevice*           device;
	SDL_Window*              window;
	SDL_GPUComputePipeline*  computePipeline;
	SDL_GPUGraphicsPipeline* graphicsPipeline;
	SDL_GPUBuffer*           buffers[2];
	double                   zoom;
	size_t                   cellsWidth;
	size_t                   cellsHeight;
	size_t                   cellsCount;
	size_t                   cellsSize;
	size_t                   current;
	RenderMode               rm;
	PlayState                ps;
} Renderer;

extern const SDL_GPUShaderFormat shaderFormat;

void rendererInit(Renderer* r);
void rendererReset(Renderer* r);
void rendererRun(Renderer* r);

#if __INCLUDE_LEVEL__ == 0 /////////////////////////////////////////////////////

#include <stdio.h>

const SDL_GPUShaderFormat shaderFormat = SDL_GPU_SHADERFORMAT_SPIRV;

static const unsigned char computeCode[] = {
#embed "life.comp.spv"
};

static const unsigned char vertexCode[] = {
#embed "life.vert.spv"
};

static const unsigned char fragmentCode[] = {
#embed "life.frag.spv"
};

#define loadShader(device, _stage, _code, ...)   \
	_loadShader(                                 \
		device, (SDL_GPUShaderCreateInfo) {      \
					.code       = _code,         \
					.code_size  = sizeof(_code), \
					.entrypoint = "main",        \
					.format     = shaderFormat,  \
					.stage      = _stage,        \
					__VA_ARGS__                  \
				}                                \
	)

static SDL_GPUShader* _loadShader(SDL_GPUDevice* device, SDL_GPUShaderCreateInfo info) {
	SDL_GPUShader* shader = SDL_CreateGPUShader(device, &info);
	if(!shader) dieSDL("failed to create shader");
	return shader;
}

void rendererInit(Renderer* r) {
	SDL_ReleaseGPUComputePipeline(r->device, r->computePipeline);
	SDL_ReleaseGPUGraphicsPipeline(r->device, r->graphicsPipeline);
	for(size_t i = 0; i < SDL_arraysize(r->buffers); i++) {
		SDL_ReleaseGPUBuffer(r->device, r->buffers[i]);
	}

	int width, height;
	SDL_GetWindowSize(r->window, (int*) &width, (int*) &height);

	r->cellsWidth  = width / r->zoom;
	r->cellsHeight = height / r->zoom;
	r->cellsCount  = r->cellsWidth * r->cellsHeight;
	r->cellsSize   = r->cellsCount * sizeof(uint32_t);

	SDL_Log(
		"%s: window %d·%d at zoom %f = cells %zu·%zu",
		__func__, width, height, r->zoom, r->cellsWidth, r->cellsHeight
	);

	if(!(r->computePipeline = SDL_CreateGPUComputePipeline(
			 r->device,
			 &(SDL_GPUComputePipelineCreateInfo) {
				 .code                          = computeCode,
				 .code_size                     = sizeof(computeCode),
				 .entrypoint                    = "main",
				 .format                        = shaderFormat,
				 .num_readonly_storage_buffers  = 1,
				 .num_readwrite_storage_buffers = 1,
				 .num_uniform_buffers           = 1,
			 }
		 ))) dieSDL("failed to create compute pipeline");

	SDL_GPUShader* vertexShader = loadShader(
		r->device, SDL_GPU_SHADERSTAGE_VERTEX, vertexCode,
	);
	if(!vertexShader) dieSDL("failed to load vertex shader");

	SDL_GPUShader* fragmentShader = loadShader(
		r->device, SDL_GPU_SHADERSTAGE_VERTEX, fragmentCode,
		.num_storage_buffers = 1, .num_uniform_buffers = 1
	);
	if(!fragmentShader) dieSDL("failed to load fragment shader");

	if(!(r->graphicsPipeline = SDL_CreateGPUGraphicsPipeline(
			 r->device,
			 &(SDL_GPUGraphicsPipelineCreateInfo) {
				 .target_info = {
					 .num_color_targets         = 1,
					 .color_target_descriptions = (SDL_GPUColorTargetDescription[]) {{
						 .format = SDL_GetGPUSwapchainTextureFormat(r->device, r->window),
					 }},
				 },

				 .primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP,
				 .vertex_shader   = vertexShader,
				 .fragment_shader = fragmentShader,
			 }
		 ))) dieSDL("failed to create graphics pipeline");

	SDL_ReleaseGPUShader(r->device, vertexShader);
	SDL_ReleaseGPUShader(r->device, fragmentShader);

	for(size_t i = 0; i < SDL_arraysize(r->buffers); i++) {
		if(!(r->buffers[i] = SDL_CreateGPUBuffer(
				 r->device,
				 &(SDL_GPUBufferCreateInfo) {
					 .size  = r->cellsSize,
					 .usage =                                    //
					 0                                           //
					 | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ  //
					 | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE //
					 | SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ //
				 }
			 ))) dieSDL("failed to create cell buffer %zu", i);
	}

	rendererReset(r);
}

void rendererReset(Renderer* r) {
	SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(
		r->device,
		&(SDL_GPUTransferBufferCreateInfo) {
			.size  = r->cellsSize,
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
		}
	);
	if(!transferBuffer) dieSDL("failed to create transfer buffer");

	uint32_t* initial = SDL_MapGPUTransferBuffer(r->device, transferBuffer, false);
	if(!initial) dieSDL("failed to map transfer buffer");

	for(size_t i = 0; i < r->cellsCount; i++) {
		initial[i] = SDL_randf() < 0.05 ? 1 : 0;
	}

	SDL_UnmapGPUTransferBuffer(r->device, transferBuffer);

	SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(r->device);
	if(!cmd) dieSDL("failed to acquire command buffer");

	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmd);
	if(!copyPass) dieSDL("failed to begin copy pass");

	SDL_UploadToGPUBuffer(
		copyPass,
		&(SDL_GPUTransferBufferLocation) {
			.transfer_buffer = transferBuffer,
		},
		&(SDL_GPUBufferRegion) {
			.buffer = r->buffers[r->current = 0],
			.size   = r->cellsSize,
		},
		false
	);

	SDL_EndGPUCopyPass(copyPass);

	if(!SDL_SubmitGPUCommandBuffer(cmd)) dieSDL("failed to execute command buffer");
}

void rendererRun(Renderer* r) {
	SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(r->device);
	if(!cmd) dieSDL("failed to acquire command buffer");

	struct {
		uint32_t w, h;
		double   z;
	} params = {r->cellsWidth, r->cellsHeight, r->zoom};
	SDL_PushGPUFragmentUniformData(cmd, 0, &params, sizeof(params));

	SDL_GPUTexture* swapchainTexture = NULL;
	if(!SDL_WaitAndAcquireGPUSwapchainTexture(
		   cmd, r->window, &swapchainTexture, NULL, NULL
	   )) dieSDL("failed to acquire swapchain texture");

	if(swapchainTexture != NULL) {
		static size_t init = 0;
		if(r->ps != PS_PAUSED && init++ > 10) {
			SDL_GPUComputePass* computePass = SDL_BeginGPUComputePass(
				cmd, NULL, 0,
				(SDL_GPUStorageBufferReadWriteBinding[]) {{
					.buffer = r->buffers[1 - r->current],
				}},
				1
			);

			SDL_BindGPUComputePipeline(computePass, r->computePipeline);
			SDL_BindGPUComputeStorageBuffers(computePass, 0, &r->buffers[r->current], 1);
			SDL_DispatchGPUCompute(computePass, r->cellsWidth / 8, r->cellsHeight / 8, 1);
			SDL_EndGPUComputePass(computePass);

			r->current = 1 - r->current;
		}

		SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(
			cmd,
			(SDL_GPUColorTargetInfo[]) {{
				.texture  = swapchainTexture,
				.load_op  = SDL_GPU_LOADOP_CLEAR,
				.store_op = SDL_GPU_STOREOP_STORE,
			}},
			1, NULL
		);
		if(!renderPass) dieSDL("failed to begin render pass");

		SDL_BindGPUGraphicsPipeline(renderPass, r->graphicsPipeline);
		SDL_BindGPUFragmentStorageBuffers(renderPass, 0, &r->buffers[r->current], 1);
		SDL_DrawGPUPrimitives(renderPass, 4, 1, 0, 0);
		SDL_EndGPURenderPass(renderPass);
	}

	if(!SDL_SubmitGPUCommandBuffer(cmd)) dieSDL("failed to execute command buffer");

	if(r->ps == PS_STEP) r->ps = PS_PAUSED;
}

#endif
