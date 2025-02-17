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

#ifndef NDEBUG
#include <set>
#endif

#include "ppl/nn/common/logger.h"
#include "ppl/nn/runtime/sequential_scheduler.h"
#include "ppl/nn/runtime/scheduler_common.h"
using namespace std;
using namespace ppl::common;

namespace ppl { namespace nn {

RetCode SequentialScheduler::Init(const ir::GraphTopo* topo, const RuntimeAuxInfo* aux_info, RuntimeGraphResource* g) {
    graph_ = g;
    topo_ = topo;
    aux_info_ = aux_info;
    return RC_SUCCESS;
}

class SchedulerAcquireObject final : public InputOutputInfo::AcquireObject {
public:
    SchedulerAcquireObject(const ir::GraphTopo* topo, vector<EdgeObject*>* edgeid2object,
                           ObjectPool<TensorImpl>* tensor_pool, ObjectPool<TensorSequence>* tensor_sequence_pool)
        : device_(nullptr), topo_(topo), edgeid2object_(edgeid2object), tensor_pool_(tensor_pool)
        , tensor_sequence_pool_(tensor_sequence_pool) {}

    void SetDevice(Device* d) {
        device_ = d;
    }

    EdgeObject* Acquire(edgeid_t eid, uint32_t etype) override {
        if (eid >= edgeid2object_->size()) {
            return nullptr;
        }

        auto object = edgeid2object_->at(eid);
        if (!object) {
            auto edge = topo_->GetEdgeById(eid);

            if (etype == EdgeObject::T_TENSOR) {
                auto tensor = tensor_pool_->Alloc(edge, TENSORTYPE_NORMAL);
                tensor->SetDevice(device_);
                object = tensor;
            } else if (etype == EdgeObject::T_TENSOR_SEQUENCE) {
                object = tensor_sequence_pool_->Alloc(edge);
            } else if (etype == EdgeObject::T_EDGE_OBJECT) {
                return nullptr;
            } else {
                LOG(ERROR) << "invalid object type[" << etype << "] of edge[" << edge->GetName() << "]";
                return nullptr;
            }

            if (!object) {
                LOG(ERROR) << "create output object[" << edge->GetName() << "] failed, oom";
                return nullptr;
            }
            edgeid2object_->at(eid) = object;
        }
        return object;
    };

private:
    Device* device_;
    const ir::GraphTopo* topo_;
    vector<EdgeObject*>* edgeid2object_;
    ObjectPool<TensorImpl>* tensor_pool_;
    ObjectPool<TensorSequence>* tensor_sequence_pool_;
};

RetCode SequentialScheduler::Run(Profiler* profiler) {
    auto release_object_func = [this](EdgeObject* object, nodeid_t user) -> RetCode {
        auto eid = object->GetEdge()->GetId();
        if (aux_info_->tensor_last_consumer[eid] == user) {
            auto obj = graph_->edgeid2object[eid];
            if (obj->GetObjectType() == EdgeObject::T_TENSOR) {
                tensor_pool_.Free(static_cast<TensorImpl*>(obj));
            } else if (obj->GetObjectType() == EdgeObject::T_TENSOR_SEQUENCE) {
                tensor_sequence_pool_.Free(static_cast<TensorSequence*>(obj));
            } else {
                LOG(ERROR) << "invalid edge object type[" << obj->GetObjectType() << "]";
                return RC_INVALID_VALUE;
            }
            graph_->edgeid2object[eid] = nullptr;
        }
        return RC_SUCCESS;
    };

#ifndef NDEBUG
    set<edgeid_t> edges_before;
    for (uint32_t i = 0; i < graph_->edgeid2object.size(); ++i) {
        if (graph_->edgeid2object[i]) {
            edges_before.insert(i);
        }
    }
#endif

    KernelExecContext ctx;
    ctx.SetProfilingFlag(profiler->IsProfilingEnabled());

    SchedulerAcquireObject getter(topo_, &graph_->edgeid2object, &tensor_pool_, &tensor_sequence_pool_);
    ctx.SetAcquireObject(&getter);

    for (auto x = aux_info_->sorted_nodes.begin(); x != aux_info_->sorted_nodes.end(); ++x) {
        auto kernel = graph_->nodeid2kernel[*x].get();
        ctx.SetNode(kernel->GetNode());
        getter.SetDevice(kernel->GetDevice());

        auto status = utils::ExecuteKernel(kernel, &ctx, release_object_func, profiler);
        if (status != RC_SUCCESS) {
            LOG(ERROR) << "execute kernel[" << kernel->GetName() << "] failed: " << GetRetCodeStr(status);
            return status;
        }
    }

#ifndef NDEBUG
    set<edgeid_t> edges_after;
    for (uint32_t i = 0; i < graph_->edgeid2object.size(); ++i) {
        if (graph_->edgeid2object[i]) {
            edges_after.insert(i);
        }
    }

    vector<edgeid_t> diff_before2after(edges_before.size());
    auto end_iter = std::set_difference(edges_before.begin(), edges_before.end(), edges_after.begin(),
                                        edges_after.end(), diff_before2after.begin());
    diff_before2after.resize(end_iter - diff_before2after.begin());
    if (!diff_before2after.empty()) {
        LOG(ERROR) << "edge(s) in `before` but not in `after`:";
        for (auto x = diff_before2after.begin(); x != diff_before2after.end(); ++x) {
            auto edge = topo_->GetEdgeById(*x);
            LOG(ERROR) << " " << edge->GetName();
        }
    }

    vector<edgeid_t> diff_after2before(edges_after.size());
    end_iter = std::set_difference(edges_after.begin(), edges_after.end(), edges_before.begin(), edges_before.end(),
                                   diff_after2before.begin());
    diff_after2before.resize(end_iter - diff_after2before.begin());
    if (!diff_after2before.empty()) {
        LOG(ERROR) << "edge(s) in `after` but not in `before`:";
        for (auto x = diff_after2before.begin(); x != diff_after2before.end(); ++x) {
            auto edge = topo_->GetEdgeById(*x);
            LOG(ERROR) << " " << edge->GetName();
        }
    }

    if (!diff_before2after.empty() || !diff_after2before.empty()) {
        return RC_OTHER_ERROR;
    }
#endif

    return RC_SUCCESS;
}

}} // namespace ppl::nn
