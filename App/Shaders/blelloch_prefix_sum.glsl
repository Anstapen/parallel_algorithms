#version 460
#extension GL_ARB_gpu_shader_int64 : require

#define BLOCK_THREADS 256
#define ELEMENTS_PER_THREAD 8
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

// nur noch 256 Werte!
shared uint temp[BLOCK_THREADS + BLOCK_THREADS / NUM_BANKS];

uint sIndex(uint i)
{
    return i + i / NUM_BANKS;
}

void main()
{
    uint tid = gl_LocalInvocationID.x;
    uint base = gl_WorkGroupID.x * ELEMENTS_PER_BLOCK;

    // --------------------------------------------------
    // 1. LOAD + SEQUENTIAL SCAN (REGISTER)
    // --------------------------------------------------

    uint vals[ELEMENTS_PER_THREAD];

    //uint idx = base + tid + i * BLOCK_THREADS;
    #if 1
    for(int i = 0; i < ELEMENTS_PER_THREAD; i++)
    {
        uint idx = base + tid * ELEMENTS_PER_THREAD + i;
        if(idx < N)
        {
            vals[i] = data[idx];
        }
        else 
        {
            vals[i] = 0;
        }
        
    }
    #endif

#if 1

    uint sum = 0;
    uint tmp = 0;

    for(int i = 0; i < ELEMENTS_PER_THREAD; i++)
    {
        tmp = vals[i];
        vals[i] = sum;
        sum += tmp;
    }

    // sum = total sum of this thread
    temp[sIndex(tid)] = sum;

    barrier();
#endif

    

    // --------------------------------------------------
    // 2. BLELLOCH SCAN (256 THREAD SUMS)
    // --------------------------------------------------

#if 1
    uint offset = 1;

    // UPSWEEP
    for(uint d = BLOCK_THREADS >> 1; d > 0; d >>= 1)
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

#endif
#if 1
    // write block sum
    if(tid == 0)
    {
        uint last = sIndex(BLOCK_THREADS - 1);
        //blockSums[gl_WorkGroupID.x] = temp[last];
        temp[last] = 0;
    }

    // vor downsweep
    barrier();

    offset = BLOCK_THREADS >> 1;

    for(uint d = 1; d < BLOCK_THREADS; d <<= 1)
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
#endif

    uint blockOffset = temp[sIndex(tid)];

    #if 1
    for(int i = 0; i < ELEMENTS_PER_THREAD; i++)
    {
        uint idx = base + tid * ELEMENTS_PER_THREAD + i;
        if(idx < N)
        {
            data[idx] = vals[i] + temp[sIndex(tid)];
        }
        else 
        {
            vals[i] = 0;
        }
        
    }
    #endif
}