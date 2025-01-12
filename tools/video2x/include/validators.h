#pragma once

#include <libvideo2x/fsutils.h>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

template <typename T>
void validate_positive(const T& value, const std::string& option_name) {
    if (value < 0) {
        throw po::validation_error(
            po::validation_error::invalid_option_value,
            option_name,
            option_name + " must be positive"
        );
    }
}

template <typename T>
void validate_min(const T& value, const std::string& option_name, const T& min) {
    if (value < min) {
        throw po::validation_error(
            po::validation_error::invalid_option_value,
            option_name,
            option_name + " must be at least " + std::to_string(min)
        );
    }
}

template <typename T>
void validate_max(const T& value, const std::string& option_name, const T& max) {
    if (value > max) {
        throw po::validation_error(
            po::validation_error::invalid_option_value,
            option_name,
            option_name + " must be at most " + std::to_string(max)
        );
    }
}

template <typename T>
void validate_range(const T& value, const std::string& option_name, const T& min, const T& max) {
    if (value < min || value > max) {
        throw po::validation_error(
            po::validation_error::invalid_option_value,
            option_name,
            option_name + " must be in the range [" + std::to_string(min) + ", " +
                std::to_string(max) + "]"
        );
    }
}

template <typename T>
void validate_greater_equal_one(const T& value, const std::string& option_name) {
    if (value < 1) {
        throw po::validation_error(
            po::validation_error::invalid_option_value,
            option_name,
            option_name + " must be greater than or equal to 1"
        );
    }
}

void validate_anime4k_shader_name(const video2x::fsutils::StringType& shader_name);

void validate_realesrgan_model_name(const video2x::fsutils::StringType& model_name);

void validate_realcugan_model_name(const video2x::fsutils::StringType& model_name);

void validate_rife_model_name(const video2x::fsutils::StringType& model_name);
