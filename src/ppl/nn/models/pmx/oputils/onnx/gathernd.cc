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

#include "ppl/nn/models/pmx/utils.h"
#include "ppl/nn/models/pmx/oputils/onnx/gathernd.h"
using namespace flatbuffers;

namespace ppl { namespace nn { namespace pmx { namespace onnx {

Offset<GatherNDParam> SerializeGatherNDParam(const ppl::nn::common::GatherNDParam& param, FlatBufferBuilder* builder) {
    return CreateGatherNDParam(*builder, param.batch_dims);
}

void DeserializeGatherNDParam(const GatherNDParam& fb_param, ppl::nn::common::GatherNDParam* param) {
    param->batch_dims = fb_param.batch_dims();
}

}}}} // namespace ppl::nn::pmx::onnx
