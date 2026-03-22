#version 460
#extension GL_ARB_gpu_shader_int64 : require

#define BLOCK_THREADS 256
#define ELEMENTS_PER_THREAD 2
#define ELEMENTS_PER_BLOCK (BLOCK_THREADS * ELEMENTS_PER_THREAD)
#define NUM_BANKS 32

layout(local_size_x = BLOCK_THREADS) in;

layout(std430, binding = 0) buffer Data
{
    uint data[];
};

layout(std430, binding = 1) buffer BlockSums
{
    uint blockSums[];
};

layout(std430, binding = 2) readonly buffer number_of_elements
{
    uint N;
};

shared uint temp[ELEMENTS_PER_BLOCK + ELEMENTS_PER_BLOCK / NUM_BANKS];

uint sIndex(uint i)
{
    return i + i / NUM_BANKS;
}

void main()
{
    uint tid = gl_LocalInvocationID.x;
    uint base = gl_WorkGroupID.x * ELEMENTS_PER_BLOCK;


    /* Load the elements */
    for(int i = 0; i < ELEMENTS_PER_THREAD; i++)
    {
        uint globalIdx = base + tid + i * BLOCK_THREADS;
        uint sharedIdx = tid + i * BLOCK_THREADS;

        if(globalIdx < N)
            temp[sIndex(sharedIdx)] = data[globalIdx];
        else
            temp[sIndex(sharedIdx)] = 0;
    }

    barrier();


    uint offset = 1;

    // UPSWEEP
    for(uint d = ELEMENTS_PER_BLOCK >> 1; d > 0; d >>= 1)
    {
        barrier();

        if(tid < d)
        {
            uint ai = offset * (2 * tid + 1) - 1;
            uint bi = offset * (2 * tid + 2) - 1;

            ai = sIndex(ai);
            bi = sIndex(bi);

            temp[bi] += temp[ai];
        }

        offset <<= 1;
    }

    // write block sum
    if(tid == 0)
    {
        uint last = sIndex(ELEMENTS_PER_BLOCK - 1);
        blockSums[gl_WorkGroupID.x] = temp[last];
        temp[last] = 0;
    }

    // vor downsweep
    barrier();

    offset = ELEMENTS_PER_BLOCK >> 1;

    for(uint d = 1; d < ELEMENTS_PER_BLOCK; d <<= 1)
    {
        barrier();

        if(tid < d)
        {
            uint ai = offset * (2 * tid + 1) - 1;
            uint bi = offset * (2 * tid + 2) - 1;

            ai = sIndex(ai);
            bi = sIndex(bi);

            uint t = temp[ai];
            temp[ai] = temp[bi];
            temp[bi] += t;
        }

        offset >>= 1;
    }

    barrier();

    for(int i = 0; i < ELEMENTS_PER_THREAD; i++)
    {
        uint globalIdx = base + tid + i * BLOCK_THREADS;
        uint sharedIdx = tid + i * BLOCK_THREADS;

        if(globalIdx < N)
            data[globalIdx] = temp[sIndex(sharedIdx)];
    }

}