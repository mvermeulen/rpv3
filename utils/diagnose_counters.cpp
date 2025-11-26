#include <rocprofiler-sdk/rocprofiler.h>
#include <stdio.h>
#include <vector>
#include <string>

int main() {
    // 1. Initialize
    rocprofiler_context_id_t context;
    rocprofiler_create_context(&context);

    // 2. Query Agents
    std::vector<rocprofiler_agent_id_t> agents;
    rocprofiler_query_available_agents(
        ROCPROFILER_AGENT_INFO_VERSION_0,
        [](rocprofiler_agent_version_t version, const void** agents_arr, size_t num_agents, void* data) {
            auto* vec = static_cast<std::vector<rocprofiler_agent_id_t>*>(data);
            for (size_t i = 0; i < num_agents; i++) {
                const auto* info = static_cast<const rocprofiler_agent_v0_t*>(agents_arr[i]);
                if (info->type == ROCPROFILER_AGENT_TYPE_GPU) {
                    printf("Found GPU Agent:\n");
                    printf("  Name: %s\n", info->name);
                    printf("  Product Name: %s\n", info->product_name);
                    printf("  Model Name: %s\n", info->model_name);
                    printf("  Family ID: %u\n", info->family_id);
                    vec->push_back(info->id);
                }
            }
            return ROCPROFILER_STATUS_SUCCESS;
        },
        sizeof(rocprofiler_agent_v0_t),
        &agents
    );

    if (agents.empty()) {
        printf("No GPU agents found.\n");
        return 1;
    }

    // 3. Iterate Counters for each agent
    for (auto agent_id : agents) {
        printf("\nChecking counters for Agent Handle: %lu\n", agent_id.handle);
        
        size_t counter_count = 0;
        rocprofiler_status_t status = rocprofiler_iterate_agent_supported_counters(
            agent_id,
            [](rocprofiler_agent_id_t agent, rocprofiler_counter_id_t* counters, size_t num_counters, void* user_data) {
                auto* count = static_cast<size_t*>(user_data);
                *count += num_counters;
                
                for (size_t i = 0; i < num_counters; i++) {
                    rocprofiler_counter_info_v0_t info;
                    if (rocprofiler_query_counter_info(counters[i], ROCPROFILER_COUNTER_INFO_VERSION_0, &info) == ROCPROFILER_STATUS_SUCCESS) {
                        printf("  - %s\n", info.name);
                    }
                }
                return ROCPROFILER_STATUS_SUCCESS;
            },
            &counter_count
        );

        if (status != ROCPROFILER_STATUS_SUCCESS) {
            printf("  Error iterating counters: %d (%s)\n", status, rocprofiler_get_status_string(status));
        } else {
            printf("  Total counters found: %zu\n", counter_count);
        }
    }

    return 0;
}
