#include "processor_factory.h"

#include <spdlog/spdlog.h>
#include <utility>

#include "filter_libplacebo.h"
#include "filter_realcugan.h"
#include "filter_realesrgan.h"
#include "interpolator_rife.h"
#include "logger_manager.h"

namespace video2x {
namespace processors {

// Access the singleton instance
ProcessorFactory& ProcessorFactory::instance() {
    static ProcessorFactory factory;

    // Ensure default processors are registered only once
    static bool initialized = false;
    if (!initialized) {
        ProcessorFactory::init_default_processors(factory);
        initialized = true;
    }

    return factory;
}

// Register a processor type and its creator
void ProcessorFactory::register_processor(ProcessorType type, Creator creator) {
    creators[type] = std::move(creator);
}

// Create a processor instance
std::unique_ptr<Processor> ProcessorFactory::create_processor(
    const ProcessorConfig& proc_cfg,
    uint32_t vk_device_index
) const {
    auto it = creators.find(proc_cfg.processor_type);
    if (it == creators.end()) {
        logger()->critical(
            "Processor type not registered: {}", static_cast<int>(proc_cfg.processor_type)
        );
        return nullptr;
    }

    // Call the corresponding creator function
    return it->second(proc_cfg, vk_device_index);
}

// Initialize default processors
void ProcessorFactory::init_default_processors(ProcessorFactory& factory) {
    factory.register_processor(
        ProcessorType::Libplacebo,
        [](const ProcessorConfig& proc_cfg,
           uint32_t vk_device_index) -> std::unique_ptr<Processor> {
            const auto& config = std::get<LibplaceboConfig>(proc_cfg.config);
            if (config.shader_path.empty()) {
                logger()->critical("Shader path must be provided for the libplacebo filter");
                return nullptr;
            }
            if (proc_cfg.width <= 0 || proc_cfg.height <= 0) {
                logger()->critical(
                    "Output width and height must be provided for the libplacebo filter"
                );
                return nullptr;
            }
            return std::make_unique<FilterLibplacebo>(
                vk_device_index,
                std::filesystem::path(config.shader_path),
                proc_cfg.width,
                proc_cfg.height
            );
        }
    );

    factory.register_processor(
        ProcessorType::RealESRGAN,
        [](const ProcessorConfig& proc_cfg,
           uint32_t vk_device_index) -> std::unique_ptr<Processor> {
            const auto& config = std::get<RealESRGANConfig>(proc_cfg.config);
            if (proc_cfg.scaling_factor <= 0) {
                logger()->critical("Scaling factor must be provided for the RealESRGAN filter");
                return nullptr;
            }
            if (config.model_name.empty()) {
                logger()->critical("Model name must be provided for the RealESRGAN filter");
                return nullptr;
            }
            return std::make_unique<FilterRealesrgan>(
                static_cast<int>(vk_device_index),
                config.tta_mode,
                proc_cfg.scaling_factor,
                proc_cfg.noise_level,
                config.model_name
            );
        }
    );

    factory.register_processor(
        ProcessorType::RealCUGAN,
        [](const ProcessorConfig& proc_cfg,
           uint32_t vk_device_index) -> std::unique_ptr<Processor> {
            const auto& config = std::get<RealCUGANConfig>(proc_cfg.config);
            if (proc_cfg.scaling_factor <= 0) {
                logger()->critical("Scaling factor must be provided for the RealCUGAN filter");
                return nullptr;
            }
            if (config.model_name.empty()) {
                logger()->critical("Model name must be provided for the RealCUGAN filter");
                return nullptr;
            }
            return std::make_unique<FilterRealcugan>(
                static_cast<int>(vk_device_index),
                config.tta_mode,
                proc_cfg.scaling_factor,
                proc_cfg.noise_level,
                config.num_threads,
                config.syncgap,
                config.model_name
            );
        }
    );

    factory.register_processor(
        ProcessorType::RIFE,
        [](const ProcessorConfig& proc_cfg,
           uint32_t vk_device_index) -> std::unique_ptr<Processor> {
            const auto& cfg = std::get<RIFEConfig>(proc_cfg.config);
            if (cfg.model_name.empty()) {
                logger()->critical("Model name must be provided for the RIFE filter");
                return nullptr;
            }
            return std::make_unique<InterpolatorRIFE>(
                static_cast<int>(vk_device_index),
                cfg.tta_mode,
                cfg.tta_temporal_mode,
                cfg.uhd_mode,
                cfg.num_threads,
                cfg.model_name
            );
        }
    );
}

}  // namespace processors
}  // namespace video2x
