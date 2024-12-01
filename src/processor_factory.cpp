#include "processor_factory.h"

#include <spdlog/spdlog.h>
#include <utility>

#include "filter_libplacebo.h"
#include "filter_realesrgan.h"
#include "interpolator_rife.h"

// Access the singleton instance
ProcessorFactory &ProcessorFactory::instance() {
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
    const ProcessorConfig *processor_config,
    uint32_t vk_device_index
) const {
    auto it = creators.find(processor_config->processor_type);
    if (it == creators.end()) {
        spdlog::critical(
            "Processor type not registered: {}", static_cast<int>(processor_config->processor_type)
        );
        return nullptr;
    }

    // Call the corresponding creator function
    return it->second(processor_config, vk_device_index);
}

// Initialize default processors
void ProcessorFactory::init_default_processors(ProcessorFactory &factory) {
    factory.register_processor(
        PROCESSOR_LIBPLACEBO,
        [](const ProcessorConfig *config, uint32_t vk_device_index) -> std::unique_ptr<Processor> {
            const auto &cfg = config->config.libplacebo;
            if (!cfg.shader_path) {
                spdlog::critical("Shader path must be provided for the libplacebo filter");
                return nullptr;
            }
            if (config->width <= 0 || config->height <= 0) {
                spdlog::critical(
                    "Output width and height must be provided for the libplacebo filter"
                );
                return nullptr;
            }
            return std::make_unique<FilterLibplacebo>(
                vk_device_index,
                std::filesystem::path(cfg.shader_path),
                config->width,
                config->height
            );
        }
    );

    factory.register_processor(
        PROCESSOR_REALESRGAN,
        [](const ProcessorConfig *config, uint32_t vk_device_index) -> std::unique_ptr<Processor> {
            const auto &cfg = config->config.realesrgan;
            if (config->scaling_factor <= 0) {
                spdlog::critical("Scaling factor must be provided for the RealESRGAN filter");
                return nullptr;
            }
            if (!cfg.model_name) {
                spdlog::critical("Model name must be provided for the RealESRGAN filter");
                return nullptr;
            }
            return std::make_unique<FilterRealesrgan>(
                static_cast<int>(vk_device_index),
                cfg.tta_mode,
                config->scaling_factor,
                cfg.model_name
            );
        }
    );

    factory.register_processor(
        PROCESSOR_RIFE,
        [](const ProcessorConfig *config, uint32_t vk_device_index) -> std::unique_ptr<Processor> {
            const auto &cfg = config->config.rife;
            if (!cfg.model_name) {
                spdlog::critical("Model name must be provided for the RIFE filter");
                return nullptr;
            }
            return std::make_unique<InterpolatorRIFE>(
                static_cast<int>(vk_device_index),
                cfg.tta_mode,
                cfg.tta_temporal_mode,
                cfg.uhd_mode,
                cfg.num_threads,
                cfg.rife_v2,
                cfg.rife_v4,
                cfg.model_name
            );
        }
    );
}
