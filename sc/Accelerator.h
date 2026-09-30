/*
 * FIR Filter Accelerator (16-tap, 16-bit) for HLS
 * Working optimized version - avoids scheduling conflicts
 */
#pragma once
#include "nvhls_pch.h"

SC_MODULE(Accelerator)
{
public:
    sc_in_clk clk;
    sc_in<bool> rst;
    typedef sc_uint<64> Data;
    
    sc_out<sc_uint<8>> st_out;
    Connections::In<sc_uint<8>> ctrl_in;
    Connections::In<sc_uint<64>> w_in;
    Connections::In<sc_uint<64>> x_in;
    Connections::Out<sc_uint<64>> z_out;
    
    static const int TAPS = 16;
    
    SC_HAS_PROCESS(Accelerator);
    Accelerator(sc_module_name name_) : sc_module(name_),
        st_out("st_out"), ctrl_in("ctrl_in"), w_in("w_in"), 
        x_in("x_in"), z_out("z_out")
    {
        SC_THREAD(run);
        sensitive << clk.pos();
        NVHLS_NEG_RESET_SIGNAL_IS(rst);
    }
    
    void run()
    {
        ctrl_in.Reset();
        w_in.Reset();
        x_in.Reset();
        z_out.Reset();
        
        sc_uint<8> st = 0;
        st_out.write(0);
        
        sc_int<16> coef[TAPS];
        sc_int<16> prev_samples[TAPS-1];
        
        #pragma hls_unroll yes
        for (int i = 0; i < TAPS; i++) {
            coef[i] = 0;
        }
        
        #pragma hls_unroll yes
        for (int i = 0; i < TAPS-1; i++) {
            prev_samples[i] = 0;
        }
        
        wait();
        
        while (1)
        {
            sc_uint<8> cmd = st;
            
            // Check for command - update st but DON'T write to st_out here
            if (!ctrl_in.Empty()) {
                cmd = ctrl_in.Pop();
                st = cmd;
            }
            
            // Command 0x10: Load Coefficients
            if (cmd == 0x10) {
                // Read 4 beats of coefficients
                for (int beat = 0; beat < 4; beat++) {
                    sc_uint<64> data = w_in.Pop();
                    coef[beat*4 + 0] = (sc_int<16>)(sc_uint<16>)(data.range(15, 0));
                    coef[beat*4 + 1] = (sc_int<16>)(sc_uint<16>)(data.range(31, 16));
                    coef[beat*4 + 2] = (sc_int<16>)(sc_uint<16>)(data.range(47, 32));
                    coef[beat*4 + 3] = (sc_int<16>)(sc_uint<16>)(data.range(63, 48));
                    wait();
                }
                
                // Echo back coefficients
                for (int beat = 0; beat < 4; beat++) {
                    sc_uint<64> out_data;
                    out_data.range(15, 0) = (sc_uint<16>)coef[beat*4 + 0];
                    out_data.range(31, 16) = (sc_uint<16>)coef[beat*4 + 1];
                    out_data.range(47, 32) = (sc_uint<16>)coef[beat*4 + 2];
                    out_data.range(63, 48) = (sc_uint<16>)coef[beat*4 + 3];
                    z_out.Push(out_data);
                    wait();
                }
                
                // Reset state
                #pragma hls_unroll yes
                for (int i = 0; i < TAPS-1; i++) {
                    prev_samples[i] = 0;
                }
                
                st = 0;
            }
            // Command 0x20: Start new FIR (reset state)
            else if (cmd == 0x20) {
                // Clear previous samples
                #pragma hls_unroll yes
                for (int i = 0; i < TAPS-1; i++) {
                    prev_samples[i] = 0;
                }
                
                // Read 16 inputs
                sc_int<16> inputs[16];
                for (int beat = 0; beat < 4; beat++) {
                    sc_uint<64> data = x_in.Pop();
                    inputs[beat*4 + 0] = (sc_int<16>)(sc_uint<16>)(data.range(15, 0));
                    inputs[beat*4 + 1] = (sc_int<16>)(sc_uint<16>)(data.range(31, 16));
                    inputs[beat*4 + 2] = (sc_int<16>)(sc_uint<16>)(data.range(47, 32));
                    inputs[beat*4 + 3] = (sc_int<16>)(sc_uint<16>)(data.range(63, 48));
                    wait();
                }
                
                // Compute and output in batches of 4
                // FIX: Force Pipelining on the batch loop to achieve II=1 (high throughput)
                #pragma hls_pipeline_init_interval 1
                for (int batch = 0; batch < 4; batch++) {
                    sc_int<16> out0, out1, out2, out3;
                    
                    // Compute 4 outputs in parallel
                    {
                        int n = batch * 4 + 0;
                        sc_int<32> sum = 0;
                        #pragma hls_unroll yes
                        for (int m = 0; m < TAPS; m++) {
                            int idx = n - TAPS + 1 + m;
                            sum += coef[m] * ((idx >= 0) ? inputs[idx] : (sc_int<16>)0);
                        }
                        out0 = (sc_int<16>)sum;
                    }
                    {
                        int n = batch * 4 + 1;
                        sc_int<32> sum = 0;
                        #pragma hls_unroll yes
                        for (int m = 0; m < TAPS; m++) {
                            int idx = n - TAPS + 1 + m;
                            sum += coef[m] * ((idx >= 0) ? inputs[idx] : (sc_int<16>)0);
                        }
                        out1 = (sc_int<16>)sum;
                    }
                    {
                        int n = batch * 4 + 2;
                        sc_int<32> sum = 0;
                        #pragma hls_unroll yes
                        for (int m = 0; m < TAPS; m++) {
                            int idx = n - TAPS + 1 + m;
                            sum += coef[m] * ((idx >= 0) ? inputs[idx] : (sc_int<16>)0);
                        }
                        out2 = (sc_int<16>)sum;
                    }
                    {
                        int n = batch * 4 + 3;
                        sc_int<32> sum = 0;
                        #pragma hls_unroll yes
                        for (int m = 0; m < TAPS; m++) {
                            int idx = n - TAPS + 1 + m;
                            sum += coef[m] * ((idx >= 0) ? inputs[idx] : (sc_int<16>)0);
                        }
                        out3 = (sc_int<16>)sum;
                    }
                    
                    sc_uint<64> out_data;
                    out_data.range(15, 0) = (sc_uint<16>)out0;
                    out_data.range(31, 16) = (sc_uint<16>)out1;
                    out_data.range(47, 32) = (sc_uint<16>)out2;
                    out_data.range(63, 48) = (sc_uint<16>)out3;
                    z_out.Push(out_data);
                    wait();
                }
                
                // Save last 15 samples
                #pragma hls_unroll yes
                for (int i = 0; i < TAPS-1; i++) {
                    prev_samples[i] = inputs[i + 1];
                }
                
                st = 0;
            }
            // Command 0x21: Continue FIR (use prev_samples)
            else if (cmd == 0x21) {
                // Read 16 new inputs
                sc_int<16> inputs[16];
                for (int beat = 0; beat < 4; beat++) {
                    sc_uint<64> data = x_in.Pop();
                    inputs[beat*4 + 0] = (sc_int<16>)(sc_uint<16>)(data.range(15, 0));
                    inputs[beat*4 + 1] = (sc_int<16>)(sc_uint<16>)(data.range(31, 16));
                    inputs[beat*4 + 2] = (sc_int<16>)(sc_uint<16>)(data.range(47, 32));
                    inputs[beat*4 + 3] = (sc_int<16>)(sc_uint<16>)(data.range(63, 48));
                    wait();
                }
                
                // Compute and output in batches
                // FIX: Force Pipelining on the batch loop to achieve II=1 (high throughput)
                #pragma hls_pipeline_init_interval 1
                for (int batch = 0; batch < 4; batch++) {
                    sc_int<16> out0, out1, out2, out3;
                    
                    // Compute 4 outputs with sliding window
                    {
                        int n = batch * 4 + 0;
                        sc_int<32> sum = 0;
                        #pragma hls_unroll yes
                        for (int m = 0; m < TAPS; m++) {
                            int idx = n - TAPS + 1 + m;
                            sc_int<16> sample = (idx < 0) ? prev_samples[TAPS - 1 + idx] : inputs[idx];
                            sum += coef[m] * sample;
                        }
                        out0 = (sc_int<16>)sum;
                    }
                    {
                        int n = batch * 4 + 1;
                        sc_int<32> sum = 0;
                        #pragma hls_unroll yes
                        for (int m = 0; m < TAPS; m++) {
                            int idx = n - TAPS + 1 + m;
                            sc_int<16> sample = (idx < 0) ? prev_samples[TAPS - 1 + idx] : inputs[idx];
                            sum += coef[m] * sample;
                        }
                        out1 = (sc_int<16>)sum;
                    }
                    {
                        int n = batch * 4 + 2;
                        sc_int<32> sum = 0;
                        #pragma hls_unroll yes
                        for (int m = 0; m < TAPS; m++) {
                            int idx = n - TAPS + 1 + m;
                            sc_int<16> sample = (idx < 0) ? prev_samples[TAPS - 1 + idx] : inputs[idx];
                            sum += coef[m] * sample;
                        }
                        out2 = (sc_int<16>)sum;
                    }
                    {
                        int n = batch * 4 + 3;
                        sc_int<32> sum = 0;
                        #pragma hls_unroll yes
                        for (int m = 0; m < TAPS; m++) {
                            int idx = n - TAPS + 1 + m;
                            sc_int<16> sample = (idx < 0) ? prev_samples[TAPS - 1 + idx] : inputs[idx];
                            sum += coef[m] * sample;
                        }
                        out3 = (sc_int<16>)sum;
                    }
                    
                    sc_uint<64> out_data;
                    out_data.range(15, 0) = (sc_uint<16>)out0;
                    out_data.range(31, 16) = (sc_uint<16>)out1;
                    out_data.range(47, 32) = (sc_uint<16>)out2;
                    out_data.range(63, 48) = (sc_uint<16>)out3;
                    z_out.Push(out_data);
                    wait();
                }
                
                // Update previous samples
                #pragma hls_unroll yes
                for (int i = 0; i < TAPS-1; i++) {
                    prev_samples[i] = inputs[i + 1];
                }
                
                st = 0;
            }
            // Passthrough mode (for test transactions)
            else {
                if (!w_in.Empty()) {
                    sc_uint<64> data = w_in.Pop();
                    z_out.Push(data);
                }
                if (!x_in.Empty()) {
                    sc_uint<64> data = x_in.Pop();
                    z_out.Push(data);
                }
            }
            
            // Update st_out ONCE per iteration at the end
            st_out.write(st);
            wait();
        }
    }
};