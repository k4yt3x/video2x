#pragma once

#include <functional>
#include <memory>
#include <unordered_map>

#include "processor.h"

namespace video2x {
namespace processors {

// Processor Factory Class
class ProcessorFactory {
   public:
    using Creator = std::function<std::unique_ptr<Processor>(const ProcessorConfig&, uint32_t)>;

    // Singleton instance accessor
    static ProcessorFactory& instance();

    // Register a processor type with its creation function
    void register_processor(ProcessorType type, Creator creator);

    // Create a processor instance based on configuration
    std::unique_ptr<Processor>
    create_processor(const ProcessorConfig& proc_cfg, uint32_t vk_device_index) const;

   private:
    // Private constructor for Singleton
    ProcessorFactory() = default;

    // Map of processor types to their creation functions
    std::unordered_map<ProcessorType, Creator> creators;

    // Static initializer for default processors
    static void init_default_processors(ProcessorFactory& factory);
};

}  // namespace processors
}  // namespace video2x
