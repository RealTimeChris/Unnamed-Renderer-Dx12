// DirectXStuff.hpp (Header Only)
// Apr 23, 2020
// Chris M.
// https://github.com/RealTimeChris

#pragma once

#ifndef DIRECTX_STUFF
	#define DIRECTX_STUFF
#endif

#ifndef URDX12_INLINE
	#define URDX12_INLINE [[msvc::forceinline]] inline
#endif

#ifdef _DEBUG
	#define DEBUG_ENABLED true
#else
	#define DEBUG_ENABLED false
#endif

#ifndef _USE_MATH_DEFINES
	#define _USE_MATH_DEFINES
#endif

#define NOMINMAX

#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <dxcapi.h>
#include <math.h>
#include <random>
#include <bit>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

// For matching types between Host and Device/HLSL code.
#ifndef CUDA_STUFF
typedef unsigned __int32 uint;

struct uint2 {
	unsigned __int32 x{}, y{};
};

struct uint3 {
	unsigned __int32 x{}, y{}, z{};
};

struct int2 {
	__int32 x{}, y{};
};

struct int3 {
	__int32 x{}, y{}, z{};
};

struct float2 {
	float x{}, y{};
};

struct float3 {
	float x{}, y{}, z{};
};
#endif

// Calculates a value blended between two extremes, given a current-step value.
URDX12_INLINE float linear_interpolation(float currentStep, float startValue = -1.0f, float endValue = +1.0f) {
	double blendedValue;

	blendedValue = ((1.0 - ( double )currentStep) * ( double )startValue) + (( double )currentStep * ( double )endValue);

	return ( float )blendedValue;
}

namespace direct_x_stuff {

	// General Utilities.

	bool reportComplete{ false };

	URDX12_INLINE void report_dxgi_debug_info() {
		if (DEBUG_ENABLED == true && direct_x_stuff::reportComplete == false) {
			IDXGIDebug1* debugController{ nullptr };

			HRESULT result{ S_OK };

			result = DXGIGetDebugInterface1(NULL, __uuidof(IDXGIDebug1), reinterpret_cast<void**>(&debugController));

			if (result != S_OK) {
				MessageBox(NULL, L"DXGIGetDebugInterface() failed.", L"DirectXStuff::ReportDXGIDebugInfo() Error", NULL);
				exit(-1);
			}

			result = (debugController)->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);

			if (result != S_OK) {
				MessageBox(NULL, L"ReportLiveObjects() failed.", L"DirectXStuff::ReportDXGIDebugInfo() Error", NULL);
				exit(-1);
			}

			direct_x_stuff::reportComplete = true;

			if (debugController != nullptr) {
				debugController->Release();
				debugController = nullptr;
			}
		}
	}

	URDX12_INLINE void fail_bail(const wchar_t* errorMessage, const wchar_t* errorTitle) {
		MessageBox(NULL, errorMessage, errorTitle, NULL);
		direct_x_stuff::report_dxgi_debug_info();
		exit(-1);
	}

	URDX12_INLINE void result_check(HRESULT result, const wchar_t* errorMessage, const wchar_t* errorTitle) {
		if (result != S_OK) {
			MessageBox(NULL, errorMessage, errorTitle, NULL);
			direct_x_stuff::report_dxgi_debug_info();
			exit(-1);
		}
	}

	// Worker group dimension checking and setting. (Don't forget to set these values in the Compute Shader)
	URDX12_INLINE constexpr uint3 set_group_count_per_grid(uint3 desiredTsGridDimensions, uint3 desiredTsGroupDimensions) {
		uint3 groupCountPerGrid{};

		if ((desiredTsGridDimensions.x % desiredTsGroupDimensions.x != 0) || (desiredTsGridDimensions.y % desiredTsGroupDimensions.y != 0) ||
			(desiredTsGridDimensions.z % desiredTsGroupDimensions.z != 0)) {
			MessageBoxW(NULL, L"Invalid worker-group thread-dimensions.", L"Workload Scaling Error", NULL);
			exit(-1);
		} else {
			groupCountPerGrid.x = desiredTsGridDimensions.x / desiredTsGroupDimensions.x;
			groupCountPerGrid.y = desiredTsGridDimensions.y / desiredTsGroupDimensions.y;
			groupCountPerGrid.z = desiredTsGridDimensions.z / desiredTsGroupDimensions.z;
		}

		return groupCountPerGrid;
	}

	// Create a transition-type resource barrier.
	URDX12_INLINE D3D12_RESOURCE_BARRIER create_resource_transition_barrier(ID3D12Resource* pResource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter,
		UINT subResource = 0u) {
		D3D12_RESOURCE_BARRIER resourceTransitionBarrier{};
		resourceTransitionBarrier.Type					 = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		resourceTransitionBarrier.Transition.pResource	 = pResource;
		resourceTransitionBarrier.Transition.Subresource = subResource;
		resourceTransitionBarrier.Transition.StateBefore = stateBefore;
		resourceTransitionBarrier.Transition.StateAfter	 = stateAfter;

		return resourceTransitionBarrier;
	}


	// Direct3D.

	class unknown_interface {
	  protected:
		IUnknown* pInterface;

		URDX12_INLINE unknown_interface() : pInterface{ nullptr } {
		}

		URDX12_INLINE ~unknown_interface() {
			if (this->pInterface != nullptr) {
				this->pInterface->Release();
				this->pInterface = nullptr;
			}
		}

	  public:
		virtual IUnknown* get_interface() = 0;
	};

	class blob : unknown_interface {
	  public:
		URDX12_INLINE ID3DBlob* get_interface() {
			if (this->pInterface != nullptr) {
				return std::bit_cast<ID3DBlob*>(this->pInterface);
			} else {
				return nullptr;
			}
		}
	};


	// Direct3D 12.

	class d3d12_debug_controller : unknown_interface {
	  public:
		URDX12_INLINE d3d12_debug_controller() {
			if (DEBUG_ENABLED == true) {
				HRESULT result{ S_OK };

				result = D3D12GetDebugInterface(__uuidof(ID3D12Debug3), reinterpret_cast<void**>(&this->pInterface));

				result_check(result, L"D3D12GetDebugInterface() failed.", L"DirectXStuff::D3D12DebugController Error");

				reinterpret_cast<ID3D12Debug3*>(this->pInterface)->EnableDebugLayer();

				reinterpret_cast<ID3D12Debug3*>(this->pInterface)->SetEnableGPUBasedValidation(true);
			}
		}

		URDX12_INLINE ID3D12Debug3* get_interface() {
			if (this->pInterface != nullptr) {
				return reinterpret_cast<ID3D12Debug3*>(this->pInterface);
			} else {
				return nullptr;
			}
		}
	};

	class device : unknown_interface {
	  public:
		URDX12_INLINE device(const wchar_t* objectName = L"Device", D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_12_1) {
			HRESULT result{ S_OK };

			result = D3D12CreateDevice(NULL, featureLevel, __uuidof(ID3D12Device6), reinterpret_cast<void**>(&this->pInterface));

			result_check(result, L"D3D12CreateDevice() failed.", L"DirectXStuff::Device Error");

			result = reinterpret_cast<ID3D12Device6*>(this->pInterface)->SetName(objectName);

			result_check(result, L"SetName() failed.", L"DirectXStuff::Device Error");
		}

		URDX12_INLINE ID3D12Device6* get_interface() {
			if (this->pInterface != nullptr) {
				return reinterpret_cast<ID3D12Device6*>(this->pInterface);
			} else {
				return nullptr;
			}
		}
	};

	class command_queue : unknown_interface {
	  public:
		URDX12_INLINE command_queue(ID3D12Device6* pDevice, D3D12_COMMAND_QUEUE_PRIORITY priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
			const wchar_t* objectName = L"CommandQueue",
			D3D12_COMMAND_LIST_TYPE queueType = D3D12_COMMAND_LIST_TYPE_DIRECT, unsigned __int32 nodeMask = 0) {
			D3D12_COMMAND_QUEUE_DESC commandQueueDescription{};
			commandQueueDescription.Flags	 = D3D12_COMMAND_QUEUE_FLAG_NONE;
			commandQueueDescription.NodeMask = nodeMask;
			commandQueueDescription.Priority = priority;
			commandQueueDescription.Type	 = queueType;

			HRESULT result{ S_OK };

			result = pDevice->CreateCommandQueue(&commandQueueDescription, __uuidof(ID3D12CommandQueue), reinterpret_cast<void**>(&this->pInterface));

			result_check(result, L"CreateCommandQueue() failed.", L"DirectXStuff::CommandQueue Error");

			result = reinterpret_cast<ID3D12CommandQueue*>(this->pInterface)->SetName(objectName);

			result_check(result, L"SetName() failed.", L"DirectXStuff::CommandQueue Error");
		}

		URDX12_INLINE ID3D12CommandQueue* get_interface() {
			if (this != nullptr) {
				return reinterpret_cast<ID3D12CommandQueue*>(this->pInterface);
			} else {
				return nullptr;
			}
		}
	};

	class command_allocator : unknown_interface {
	  public:
		URDX12_INLINE command_allocator(ID3D12Device6* pDevice, const wchar_t* objectName = L"CommandAllocator", D3D12_COMMAND_LIST_TYPE commandListType = D3D12_COMMAND_LIST_TYPE_DIRECT) {
			HRESULT result{ S_OK };

			result = pDevice->CreateCommandAllocator(commandListType, __uuidof(ID3D12CommandAllocator), reinterpret_cast<void**>(&this->pInterface));

			result_check(result, L"CreatedCommandAllocator() failed.", L"DirectXStuff::CommandAllocator Error");

			result = reinterpret_cast<ID3D12CommandAllocator*>(this->pInterface)->SetName(objectName);

			result_check(result, L"SetName() failed.", L"DirectXStuff::CommandAllocator Error");
		}

		URDX12_INLINE ID3D12CommandAllocator* get_interface() {
			if (this->pInterface != nullptr) {
				return reinterpret_cast<ID3D12CommandAllocator*>(this->pInterface);
			} else {
				return nullptr;
			}
		}
	};

	class descriptor_heap : unknown_interface {
	  public:
		URDX12_INLINE descriptor_heap(ID3D12Device6* pDevice, unsigned __int32 descriptorCount = 128u, const wchar_t* objectName = L"DescriptorHeap", unsigned __int32 nodeMask = 0u) {
			D3D12_DESCRIPTOR_HEAP_DESC decsriptorHeapDescription{};
			decsriptorHeapDescription.Type			 = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			decsriptorHeapDescription.NodeMask		 = nodeMask;
			decsriptorHeapDescription.Flags			 = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			decsriptorHeapDescription.NumDescriptors = descriptorCount;

			HRESULT result{ S_OK };

			result = pDevice->CreateDescriptorHeap(&decsriptorHeapDescription, __uuidof(ID3D12DescriptorHeap), reinterpret_cast<void**>(&this->pInterface));

			result_check(result, L"CreateDescriptorHeap() failed.", L"DirectXStuff::DescriptorHeap Error");

			result = reinterpret_cast<ID3D12DescriptorHeap*>(this->pInterface)->SetName(objectName);

			result_check(result, L"SetName() failed.", L"DirectXStuff::DescriptorHeap Error");
		}

		URDX12_INLINE ID3D12DescriptorHeap* get_interface() {
			if (this->pInterface != nullptr) {
				return reinterpret_cast<ID3D12DescriptorHeap*>(this->pInterface);
			} else {
				return nullptr;
			}
		}
	};

	class fence : unknown_interface {
	  protected:
		const uint64_t fenceIncrement;
		uint64_t fenceCurrentValue;
		HANDLE fenceEventHandle;

	  public:
		URDX12_INLINE fence(ID3D12Device6* pDevice, const wchar_t* objectName = L"Fence") : fenceIncrement{ 1u }, fenceCurrentValue{}, fenceEventHandle{} {
			HRESULT result{ S_OK };

			result = pDevice->CreateFence(0u, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence1), reinterpret_cast<void**>(&this->pInterface));

			result_check(result, L"CreateFence() failed.", L"DirectXStuff::Fence Error");

			result = reinterpret_cast<ID3D12Fence1*>(this->pInterface)->SetName(objectName);

			result_check(result, L"SetName() failed.", L"DirectXStuff::Fence Error");
		}

		URDX12_INLINE void flush_command_queue(ID3D12CommandQueue* pCommandQueue, DWORD waitTimeInMilliseconds = INFINITE) {
			pCommandQueue->Signal(reinterpret_cast<ID3D12Fence1*>(this->pInterface), this->fenceCurrentValue + this->fenceIncrement);

			reinterpret_cast<ID3D12Fence1*>(this->pInterface)->SetEventOnCompletion(this->fenceCurrentValue + this->fenceIncrement, this->fenceEventHandle);

			WaitForSingleObject(this->fenceEventHandle, waitTimeInMilliseconds);

			this->fenceCurrentValue = reinterpret_cast<ID3D12Fence1*>(this->pInterface)->GetCompletedValue();
		}

		URDX12_INLINE ID3D12Fence1* get_interface() {
			if (this->pInterface != nullptr) {
				return reinterpret_cast<ID3D12Fence1*>(this->pInterface);
			} else {
				return nullptr;
			}
		}

		URDX12_INLINE ~fence() {
			CloseHandle(this->fenceEventHandle);
		}
	};

	class root_signature : unknown_interface {
	  public:
		URDX12_INLINE root_signature(ID3D12Device6* pDevice, ID3DBlob* pRootSignatureBlob, ID3DBlob* pRootSignatureErrorBlob, unsigned __int32 pipelineResourceCount = 1u,
			const wchar_t* objectName = L"RootSignature", unsigned __int32 nodeMask = 0u) {
			D3D12_DESCRIPTOR_RANGE1 uavDescriptorRange[1]{};
			uavDescriptorRange[0].BaseShaderRegister				= 0u;
			uavDescriptorRange[0].Flags								= D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
			uavDescriptorRange[0].NumDescriptors					= pipelineResourceCount;
			uavDescriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
			uavDescriptorRange[0].RangeType							= D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
			uavDescriptorRange[0].RegisterSpace						= 0u;

			D3D12_ROOT_DESCRIPTOR_TABLE1 descriptorTable{};
			descriptorTable.NumDescriptorRanges = 1u;
			descriptorTable.pDescriptorRanges	= uavDescriptorRange;

			D3D12_ROOT_CONSTANTS rootConstants{};
			rootConstants.Num32BitValues = 60u;
			rootConstants.RegisterSpace	 = 0u;
			rootConstants.ShaderRegister = 0u;

			D3D12_ROOT_PARAMETER1 rootParameters[2]{};
			rootParameters[0].ParameterType	   = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			rootParameters[0].DescriptorTable  = descriptorTable;
			rootParameters[1].ParameterType	   = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
			rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			rootParameters[1].Constants		   = rootConstants;

			D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription{};
			rootSignatureDescription.Version					= D3D_ROOT_SIGNATURE_VERSION_1_1;
			rootSignatureDescription.Desc_1_1.NumStaticSamplers = 0u;
			rootSignatureDescription.Desc_1_1.pStaticSamplers	= nullptr;
			rootSignatureDescription.Desc_1_1.NumParameters		= 2u;
			rootSignatureDescription.Desc_1_1.pParameters		= rootParameters;
			rootSignatureDescription.Desc_1_1.Flags				= D3D12_ROOT_SIGNATURE_FLAG_NONE;

			HRESULT result{ S_OK };

			result = D3D12SerializeVersionedRootSignature(&rootSignatureDescription, &pRootSignatureBlob, &pRootSignatureErrorBlob);

			result_check(result, L"D3D12SerializeVersionedRootSignature() failed.", L"DirectXStuff::RootSignature Error");

			result = pDevice->CreateRootSignature(nodeMask, pRootSignatureBlob->GetBufferPointer(), pRootSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature),
				reinterpret_cast<void**>(&this->pInterface));

			result_check(result, L"CreateRootSignature() failed.", L"DirectXStuff::RootSignature Error");

			result = reinterpret_cast<ID3D12RootSignature*>(this->pInterface)->SetName(objectName);

			result_check(result, L"SetName() failed.", L"DirectXStuff::RootSignature Error");
		}

		URDX12_INLINE ID3D12RootSignature* get_interface() {
			if (this->pInterface != nullptr) {
				return reinterpret_cast<ID3D12RootSignature*>(this->pInterface);
			} else {
				return nullptr;
			}
		}
	};

	class pipeline_state : unknown_interface {
	  public:
		URDX12_INLINE pipeline_state(ID3D12Device6* pDevice, ID3D12RootSignature* pRootSignature, unsigned __int32 sizeOfShaderBytecodeArray, void* shaderBytecodeArray,
			const wchar_t* objectName = L"PipelineState", unsigned __int32 nodeMask = 0u) {
			D3D12_SHADER_BYTECODE shaderByteCode{};
			shaderByteCode.pShaderBytecode = shaderBytecodeArray;
			shaderByteCode.BytecodeLength  = sizeOfShaderBytecodeArray;

			D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDescription{};
			computePipelineStateDescription.CS			   = shaderByteCode;
			computePipelineStateDescription.Flags		   = D3D12_PIPELINE_STATE_FLAG_NONE;
			computePipelineStateDescription.NodeMask	   = nodeMask;
			computePipelineStateDescription.pRootSignature = pRootSignature;

			HRESULT result{ S_OK };

			result = pDevice->CreateComputePipelineState(&computePipelineStateDescription, __uuidof(ID3D12PipelineState), reinterpret_cast<void**>(&this->pInterface));

			result_check(result, L"CreateComputePipelineState() failed.", L"DirectXStuff::PipelineState Error");

			result = reinterpret_cast<ID3D12PipelineState*>(this->pInterface)->SetName(objectName);

			result_check(result, L"this->pPipelineStateInterface->SetName failed.", L"DirectXStuff::PipelineState Error");
		}

		URDX12_INLINE ID3D12PipelineState* get_interface() {
			if (this->pInterface != nullptr) {
				return reinterpret_cast<ID3D12PipelineState*>(this->pInterface);
			} else {
				return nullptr;
			}
		}
	};

	class graphics_command_list : unknown_interface {
	  public:
		URDX12_INLINE graphics_command_list(ID3D12Device6* pDevice, ID3D12CommandAllocator* pCommandAllocator, const wchar_t* objectName = L"GraphicsCommandList",
			D3D12_COMMAND_LIST_TYPE commandListType = D3D12_COMMAND_LIST_TYPE_DIRECT, unsigned __int32 nodeMask = 0u) {
			HRESULT result{ S_OK };

			result = pDevice->CreateCommandList(nodeMask, commandListType, pCommandAllocator, nullptr, __uuidof(ID3D12GraphicsCommandList5),
				reinterpret_cast<void**>(&this->pInterface));

			result_check(result, L"CreateCommandList() failed.", L"DirectXStuff::GraphicsCommandList Error");

			result = reinterpret_cast<ID3D12GraphicsCommandList5*>(this->pInterface)->SetName(objectName);

			result_check(result, L"SetName() failed.", L"DirectXStuff::GraphicsCommandList Error");

			result = this->get_interface()->Close();

			result_check(result, L"Close() failed.", L"DirectXStuff::GraphicsCommandList Error");
		}

		URDX12_INLINE ID3D12GraphicsCommandList5* get_interface() {
			if (this->pInterface != nullptr) {
				return reinterpret_cast<ID3D12GraphicsCommandList5*>(this->pInterface);
			} else {
				return nullptr;
			}
		}

		URDX12_INLINE ID3D12CommandList* get_list_for_submission() {
			if (this->pInterface != nullptr) {
				return reinterpret_cast<ID3D12CommandList*>(this->pInterface);
			} else {
				return nullptr;
			}
		}
	};

	struct r8g8b8a8_uint {
		unsigned __int8 r{}, g{}, b{}, a{};
	};

	struct r8g8b8a8_uint_host_buffer_config {
		uint64_t bufferLengthInElements{};
		r8g8b8a8_uint initialColor{};
	};

	class r8g8b8a8_uint_host_buffer {
	  protected:
		r8g8b8a8_uint* pBuffer{ nullptr };
		uint64_t bufferSizeInBytes{ 0u };

	  public:
		URDX12_INLINE r8g8b8a8_uint_host_buffer(r8g8b8a8_uint_host_buffer_config config) {
			this->pBuffer = new r8g8b8a8_uint[config.bufferLengthInElements]{};

			for (uint64_t i{ 0 }; i < config.bufferLengthInElements; i++) {
				this->pBuffer[i].r = config.initialColor.r;
				this->pBuffer[i].g = config.initialColor.g;
				this->pBuffer[i].b = config.initialColor.b;
				this->pBuffer[i].a = config.initialColor.a;
			}

			this->bufferSizeInBytes = config.bufferLengthInElements * sizeof(r8g8b8a8_uint);
		}

		URDX12_INLINE uint64_t get_buffer_size_in_bytes() const {
			return this->bufferSizeInBytes;
		}

		URDX12_INLINE void* get_pointer_to_buffer_start() {
			if (this->pBuffer != nullptr) {
				return reinterpret_cast<void*>(this->pBuffer);
			} else {
				return nullptr;
			}
		}

		URDX12_INLINE ~r8g8b8a8_uint_host_buffer() {
			if (this->pBuffer != nullptr) {
				delete[] this->pBuffer;
				this->pBuffer = nullptr;
			}
		}
	};

	struct random_float_host_buffer_config {
		__int32 randomFloatCount{};
	};

	class random_float_host_buffer {
	  protected:
		int64_t randomFloatCount{};
		float* pBuffer{ nullptr };
		int64_t bufferSizeInBytes{ 0u };
		std::random_device randomDevice{};
		std::mt19937_64 mersenneTwisterEngine{};

	  public:
		URDX12_INLINE random_float_host_buffer(random_float_host_buffer_config config) {
			this->randomFloatCount	= config.randomFloatCount;
			this->pBuffer			= new float[config.randomFloatCount]{};
			this->bufferSizeInBytes = config.randomFloatCount * sizeof(float);

			for (int64_t i{ 0 }; i < this->randomFloatCount; i++) {
				float randomFloat{};

				randomFloat = (( float )mersenneTwisterEngine() / ( float )mersenneTwisterEngine.max()) * 2.0f - 1.0f;

				this->pBuffer[i] = randomFloat;
			}
		}

		URDX12_INLINE void refresh_buffer_contents() {
			for (int64_t i{ 0 }; i < this->randomFloatCount; i++) {
				float randomFloat{};

				randomFloat = (( float )mersenneTwisterEngine() / ( float )mersenneTwisterEngine.max()) * 2.0f - 1.0f;

				this->pBuffer[i] = randomFloat;
			}
		}

		URDX12_INLINE uint64_t get_buffer_size_in_bytes() const {
			return this->bufferSizeInBytes;
		}

		URDX12_INLINE void* get_pointer_to_buffer_start() {
			if (this->pBuffer != nullptr) {
				return reinterpret_cast<void*>(this->pBuffer);
			} else {
				return nullptr;
			}
		}

		URDX12_INLINE ~random_float_host_buffer() {
			if (this->pBuffer != nullptr) {
				delete[] this->pBuffer;
				this->pBuffer = nullptr;
			}
		}
	};

	struct buffer_config {
		D3D12_CPU_PAGE_PROPERTY cpuPageProperty{};
		D3D12_MEMORY_POOL memoryPool{};
		DXGI_FORMAT bufferFormat{};
		unsigned __int32 bufferWidth{};
		D3D12_RESOURCE_STATES initialResourceState{};
		D3D12_HEAP_FLAGS heapFlags{};
	};

	class buffer : unknown_interface {
	  public:
		URDX12_INLINE buffer(ID3D12Device6* pDevice, buffer_config config, const wchar_t* objectName = L"Buffer") {
			D3D12_HEAP_PROPERTIES heapProperties{};
			heapProperties.CPUPageProperty		= config.cpuPageProperty;
			heapProperties.CreationNodeMask		= 0u;
			heapProperties.MemoryPoolPreference = config.memoryPool;
			heapProperties.Type					= D3D12_HEAP_TYPE_CUSTOM;
			heapProperties.VisibleNodeMask		= 0u;

			D3D12_RESOURCE_DESC resourceDescription{};
			resourceDescription.Alignment		   = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
			resourceDescription.DepthOrArraySize   = 1u;
			resourceDescription.Dimension		   = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDescription.Flags			   = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			resourceDescription.Format			   = config.bufferFormat;
			resourceDescription.Height			   = 1u;
			resourceDescription.Layout			   = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDescription.MipLevels		   = 1u;
			resourceDescription.SampleDesc.Count   = 1u;
			resourceDescription.SampleDesc.Quality = 0u;
			resourceDescription.Width			   = config.bufferWidth;

			HRESULT result{ S_OK };

			result = pDevice->CreateCommittedResource(&heapProperties, config.heapFlags, &resourceDescription, config.initialResourceState, nullptr, __uuidof(ID3D12Resource),
				reinterpret_cast<void**>(&this->pInterface));

			result_check(result, L"CreateCommittedResource() failed.", L"DirectXStuff::Buffer Error");

			result = reinterpret_cast<ID3D12Resource*>(this->pInterface)->SetName(objectName);

			result_check(result, L"SetName() failed.", L"DirectXStuff::Buffer Error");
		}

		URDX12_INLINE ID3D12Resource* get_interface() {
			if (this->pInterface != nullptr) {
				return reinterpret_cast<ID3D12Resource*>(this->pInterface);
			} else {
				return nullptr;
			}
		}
	};

	struct texture_1d_config {
		D3D12_CPU_PAGE_PROPERTY cpuPageProperty{};
		unsigned __int32 nodeMask{};
		D3D12_MEMORY_POOL memoryPool{};
		DXGI_FORMAT textureFormat{};
		unsigned __int32 textureWidth{};
		D3D12_RESOURCE_STATES initialResourceState{};
		D3D12_HEAP_FLAGS heapFlags{};
	};

	class texture_1d : unknown_interface {
	  public:
		URDX12_INLINE texture_1d(ID3D12Device6* pDevice, texture_1d_config config, const wchar_t* objectName = L"Texture1D") {
			D3D12_HEAP_PROPERTIES heapProperties{};
			heapProperties.CPUPageProperty		= config.cpuPageProperty;
			heapProperties.CreationNodeMask		= config.nodeMask;
			heapProperties.MemoryPoolPreference = config.memoryPool;
			heapProperties.Type					= D3D12_HEAP_TYPE_CUSTOM;
			heapProperties.VisibleNodeMask		= config.nodeMask;

			D3D12_RESOURCE_DESC resourceDescription{};
			resourceDescription.Alignment		   = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
			resourceDescription.DepthOrArraySize   = 1u;
			resourceDescription.Dimension		   = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
			resourceDescription.Flags			   = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			resourceDescription.Format			   = config.textureFormat;
			resourceDescription.Height			   = 1u;
			resourceDescription.Layout			   = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			resourceDescription.MipLevels		   = 1u;
			resourceDescription.SampleDesc.Count   = 1u;
			resourceDescription.SampleDesc.Quality = 0u;
			resourceDescription.Width			   = config.textureWidth;

			HRESULT result{ S_OK };

			result = pDevice->CreateCommittedResource(&heapProperties, config.heapFlags, &resourceDescription, config.initialResourceState, nullptr, __uuidof(ID3D12Resource),
				reinterpret_cast<void**>(&this->pInterface));

			result_check(result, L"CreateCommittedResource() failed.", L"DirectXStuff::Texture1D Error");

			result = reinterpret_cast<ID3D12Resource*>(this->pInterface)->SetName(objectName);

			result_check(result, L"SetName() failed.", L"DirectXStuff::Texture1D Error");
		}

		URDX12_INLINE ID3D12Resource* get_interface() {
			if (this->pInterface != nullptr) {
				return reinterpret_cast<ID3D12Resource*>(this->pInterface);
			} else {
				return nullptr;
			}
		}
	};

	struct texture_2d_config {
		D3D12_CPU_PAGE_PROPERTY cpuPageProperty{};
		unsigned __int32 nodeMask{};
		D3D12_MEMORY_POOL memoryPool{};
		DXGI_FORMAT textureFormat{};
		unsigned __int32 textureWidth{};
		unsigned __int32 textureHeight{};
		D3D12_RESOURCE_STATES initialResourceState{};
		D3D12_HEAP_FLAGS heapFlags{};
	};

	class texture_2d : unknown_interface {
	  public:
		URDX12_INLINE texture_2d(ID3D12Device6* pDevice, texture_2d_config config, const wchar_t* objectName = L"Texture2D") {
			D3D12_HEAP_PROPERTIES heapProperties{};
			heapProperties.CPUPageProperty		= config.cpuPageProperty;
			heapProperties.CreationNodeMask		= config.nodeMask;
			heapProperties.MemoryPoolPreference = config.memoryPool;
			heapProperties.Type					= D3D12_HEAP_TYPE_CUSTOM;
			heapProperties.VisibleNodeMask		= config.nodeMask;

			D3D12_RESOURCE_DESC resourceDescription{};
			resourceDescription.Alignment		   = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
			resourceDescription.DepthOrArraySize   = 1u;
			resourceDescription.Dimension		   = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			resourceDescription.Flags			   = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			resourceDescription.Format			   = config.textureFormat;
			resourceDescription.Height			   = config.textureHeight;
			resourceDescription.Layout			   = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			resourceDescription.MipLevels		   = 1u;
			resourceDescription.SampleDesc.Count   = 1u;
			resourceDescription.SampleDesc.Quality = 0u;
			resourceDescription.Width			   = config.textureWidth;

			HRESULT result{ S_OK };

			result = pDevice->CreateCommittedResource(&heapProperties, config.heapFlags, &resourceDescription, config.initialResourceState, nullptr, __uuidof(ID3D12Resource),
				reinterpret_cast<void**>(&this->pInterface));

			result_check(result, L"CreateCommittedResource() failed.", L"DirectXStuff::Texture2D Error");

			result = reinterpret_cast<ID3D12Resource*>(this->pInterface)->SetName(objectName);

			result_check(result, L"SetName() failed.", L"DirectXStuff::Texture2D Error");
		}

		URDX12_INLINE ID3D12Resource* get_interface() {
			if (this->pInterface != nullptr) {
				return reinterpret_cast<ID3D12Resource*>(this->pInterface);
			} else {
				return nullptr;
			}
		}
	};

	struct texture_3d_config {
		D3D12_CPU_PAGE_PROPERTY cpuPageProperty{};
		unsigned __int32 nodeMask{};
		D3D12_MEMORY_POOL memoryPool{};
		DXGI_FORMAT textureFormat{};
		unsigned __int32 textureWidth{};
		unsigned __int32 textureHeight{};
		unsigned __int32 textureDepth{};
		D3D12_RESOURCE_STATES initialResourceState{};
		D3D12_HEAP_FLAGS heapFlags{};
	};

	class texture_3d : unknown_interface {
	  public:
		URDX12_INLINE texture_3d(ID3D12Device6* pDevice, texture_3d_config config, const wchar_t* objectName = L"Texture3D") {
			D3D12_HEAP_PROPERTIES heapProperties{};
			heapProperties.CPUPageProperty		= config.cpuPageProperty;
			heapProperties.CreationNodeMask		= config.nodeMask;
			heapProperties.MemoryPoolPreference = config.memoryPool;
			heapProperties.Type					= D3D12_HEAP_TYPE_CUSTOM;
			heapProperties.VisibleNodeMask		= config.nodeMask;

			D3D12_RESOURCE_DESC resourceDescription{};
			resourceDescription.Alignment		   = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
			resourceDescription.DepthOrArraySize   = config.textureDepth;
			resourceDescription.Dimension		   = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
			resourceDescription.Flags			   = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			resourceDescription.Format			   = config.textureFormat;
			resourceDescription.Height			   = config.textureHeight;
			resourceDescription.Layout			   = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			resourceDescription.MipLevels		   = 1u;
			resourceDescription.SampleDesc.Count   = 1u;
			resourceDescription.SampleDesc.Quality = 0u;
			resourceDescription.Width			   = config.textureWidth;

			HRESULT result{ S_OK };

			result = pDevice->CreateCommittedResource(&heapProperties, config.heapFlags, &resourceDescription, config.initialResourceState, nullptr, __uuidof(ID3D12Resource),
				reinterpret_cast<void**>(&this->pInterface));

			result_check(result, L"CreateCommittedResource() failed.", L"DirectXStuff::Texture3D Error");

			result = reinterpret_cast<ID3D12Resource*>(this->pInterface)->SetName(objectName);

			result_check(result, L"SetName() failed.", L"DirectXStuff::Texture3D Error");
		}

		URDX12_INLINE ID3D12Resource* get_interface() {
			if (this->pInterface != nullptr) {
				return reinterpret_cast<ID3D12Resource*>(this->pInterface);
			} else {
				return nullptr;
			}
		}
	};


	// DirectX Graphics Infrastructure 2/2.

	class factory : unknown_interface {
	  public:
		URDX12_INLINE factory() {
			HRESULT result{ S_OK };

			result = CreateDXGIFactory(__uuidof(IDXGIFactory7), reinterpret_cast<void**>(&this->pInterface));

			result_check(result, L"CreateDXGIFactory() failed.", L"DXGIStuff::Factory Error");
		}

		URDX12_INLINE IDXGIFactory7* get_interface() {
			if (this->pInterface != nullptr) {
				return reinterpret_cast<IDXGIFactory7*>(this->pInterface);
			} else {
				return nullptr;
			}
		}
	};

	class swap_chain : unknown_interface {
	  public:
		URDX12_INLINE swap_chain(IDXGIFactory7* pFactory, ID3D12CommandQueue* pCommandQueue, HWND windowHandle, unsigned __int32 pixelWidth, unsigned __int32 pixelHeight,
			unsigned __int32 bufferCount = 2u, DXGI_FORMAT bufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM) {
			DXGI_SWAP_CHAIN_DESC1 swapChainDescription{};
			swapChainDescription.AlphaMode			= DXGI_ALPHA_MODE_IGNORE;
			swapChainDescription.BufferCount		= bufferCount;
			swapChainDescription.BufferUsage		= DXGI_USAGE_BACK_BUFFER;
			swapChainDescription.Flags				= NULL;
			swapChainDescription.Format				= bufferFormat;
			swapChainDescription.Height				= pixelHeight;
			swapChainDescription.SampleDesc.Count	= 1u;
			swapChainDescription.SampleDesc.Quality = 0u;
			swapChainDescription.Scaling			= DXGI_SCALING_NONE;
			swapChainDescription.Stereo				= FALSE;
			swapChainDescription.SwapEffect			= DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
			swapChainDescription.Width				= pixelWidth;

			HRESULT result{ S_OK };

			result = pFactory->CreateSwapChainForHwnd(pCommandQueue, windowHandle, &swapChainDescription, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(&this->pInterface));

			result_check(result, L"CreateSwapChainForHwnd() failed.", L"DXGIStuff::SwapChain Error");
		}

		URDX12_INLINE IDXGISwapChain4* get_interface() {
			if (this->pInterface != nullptr) {
				return reinterpret_cast<IDXGISwapChain4*>(this->pInterface);
			} else {
				return nullptr;
			}
		}
	};


	// DirectX Shader Compiler.

	class dxc_library : unknown_interface {
	  public:
		URDX12_INLINE dxc_library() {
			HRESULT result{ S_OK };

			result = DxcCreateInstance(CLSID_DxcLibrary, __uuidof(IDxcLibrary), reinterpret_cast<void**>(&this->pInterface));

			result_check(result, L"DxcCreateInstance() failed.", L"DirectXStuff::DXCLibrary Error");
		}

		URDX12_INLINE IDxcLibrary* get_interface() {
			if (this->pInterface != nullptr) {
				return reinterpret_cast<IDxcLibrary*>(this->pInterface);
			} else {
				return nullptr;
			}
		}
	};

	class dxc_compiler : unknown_interface {
	  public:
		URDX12_INLINE dxc_compiler() {
			HRESULT result{ S_OK };

			result = DxcCreateInstance(CLSID_DxcCompiler, __uuidof(IDxcCompiler), reinterpret_cast<void**>(&this->pInterface));

			result_check(result, L"DxcCreateInstance() failed.", L"DirectXStuff::DXCCompiler Error");
		}

		URDX12_INLINE IDxcCompiler2* get_interface() {
			if (this->pInterface != nullptr) {
				return reinterpret_cast<IDxcCompiler2*>(this->pInterface);
			} else {
				return nullptr;
			}
		}
	};

	struct shader_config {
		const wchar_t* shaderFileName{ L"Shader.hlsl" };
		unsigned __int32 shaderFileEncoding{ CP_UTF8 };
		const wchar_t* shaderEntryPoint{ L"Main" };
		const wchar_t* targetProfile{ L"cs_6_3" };
	};

	class shader {
	  protected:
		IDxcBlobEncoding* pSourceBlob;
		IDxcOperationResult* pOperationResult;
		IDxcBlob* pCompiledShaderBlob;
		IDxcIncludeHandler* pIncludeHandler;

	  public:
		URDX12_INLINE shader(IDxcLibrary* pDXCLibrary, IDxcCompiler* pDXCCompiler, shader_config config)
			: pSourceBlob{ nullptr }, pOperationResult{ nullptr }, pCompiledShaderBlob{ nullptr }, pIncludeHandler{ nullptr } {
			HRESULT result{ S_OK };

			result = pDXCLibrary->CreateBlobFromFile(config.shaderFileName, &config.shaderFileEncoding, &pSourceBlob);

			result_check(result, L"CreateBlobFromFile() failed.", L"DirectXStuff::Shader Error");

			result = pDXCLibrary->CreateIncludeHandler(&pIncludeHandler);

			result_check(result, L"CreateIncludeHandler() failed.", L"DirectXStuff::Shader Error");

			result =
				pDXCCompiler->Compile(pSourceBlob, config.shaderFileName, config.shaderEntryPoint, config.targetProfile, NULL, 0u, NULL, 0u, pIncludeHandler, &pOperationResult);

			result_check(result, L"Compile() failed.", L"DirectXStuff::Shader Error");

			pOperationResult->GetStatus(&result);

			result_check(result, L"Compile() failed.", L"DirectXStuff::Shader Error");

			result = pOperationResult->GetResult(&pCompiledShaderBlob);

			result_check(result, L"Compile() failed.", L"DirectXStuff::Shader Error");
		}

		URDX12_INLINE unsigned __int32 get_shader_byte_code_size() {
			if (pCompiledShaderBlob != nullptr) {
				return ( unsigned __int32 )(pCompiledShaderBlob->GetBufferSize());
			} else {
				return 0u;
			}
		}

		URDX12_INLINE void* get_shader_byte_code() {
			if (pCompiledShaderBlob != nullptr) {
				return pCompiledShaderBlob->GetBufferPointer();
			} else {
				return nullptr;
			}
		}

		URDX12_INLINE ~shader() {
			if (this->pIncludeHandler != nullptr) {
				this->pIncludeHandler->Release();
				this->pIncludeHandler = nullptr;
			}

			if (this->pCompiledShaderBlob != nullptr) {
				this->pCompiledShaderBlob->Release();
				this->pCompiledShaderBlob = nullptr;
			}

			if (this->pOperationResult != nullptr) {
				this->pOperationResult->Release();
				this->pOperationResult = nullptr;
			}

			if (this->pSourceBlob != nullptr) {
				this->pSourceBlob->Release();
				this->pSourceBlob = nullptr;
			}
		}
	};

}
