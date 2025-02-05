/*
 * Copyright (c) 2016 University of Cordoba and University of Illinois
 * All rights reserved.
 *
 * Developed by:    IMPACT Research Group
 *                  University of Cordoba and University of Illinois
 *                  http://impact.crhc.illinois.edu/
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * with the Software without restriction, including without limitation the 
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 *      > Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimers.
 *      > Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimers in the
 *        documentation and/or other materials provided with the distribution.
 *      > Neither the names of IMPACT Research Group, University of Cordoba, 
 *        University of Illinois nor the names of its contributors may be used 
 *        to endorse or promote products derived from this Software without 
 *        specific prior written permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS WITH
 * THE SOFTWARE.
 *
 */

// #include <CL/cl.h>
#include <iostream>
#include <fstream>
#include "AOCLUtils/aocl_utils.h"

using namespace aocl_utils;

#define CL_ERR()                                                                                                       \
    if(clStatus != CL_SUCCESS) {                                                                                       \
        fprintf(stderr, "OpenCL error: %d\n at %s, %d\n", clStatus, __FILE__, __LINE__);                               \
        exit(-1);                                                                                                      \
    }

struct OpenCLSetup {

    cl_context       clContext;
    cl_command_queue clCommandQueue;
    cl_program       clProgram;
    cl_kernel        clKernel_gauss;
    cl_kernel        clKernel_sobel;

    OpenCLSetup(int platform, int device) {
        cl_int  clStatus;
        cl_uint clNumPlatforms;
        clStatus = clGetPlatformIDs(0, NULL, &clNumPlatforms);
        CL_ERR();
        cl_platform_id *clPlatforms = new cl_platform_id[clNumPlatforms];
        clStatus                    = clGetPlatformIDs(clNumPlatforms, clPlatforms, NULL);
        CL_ERR();
        char           clPlatformVendor[128];
        char           clPlatformVersion[128];
        cl_platform_id clPlatform;
        char           clVendorName[128];
        for(int i = 0; i < clNumPlatforms; i++) {
            clStatus =
                clGetPlatformInfo(clPlatforms[i], CL_PLATFORM_VENDOR, 128 * sizeof(char), clPlatformVendor, NULL);
            CL_ERR();
            std::string clVendorName(clPlatformVendor);
            if(clVendorName.find(clVendorName) != std::string::npos) {
                clPlatform = clPlatforms[i];
                if(i == platform)
                    break;
            }
        }
        delete[] clPlatforms;

        cl_uint clNumDevices;
        clStatus = clGetDeviceIDs(clPlatform, CL_DEVICE_TYPE_ALL, 0, NULL, &clNumDevices);
        CL_ERR();
        cl_device_id *clDevices = new cl_device_id[clNumDevices];
        clStatus                = clGetDeviceIDs(clPlatform, CL_DEVICE_TYPE_ALL, clNumDevices, clDevices, NULL);
        CL_ERR();
        clContext = clCreateContext(NULL, clNumDevices, clDevices, NULL, NULL, &clStatus);
        CL_ERR();
        char device_name_[100];
        clGetDeviceInfo(clDevices[device], CL_DEVICE_NAME, 100, &device_name_, NULL);
        std::cerr << device_name_ << "\t";

#ifdef OCL_2_0
        cl_queue_properties prop[] = {0};
        clCommandQueue             = clCreateCommandQueueWithProperties(clContext, clDevices[device], prop, &clStatus);
#else
        clCommandQueue = clCreateCommandQueue(clContext, clDevices[device], 0, &clStatus);
#endif
        CL_ERR();

        std::filebuf clFile;
        clFile.open("CEDT.cl", std::ios::in);
        std::istream in(&clFile);
        std::string  clCode(std::istreambuf_iterator<char>(in), (std::istreambuf_iterator<char>()));

        const char *clSource[] = {clCode.c_str()};
#ifdef FPGA
        std::string binary_file = getBoardBinaryFile("CEDT", clDevices[device]);
        printf("Using AOCX: %s\n", binary_file.c_str());
        clProgram = createProgramFromBinary(clContext, binary_file.c_str(), &clDevices[device], 1);
#else
        clProgram              = clCreateProgramWithSource(clContext, 1, clSource, NULL, &clStatus);
#endif
        CL_ERR();

        char clOptions[50];
//#ifdef OCL_2_0
#if 1
        sprintf(clOptions, "-I. -cl-std=CL2.0");
#else
        sprintf(clOptions, "-I.");
#endif
        //std::cerr << clOptions << "\t";

        clStatus = clBuildProgram(clProgram, 0, NULL, clOptions, NULL, NULL);
        if(clStatus == CL_BUILD_PROGRAM_FAILURE) {
            // Determine the size of the log
            size_t log_size;
            clGetProgramBuildInfo(clProgram, clDevices[device], CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
            // Allocate memory for the log
            char *log = (char *)malloc(log_size);
            // Get the log
            clGetProgramBuildInfo(clProgram, clDevices[device], CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
            // Print the log
            std::cerr << log << "\t";
        }
        CL_ERR();

        clKernel_gauss  = clCreateKernel(clProgram, "gaussian_kernel", &clStatus);
        clKernel_sobel  = clCreateKernel(clProgram, "sobel_kernel", &clStatus);
        CL_ERR();
    }

    void release() {
        clReleaseKernel(clKernel_gauss);
        clReleaseKernel(clKernel_sobel);
        clReleaseProgram(clProgram);
        clReleaseCommandQueue(clCommandQueue);
        clReleaseContext(clContext);
    }
};
