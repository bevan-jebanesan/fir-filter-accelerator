#include <stdio.h>

#define TAPS 16
#define TSTEP1 32
#define TSTEP2 48

#include "expected.inc"

// Compiler barrier to prevent instruction reordering
static void clobber() {
  asm volatile ("" : : : "memory");
}

int main( int argc, char* argv[] )
{
  int n;
  // Memory Map Addresses (DRAM)
  volatile short *coef=(short *)0x60004000;
  volatile short *input=(short *)0x60002000;
  volatile short *output=(short *)0x60001000;
  short error, total_error;

  // DMA and Accelerator Mapped Registers
  volatile long long *dma_st      = (volatile long long*)  0x70000000;
  volatile long long **dma_sr     = (volatile long long**) 0x70000010;
  volatile long long **dma_dr     = (volatile long long**) 0x70000018;
  volatile long long *dma_len     = (volatile long long*)  0x70000020;
  
  volatile long long *accel_st    = (volatile long long*)  0x70010000;
  volatile long long *accel_ctrl  = (volatile long long*)  0x70010008;
  volatile long long *accel_w     = (volatile long long*)  0x70010010;
  volatile long long *accel_x     = (volatile long long*)  0x70010030;
  volatile long long *accel_z     = (volatile long long*)  0x70010050;

  // ========================================================================
  // HARDWARE ACCELERATED FIR FILTER OPERATIONS
  // ========================================================================

  // ------------------------------------------------------------------------
  // STEP 1: Load First Set of Coefficients (W1)
  // ------------------------------------------------------------------------
  *accel_ctrl = 0x10;  // Command: Load coefficients
  clobber();
  
  // DMA coefficients from DRAM to Accelerator
  *dma_sr = (volatile long long*)((long)coef & 0x1fffffff);
  *dma_dr = (volatile long long*)((long)accel_w & 0x1fffffff);
  *dma_len = 32;  // 16 taps * 2 bytes = 32 bytes
  clobber();

  // IMPORTANT: Read the Echo-back from Accelerator (Defined in your Accelerator.h)
  // If we don't read this, the output FIFO will be full and stall the next step.
  *dma_sr = (volatile long long*)((long)accel_z & 0x1fffffff);
  *dma_dr = (volatile long long*)((long)output & 0x1fffffff); // Dump to output (or temp buffer)
  *dma_len = 32;
  clobber();

  // ------------------------------------------------------------------------
  // STEP 2: Process First FIR Filter (TSTEP1 = 32 samples)
  // ------------------------------------------------------------------------
  
  // --- Batch 1: First 16 samples ---
  *accel_ctrl = 0x20;  // Command: Start New FIR (Reset History)
  clobber();

  *dma_sr = (volatile long long*)((long)input & 0x1fffffff);
  *dma_dr = (volatile long long*)((long)accel_x & 0x1fffffff);
  *dma_len = 32;  // Send 16 samples
  clobber();

  *dma_sr = (volatile long long*)((long)accel_z & 0x1fffffff);
  *dma_dr = (volatile long long*)((long)output & 0x1fffffff);
  *dma_len = 32;  // Read 16 results
  clobber();

  // --- Batch 2: Next 16 samples ---
  *accel_ctrl = 0x21;  // Command: Continue (Keep History)
  clobber();

  *dma_sr = (volatile long long*)((long)(input + 16) & 0x1fffffff);
  *dma_dr = (volatile long long*)((long)accel_x & 0x1fffffff);
  *dma_len = 32;
  clobber();

  *dma_sr = (volatile long long*)((long)accel_z & 0x1fffffff);
  *dma_dr = (volatile long long*)((long)(output + 16) & 0x1fffffff);
  *dma_len = 32;
  clobber();

  // ------------------------------------------------------------------------
  // STEP 3: Load Second Set of Coefficients (W2)
  // ------------------------------------------------------------------------
  *accel_ctrl = 0x10; // Load Coeffs
  clobber();
  
  // Note: coef + 16 skips the first 16 coefficients to get the next set
  *dma_sr = (volatile long long*)((long)(coef + 16) & 0x1fffffff);
  *dma_dr = (volatile long long*)((long)accel_w & 0x1fffffff);
  *dma_len = 32;
  clobber();

  // Consume Echo-back
  *dma_sr = (volatile long long*)((long)accel_z & 0x1fffffff);
  *dma_dr = (volatile long long*)((long)(output + TSTEP1) & 0x1fffffff); // Dump to output
  *dma_len = 32;
  clobber();

  // ------------------------------------------------------------------------
  // STEP 4: Process Second FIR Filter (TSTEP2 = 48 samples)
  // ------------------------------------------------------------------------
  
  // --- Batch 1: Outputs 32-47 ---
  *accel_ctrl = 0x20;  // Command: Reset History for new filter
  clobber();

  // Note: input + TSTEP1 points to the start of the 2nd input stream
  *dma_sr = (volatile long long*)((long)(input + TSTEP1) & 0x1fffffff);
  *dma_dr = (volatile long long*)((long)accel_x & 0x1fffffff);
  *dma_len = 32;
  clobber();

  *dma_sr = (volatile long long*)((long)accel_z & 0x1fffffff);
  *dma_dr = (volatile long long*)((long)(output + TSTEP1) & 0x1fffffff);
  *dma_len = 32;
  clobber();

  // --- Batch 2: Outputs 48-63 ---
  *accel_ctrl = 0x21;  // Command: Continue
  clobber();

  *dma_sr = (volatile long long*)((long)(input + TSTEP1 + 16) & 0x1fffffff);
  *dma_dr = (volatile long long*)((long)accel_x & 0x1fffffff);
  *dma_len = 32;
  clobber();

  *dma_sr = (volatile long long*)((long)accel_z & 0x1fffffff);
  *dma_dr = (volatile long long*)((long)(output + TSTEP1 + 16) & 0x1fffffff);
  *dma_len = 32;
  clobber();

  // --- Batch 3: Outputs 64-79 ---
  *accel_ctrl = 0x21;  // Command: Continue
  clobber();

  *dma_sr = (volatile long long*)((long)(input + TSTEP1 + 32) & 0x1fffffff);
  *dma_dr = (volatile long long*)((long)accel_x & 0x1fffffff);
  *dma_len = 32;
  clobber();

  *dma_sr = (volatile long long*)((long)accel_z & 0x1fffffff);
  *dma_dr = (volatile long long*)((long)(output + TSTEP1 + 32) & 0x1fffffff);
  *dma_len = 32;
  clobber();

  // ========================================================================
  // ERROR CHECK
  // ========================================================================
  total_error = 0;
  for (n = 0; n < (TSTEP1 + TSTEP2); n++) {
    error = expected[n] - output[n];
    total_error += (error < 0) ? (-error) : (error);
  }

  printf("cpu main FIR total error: %d\n", total_error);

  // ========================================================================
  // ORIGINAL TEST TRANSACTIONS 
  // (Disabled with #if 0 to prevent interference with final error check)
  // ========================================================================
#if 0
  *dma_sr=(volatile long long*)((long)input & 0x1fffffff);
  *dma_dr=(volatile long long*)((long)accel_x & 0x1fffffff);
  *dma_len=32;
  clobber();
 
  *dma_sr=(volatile long long*)((long)accel_z & 0x1fffffff);
  *dma_dr=(volatile long long*)((long)output & 0x1fffffff);
  *dma_len=32;
  clobber();

  total_error=0;
  for (n=0; n<TAPS; n++) {
    error=input[n]-output[n];
    total_error+=(error<0)?(-error):(error);
  }
  printf("cpu main DMA transfer 1 total error: %d\n",total_error);

  *dma_sr=(volatile long long*)((long)coef & 0x1fffffff);; // memctl coeff address
  *dma_dr=(volatile long long*)((long)accel_w & 0x1fffffff); // fir coeff address
  *dma_len=32; // starts transfer
  clobber();

  *dma_sr=(volatile long long*)((long)accel_z & 0x1fffffff);
  *dma_dr=(volatile long long*)((long)output & 0x1fffffff);
  *dma_len=32; // starts transfer 
  clobber();

  total_error=0;
  for (n=0; n<TAPS; n++) {
    error=coef[n]-output[n];
    total_error+=(error<0)?(-error):(error);
  }
  printf("cpu main DMA transfer 2 total error: %d\n",total_error);

  *accel_ctrl=2;
  printf("cpu main accel_ctrl write/read long long test error: ");
  printf("%d\n",2-*accel_st); 
  volatile short *sp=(volatile short *)accel_ctrl;
  *sp=3;
  printf("cpu main accel_ctrl write/read short test error: ");
  printf("%d\n",3-*accel_st);
  volatile char *cp=(volatile char *)accel_ctrl;
  *cp=4;
  printf("cpu main accel_ctrl write/read char test 1 error: ");
  printf("%d\n",4-*accel_st);
  printf("cpu main accel_ctrl write/read char test 2 error: %d\n",4-*accel_st);

  *accel_x=0x33;
  *accel_x=0x34;
  *accel_x=0x35;
  *accel_x=0x36;

  *dma_sr=(volatile long long*)((long)accel_z & 0x1fffffff);
  *dma_dr=(volatile long long*)((long)output & 0x1fffffff);
  *dma_len=32;
  clobber();

  printf("cpu main accel_x write + accel_z DMA transfer test 1 error: %d\n",0x33-output[0]);
  printf("cpu main accel_x write + accel_z DMA transfer test 2 error: %d\n",0x34-output[4]);
  printf("cpu main accel_x write + accel_z DMA transfer test 3 error: %d\n",0x35-output[8]);
  printf("cpu main accel_x write + accel_z DMA transfer test 4 error: %d\n",0x36-output[12]);

  *accel_w=0x133;
  *accel_w=0x134;
  *accel_w=0x135;
  *accel_w=0x136;
  printf("cpu main accel_w write + accel_z read test 1 error: %d\n",0x133-*accel_z);
  printf("cpu main accel_w write + accel_z read test 2 error: %d\n",0x134-*accel_z);
  printf("cpu main accel_w write + accel_z read test 3 error: %d\n",0x135-*accel_z);
  printf("cpu main accel_w write + accel_z read test 4 error: %d\n",0x136-*accel_z);
#endif

  *accel_ctrl=(volatile long long)0x0f;  // Exit Simulation
  return 0;
}
