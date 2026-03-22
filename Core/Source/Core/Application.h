#pragma once

#include <string>
#include <memory>
#include <vector>

#include "GPU/GPUVector.h"

namespace Mupfel {

	/**
	 * @brief Defines the specification parameters used to initialize the Application.
	 *
	 * This structure provides all information needed for application setup,
	 * including the application name and window configuration.
	 */
	struct ApplicationSpecification {
		/**
		 * @brief The name of the application (used as window title).
		 */
		std::string name;
	};

	struct ProgramParams {
		/**
		 * @brief The total number of currently active entities.
		 *
		 * For now, this is equal to the number of entities that have a
		 * transform component. Might change in the future if there
		 * are more clever ways on how to store the active entity buffers.
		 */
		uint64_t buffer_size = 0;

		uint64_t workload = 0;

		uint64_t num_threads = 0;
	};

	/**
	 * @brief The main entry point of the Mupfel Engine.
	 *
	 * The Application class represents the central runtime controller
	 * of the entire engine. It manages window creation, event handling,
	 * rendering, physics simulation, input, debug overlay, and layer management.
	 *
	 * Only one instance of Application exists (Singleton pattern).
	 */
	class Application
	{
		friend class DebugLayer;

	public:
		/**
		 * @brief Retrieves the global instance of the Application.
		 * @return Reference to the singleton Application object.
		 */
		static Application& Get();

		/**
		 * @brief Destructor. Cleans up engine systems and resources.
		 */
		~Application();

		/**
		 * @brief Initializes the engine subsystems and creates the main window.
		 * @param spec The specification used for initialization.
		 * @return True if initialization succeeded.
		 */
		bool Init(const ApplicationSpecification& spec);

		/**
		 * @brief Starts the main application loop.
		 *
		 * This function runs the engine until the window is closed or
		 * Application::Stop() is called.
		 */
		void Run();

		/**
		 * @brief Returns the current time in seconds since startup.
		 */
		static double GetCurrentTime();

		/**
		 * @brief Returns a random integer between @p min and @p max.
		 */
		static int GetRandomNumber(int min, int max);


	private:
		/**
		 * @brief Private constructor (Singleton pattern).
		 */
		Application();

		void CreateGPUBuffer(size_t size);

		void FillGPUBufferRND(uint32_t min, uint32_t max);

		void CPUPrefixSum();

		void GPUPrefixSum();

		bool CheckBuffer();

		void PrintBuffer();

		void CopyBuffers();

		void PrefixScan(uint32_t dataBuffer, uint32_t elementCount);
		void PrefixScanIterative(uint32_t dataBuffer, uint32_t elementCount);

		void InitShaders();

		void InitPrefixScanBuffers(uint32_t maxElements);

	private:
		/** @brief The startup specification of the application. */
		ApplicationSpecification spec;

		/** @brief Timestamp of the current frame’s start time. */
		double start_time = 0.0;

		/** @brief Duration of the most recently completed frame (in seconds). */
		double last_frame_time = 0.0;

		std::unique_ptr<GPUVector<uint32_t>> original_buffer = nullptr;
		std::unique_ptr<GPUVector<uint32_t>> cpu_buffer = nullptr;
		std::unique_ptr<GPUVector<uint32_t>> gpu_buffer = nullptr;
		std::vector<std::unique_ptr<GPUVector<uint32_t>>> blocksum_levels;
	};
}



