// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#ifndef __ST_PPL_KERNEL_RISCV_FP32_FC_VEC128_FC_FP32_VEC128_H_
#define __ST_PPL_KERNEL_RISCV_FP32_FC_VEC128_FC_FP32_VEC128_H_

#include "ppl/kernel/riscv/fp32/fc.h"
#include "ppl/kernel/riscv/common/internal_include.h"

namespace ppl { namespace kernel { namespace riscv {

// forward declare;
class fc_fp32_vec128_manager;

class fc_fp32_vec128_executor final : public fc_executor<float> {
public:
    fc_fp32_vec128_executor() {}
    fc_fp32_vec128_executor(const fc_common_param* fc_param, const float* cvt_filter, const float* bias)
        : fc_executor<float>(fc_param, cvt_filter, bias) {}
    uint64_t cal_temp_buffer_size() override;
    ppl::common::RetCode prepare() override;
    ppl::common::RetCode execute() override;

private:
    void cal_kernel_tunning_param();
    friend fc_fp32_vec128_manager;
};

class fc_fp32_vec128_manager final : public fc_manager<float> {
public:
    fc_fp32_vec128_manager() {}
    fc_fp32_vec128_manager(const fc_common_param& param, ppl::common::Allocator* allocator)
        : fc_manager<float>(param, allocator) {}
    ppl::common::RetCode gen_cvt_weights(const float* filter, const float* bias) override;
    fc_executor<float>* gen_executor() override;
};

}}}; // namespace ppl::kernel::riscv

#endif
