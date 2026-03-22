#version 460
#extension GL_ARB_gpu_shader_int64 : require

#define BLOCK_THREADS 256
#define ELEMENTS_PER_THREAD 8
#define ELEMENTS_PER_BLOCK (BLOCK_THREADS * ELEMENTS_PER_THREAD)

layout(local_size_x = BLOCK_THREADS) in;

layout(std430, binding = 0) buffer Data
{
    uint data[];
};

layout(std430, binding = 1) buffer Offsets
{
    uint offsets[];
};

layout(std430, binding = 2) readonly buffer number_of_elements
{
    uint64_t N;
};

void main()
{
    uint tid = gl_LocalInvocationID.x;
    uint block = gl_WorkGroupID.x;

    uint base = block * ELEMENTS_PER_BLOCK;
    uint offset = offsets[block];

    for(int i = 0; i < ELEMENTS_PER_THREAD; i++)
    {
        uint idx = base + tid + i * BLOCK_THREADS;

        if(idx < N)
            data[idx] += offset;
    }
}