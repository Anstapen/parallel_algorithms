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

GLsync fence = 0;


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

	number_of_elements.reset(new GPUVector<uint32_t>());
	number_of_elements->resize(1, 0);
	number_of_elements->operator[](0) = size;

	InitPrefixScanBuffers(size);
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

void Application::PrefixScanIterative(uint32_t dataBuffer, uint32_t elementCount)
{
	std::vector<uint32_t> levelSizes;

	uint32_t currentCount = elementCount;
	uint32_t level = 0;

	// -----------------------------
	// Phase 1: DOWN-SWEEP (collect block sums)
	// -----------------------------
	while (true)
	{
		uint32_t numBlocks =
			(currentCount + ELEMENTS_PER_BLOCK - 1) / ELEMENTS_PER_BLOCK;

		if (numBlocks <= 1)
			break;

		levelSizes.push_back(numBlocks);

		number_of_elements->operator[](0) = currentCount;

		DispatchBlockScan(
			(level == 0) ? dataBuffer
			: blocksum_levels[level - 1]->GetSSBOID(),
			blocksum_levels[level]->GetSSBOID(),
			numBlocks
		);

		currentCount = numBlocks;
		level++;
	}

	// -----------------------------
	// Phase 2: scan top level (single block)
	// -----------------------------
	number_of_elements->operator[](0) = currentCount;

	DispatchBlockScan(
		(level == 0) ? dataBuffer
		: blocksum_levels[level - 1]->GetSSBOID(),
		0, // optional, kein Blocksum nötig
		1
	);

	// -----------------------------
	// Phase 3: UP-SWEEP (add offsets)
	// -----------------------------
	for (int i = level - 1; i >= 0; i--)
	{
		uint32_t numBlocks = levelSizes[i];

		number_of_elements->operator[](0) =
			(i == 0) ? elementCount : levelSizes[i - 1];

		DispatchAddOffsets(
			(i == 0) ? dataBuffer
			: blocksum_levels[i - 1]->GetSSBOID(),
			blocksum_levels[i]->GetSSBOID(),
			numBlocks
		);
	}
}

void Application::GPUPrefixSum()
{
	//PrefixScan(gpu_buffer->GetSSBOID(), gpu_buffer->size());
	PrefixScanIterative(gpu_buffer->GetSSBOID(), gpu_buffer->size());
	fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

bool Mupfel::Application::CheckBuffer()
{
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	glFinish();
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

	shader_code = LoadFileText("Shaders/add_offsets.glsl");
	shader_data = rlLoadShader(shader_code, RL_COMPUTE_SHADER);
	add_offsets_shader = rlLoadShaderProgramCompute(shader_data);
	UnloadFileText(shader_code);
}

void Application::InitPrefixScanBuffers(uint32_t maxElements)
{
	blocksum_levels.clear();

	uint32_t current = maxElements;

	while (true)
	{
		uint32_t numBlocks =
			(current + ELEMENTS_PER_BLOCK - 1) / ELEMENTS_PER_BLOCK;

		if (numBlocks <= 1)
			break;

		auto buffer = std::make_unique<GPUVector<uint32_t>>();
		buffer->resize(numBlocks, 0);

		blocksum_levels.push_back(std::move(buffer));

		current = numBlocks;
	}
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

	const size_t buffer_size = 1000000;
	const uint32_t rnd_max = 300;
	const uint32_t num_runs = 10;
	const uint32_t warmup_runs = 20;
	std::cout << "Hello World!" << std::endl;

	const uint64_t max_result_buffer_size = buffer_size * rnd_max;

	if (max_result_buffer_size >= std::numeric_limits<uint32_t>::max())
	{
		std::cout << "indices exceed 32 bit, exiting...\n";
		CloseWindow();
		return;
	}

	InitShaders();
	CreateGPUBuffer(buffer_size);

	// -----------------------------
	// GPU TIMER SETUP
	// -----------------------------
	std::vector<GLuint> queries(num_runs);
	glGenQueries(num_runs, queries.data());

	// -----------------------------
	// WARMUP (extrem wichtig!)
	// -----------------------------
	for (uint32_t i = 0; i < warmup_runs; i++)
	{
		if (fence)
		{
			glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, UINT64_MAX);
			glDeleteSync(fence);
			fence = 0;
		}
		FillGPUBufferRND(0, rnd_max);
		CopyBuffers();
		CPUPrefixSum();
		GPUPrefixSum();
	}

	// -----------------------------
	// TIMING ARRAYS
	// -----------------------------
	std::vector<double> cpu_times(num_runs);
	std::vector<double> gpu_times(num_runs);

	// -----------------------------
	// MAIN BENCH LOOP
	// -----------------------------
	for (uint32_t i = 0; i < num_runs; i++)
	{
		// sicherstellen dass GPU frei ist
		if (fence)
		{
			glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, UINT64_MAX);
			glDeleteSync(fence);
			fence = 0;
		}

		FillGPUBufferRND(0, rnd_max);
		CopyBuffers();

		glBeginQuery(GL_TIME_ELAPSED, queries[i]);

		GPUPrefixSum();

		glEndQuery(GL_TIME_ELAPSED);

		glFinish();  // nur für Benchmark!

		fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, UINT64_MAX);
		glDeleteSync(fence);
		fence = 0;

	}

	// -----------------------------
	// QUERY RESULTS AUSLESEN
	// -----------------------------
	for (uint32_t i = 0; i < num_runs; i++)
	{
		GLuint64 time_ns = 0;
		glGetQueryObjectui64v(queries[i], GL_QUERY_RESULT, &time_ns);

		gpu_times[i] = time_ns / 1e6; // ns -> ms
	}

	// -----------------------------
	// STATISTIK
	// -----------------------------
	auto compute_stats = [](const std::vector<double>& data)
		{
			double sum = 0.0;
			double min = std::numeric_limits<double>::max();
			double max = 0.0;

			for (double v : data)
			{
				sum += v;
				min = std::min(min, v);
				max = std::max(max, v);
			}

			double avg = sum / data.size();

			return std::tuple<double, double, double>(avg, min, max);
		};

	auto [cpu_avg, cpu_min, cpu_max] = compute_stats(cpu_times);
	auto [gpu_avg, gpu_min, gpu_max] = compute_stats(gpu_times);

	// -----------------------------
	// OUTPUT
	// -----------------------------
	std::cout << "\n===== CPU =====\n";
	std::cout << "Avg: " << cpu_avg << " ms\n";
	std::cout << "Min: " << cpu_min << " ms\n";
	std::cout << "Max: " << cpu_max << " ms\n";

	std::cout << "\n===== GPU =====\n";
	std::cout << "Avg: " << gpu_avg << " ms\n";
	std::cout << "Min: " << gpu_min << " ms\n";
	std::cout << "Max: " << gpu_max << " ms\n";

	// Optional: einzelne Runs
	for (uint32_t i = 0; i < num_runs; i++)
	{
		std::cout << "Run " << i + 1
			<< ": CPU = " << cpu_times[i]
			<< " ms, GPU = " << gpu_times[i] << " ms\n";
	}

	glDeleteQueries(num_runs, queries.data());

	if (!CheckBuffer())
	{
		std::cout << "Mismatch of buffers! " << std::endl;
	}

	CloseWindow();
}