#include "Application.h"
#include <algorithm>
#include <iostream>
#include <limits>

#include "raylib.h"
#include "rlgl.h"
#include "glad.h"
#include <bit>

#include "GPU/GPUVector.h"

using namespace Mupfel;


constexpr uint32_t BLOCK_THREADS = 256;
constexpr uint32_t ELEMENTS_PER_THREAD = 2;
constexpr uint32_t ELEMENTS_PER_BLOCK = BLOCK_THREADS * ELEMENTS_PER_THREAD;
uint32_t prefix_sum_shader = 0;
uint32_t add_offsets_shader = 0;

std::unique_ptr<GPUVector<uint32_t>> number_of_elements;



Application& Application::Get()
{
	static Application app;
	return app;
}

Application::Application()
{
}

void Mupfel::Application::CreateGPUBuffer(size_t size)
{
	original_buffer = std::make_unique<GPUVector<uint32_t>>();
	original_buffer->resize(size, 0);

	cpu_buffer = std::make_unique<GPUVector<uint32_t>>();
	cpu_buffer->resize(original_buffer->size(), 0);

	gpu_buffer = std::make_unique<GPUVector<uint32_t>>();
	gpu_buffer->resize(original_buffer->size(), 0);

	blocksum_buffer = std::make_unique<GPUVector<uint32_t>>();
	blocksum_buffer->resize((original_buffer->size() + ELEMENTS_PER_BLOCK - 1) / ELEMENTS_PER_BLOCK, 0);

	number_of_elements.reset(new GPUVector<uint32_t>());
	number_of_elements->resize(1, 0);
	number_of_elements->operator[](0) = size;
}

void Mupfel::Application::FillGPUBufferRND(uint32_t min, uint32_t max)
{
	for (uint32_t i = 0; i < original_buffer->size(); i++)
	{
		original_buffer->operator[](i) = GetRandomNumber(min, max);
	}
}

void Mupfel::Application::CPUPrefixSum()
{
	std::cout << "Calculating the Prefix Sum...\n";

	uint32_t sum = 0;

	for (uint32_t i = 0; i < cpu_buffer->size(); i++)
	{
		uint32_t temp = cpu_buffer->operator[](i);
		cpu_buffer->operator[](i) = sum;
		sum += temp;
	}
	
}

void DispatchBlockScan(GLuint dataBuffer,
	GLuint blockSumBuffer,
	uint32_t numBlocks)
{
	glUseProgram(prefix_sum_shader);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, dataBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, blockSumBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, number_of_elements->GetSSBOID());

	glDispatchCompute(numBlocks, 1, 1);

	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void DispatchAddOffsets(GLuint dataBuffer,
	GLuint blockOffsetBuffer,
	uint32_t numBlocks)
{
	glUseProgram(add_offsets_shader);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, dataBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, blockOffsetBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, number_of_elements->GetSSBOID());

	glDispatchCompute(numBlocks, 1, 1);

	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void Application::PrefixScan(uint32_t dataBuffer, uint32_t elementCount)
{
	uint32_t numBlocks =
		(elementCount + ELEMENTS_PER_BLOCK - 1) / ELEMENTS_PER_BLOCK;

	if (numBlocks == 0)
		return;

	GLuint blockSumBuffer;

	glCreateBuffers(1, &blockSumBuffer);

	glNamedBufferStorage(
		blockSumBuffer,
		sizeof(uint32_t) * numBlocks,
		nullptr,
		GL_DYNAMIC_STORAGE_BIT
	);

	std::cout << "NumBlocks: " << numBlocks << std::endl;

	// Phase 1: scan blocks
	number_of_elements->operator[](0) = elementCount;
	DispatchBlockScan(dataBuffer, blockSumBuffer, numBlocks);

	if (numBlocks > 1)
	{
		// Phase 2: recursively scan block sums
		PrefixScan(blockSumBuffer, numBlocks);

		number_of_elements->operator[](0) = elementCount;
		// Phase 3: add offsets back
		DispatchAddOffsets(dataBuffer, blockSumBuffer, numBlocks);
	}

	glDeleteBuffers(1, &blockSumBuffer);
}

void Application::PrefixScanSimple(uint32_t dataBuffer, uint32_t elementCount)
{
	uint32_t numBlocks =
		(elementCount + ELEMENTS_PER_BLOCK - 1) / ELEMENTS_PER_BLOCK;

	std::cout << "Calculating blocksize: " << "(" << elementCount << " + " << ELEMENTS_PER_BLOCK << " - 1) / " << ELEMENTS_PER_BLOCK << std::endl;

	if (numBlocks == 0)
		return;

	if (numBlocks > 1)
	{
		std::cout << "Element count exceeds block size, simple scan not possible.\n";
		return;
	}

	std::cout << "NumBlocks: " << numBlocks << ", element_count: " << elementCount << std::endl;

	// Phase 1: scan blocks
	number_of_elements->operator[](0) = elementCount;
	DispatchBlockScan(dataBuffer, blocksum_buffer->GetSSBOID(), numBlocks);
}

void Application::GPUPrefixSum()
{
	InitShaders();

	PrefixScan(gpu_buffer->GetSSBOID(), gpu_buffer->size());

	glFinish();
}

bool Mupfel::Application::CheckBuffer()
{
	for (uint32_t i = 0; i < original_buffer->size(); i++)
	{
		if (cpu_buffer->operator[](i) != gpu_buffer->operator[](i))
		{
			return false;
		}
	}

	return true;
}

void Mupfel::Application::PrintBuffer()
{
	std::cout << "Orignal Buffer:\n";
	for (uint32_t i = 0; i < original_buffer->size(); i++)
	{
		std::cout << original_buffer->operator[](i) << ", ";
	}
	std::cout << std::endl;

	std::cout << "CPU Buffer:\n";
	for (uint32_t i = 0; i < cpu_buffer->size(); i++)
	{
		std::cout << cpu_buffer->operator[](i) << ", ";
	}
	std::cout << std::endl;

	std::cout << "GPU Buffer:\n";
	for (uint32_t i = 0; i < gpu_buffer->size(); i++)
	{
		std::cout << gpu_buffer->operator[](i) << ", ";
	}
	std::cout << std::endl;
}

void Mupfel::Application::CopyBuffers()
{
	/* Copy CPU */
	cpu_buffer->resize(original_buffer->size(), 0);
	for (uint32_t i = 0; i < original_buffer->size(); i++)
	{
		cpu_buffer->operator[](i) = original_buffer->operator[](i);
	}

	/* Copy GPU */
	gpu_buffer->resize(original_buffer->size(), 0);
	for (uint32_t i = 0; i < original_buffer->size(); i++)
	{
		gpu_buffer->operator[](i) = original_buffer->operator[](i);
	}

	for (uint32_t i = 0; i < original_buffer->size(); i++)
	{
		if (original_buffer->operator[](i) != gpu_buffer->operator[](i) || original_buffer->operator[](i) != cpu_buffer->operator[](i))
		{
			std::cout << "Buffer copy failed!" << std::endl;
		}
	}
}


void Mupfel::Application::InitShaders()
{
	char* shader_code = LoadFileText("Shaders/blelloch_prefix_sum.glsl");
	int shader_data = rlLoadShader(shader_code, RL_COMPUTE_SHADER);
	prefix_sum_shader = rlLoadShaderProgramCompute(shader_data);
	UnloadFileText(shader_code);

	shader_code = LoadFileText("Shaders/prefix_sum_basic.glsl");
	shader_data = rlLoadShader(shader_code, RL_COMPUTE_SHADER);
	add_offsets_shader = rlLoadShaderProgramCompute(shader_data);
	UnloadFileText(shader_code);
}

Application::~Application()
{
}

static void GLAPIENTRY
MessageCallback(GLenum source,
	GLenum type,
	GLuint id,
	GLenum severity,
	GLsizei length,
	const GLchar* message,
	const void* userParam)
{
	if (severity <= GL_DEBUG_SEVERITY_NOTIFICATION)
	{
		return;
	}
	fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
		(type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
		type, severity, message);
}

bool Application::Init(const ApplicationSpecification& in_spec)
{
	SetTraceLogLevel(LOG_ERROR);
	auto &app = Get();
	app.spec = in_spec;

	if (app.spec.name.empty())
	{
		app.spec.name.insert(0, "Application");
	}

	InitWindow(200, 200, "Test Window");
	

	// During init, enable debug output
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(MessageCallback, 0);

	return true;
}


double Application::GetCurrentTime()
{
	return GetTime();
}

int Mupfel::Application::GetRandomNumber(int min, int max)
{
	return GetRandomValue(min, max);
}


void Application::Run()
{
	std::cout << "Hello World!" << std::endl;

	const size_t buffer_size = 10000000;
	const uint32_t rnd_max = 30;

	const uint64_t max_result_buffer_size = buffer_size * rnd_max;

	if (max_result_buffer_size >= std::numeric_limits<uint32_t>::max())
	{
		std::cout << "indices exceed 32 bit, exiting...\n";
		CloseWindow();
		return;
	}

	std::cout << "using a buffer size of " << buffer_size << std::endl;
	CreateGPUBuffer(buffer_size);

	//FillGPUBufferRND(0, rnd_max);

	FillGPUBufferRND(1, 1);

	CopyBuffers();

	double before_cpu_prefix_sum = GetTime();
	CPUPrefixSum();
	double after_cpu_prefix_sum = GetTime();

	double before_gpu_prefix_sum = GetTime();
	GPUPrefixSum();
	double after_gpu_prefix_sum = GetTime();

	std::cout << "CPU Prefix Sum took: " << ((after_cpu_prefix_sum - before_cpu_prefix_sum) * 1000) << " milliseconds.\n";
	std::cout << "GPU Prefix Sum took: " << ((after_gpu_prefix_sum - before_gpu_prefix_sum) * 1000) << " milliseconds.\n";

	if (!CheckBuffer())
	{
		std::cout << "CPU and GPU buffers dont match!" << std::endl;
	}

	//PrintBuffer();

	CloseWindow();
}