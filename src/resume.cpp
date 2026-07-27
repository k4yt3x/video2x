#include "libvideo2x/resume.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <system_error>
#include <type_traits>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

extern "C" {
#include <libavutil/imgutils.h>
}

#include "avutils.h"
#include "fsutils.h"
#include "logger_manager.h"
#include "resume_session.h"

namespace video2x {
namespace resume {
namespace {

constexpr uint64_t FNV_OFFSET = 14695981039346656037ULL;
constexpr uint64_t FNV_PRIME = 1099511628211ULL;
constexpr uint64_t UNIT_MAGIC = 0x55454d5553455232ULL;
constexpr uint32_t UNIT_VERSION = 1;
constexpr int64_t CHECKPOINT_INTERVAL = 30;

uint64_t hash_bytes(uint64_t hash, const void* data, size_t size) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    for (size_t index = 0; index < size; ++index) {
        hash = (hash ^ bytes[index]) * FNV_PRIME;
    }
    return hash;
}

std::string hex_encode(const std::string& value) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string encoded;
    encoded.reserve(value.size() * 2);
    for (unsigned char character : value) {
        encoded.push_back(digits[character >> 4]);
        encoded.push_back(digits[character & 15]);
    }
    return encoded;
}

bool hex_decode(const std::string& value, std::string& decoded) {
    if (value.size() % 2 != 0) {
        return false;
    }
    auto nibble = [](char character) -> int {
        if (character >= '0' && character <= '9') return character - '0';
        if (character >= 'a' && character <= 'f') return character - 'a' + 10;
        if (character >= 'A' && character <= 'F') return character - 'A' + 10;
        return -1;
    };
    decoded.clear();
    decoded.reserve(value.size() / 2);
    for (size_t index = 0; index < value.size(); index += 2) {
        int high = nibble(value[index]);
        int low = nibble(value[index + 1]);
        if (high < 0 || low < 0) {
            return false;
        }
        decoded.push_back(static_cast<char>((high << 4) | low));
    }
    return true;
}

std::string path_string(const std::filesystem::path& path) {
    return path.u8string();
}

std::filesystem::path normalized_path(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::path normalized = std::filesystem::weakly_canonical(path, error);
    if (!error) {
        return normalized;
    }
    normalized = std::filesystem::absolute(path, error);
    return error ? path.lexically_normal() : normalized.lexically_normal();
}

fsutils::StringType native_string(const std::string& value) {
    return std::filesystem::u8path(value).native();
}

int64_t file_mtime(const std::filesystem::path& path, std::error_code& error) {
    auto time = std::filesystem::last_write_time(path, error);
    if (error) {
        return 0;
    }
    return std::chrono::duration_cast<std::chrono::nanoseconds>(time.time_since_epoch()).count();
}

uint64_t fingerprint_file(const std::filesystem::path& path, std::string& error) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error = "Unable to read original input";
        return 0;
    }
    stream.seekg(0, std::ios::end);
    std::streamoff size = stream.tellg();
    if (size < 0) {
        error = "Unable to determine original input size";
        return 0;
    }
    constexpr std::streamoff sample_size = 1024 * 1024;
    std::array<char, static_cast<size_t>(sample_size)> buffer{};
    uint64_t hash = hash_bytes(FNV_OFFSET, &size, sizeof(size));
    for (std::streamoff offset : {std::streamoff(0), std::max(std::streamoff(0), size - sample_size)}) {
        stream.clear();
        stream.seekg(offset);
        stream.read(buffer.data(), std::min(sample_size, size - offset));
        std::streamsize count = stream.gcount();
        hash = hash_bytes(hash, buffer.data(), static_cast<size_t>(count));
    }
    return hash;
}

bool flush_file(FILE* file) {
    if (std::fflush(file) != 0) {
        return false;
    }
#ifdef _WIN32
    return _commit(_fileno(file)) == 0;
#else
    return fsync(fileno(file)) == 0;
#endif
}

bool atomic_replace(
    const std::filesystem::path& temporary,
    const std::filesystem::path& destination,
    std::string& error
) {
#ifdef _WIN32
    if (!MoveFileExW(
            temporary.c_str(),
            destination.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
        )) {
        error = "Atomic file replacement failed with Windows error " + std::to_string(GetLastError());
        return false;
    }
#else
    if (::rename(temporary.c_str(), destination.c_str()) != 0) {
        error = "Atomic file replacement failed: " + std::string(std::strerror(errno));
        return false;
    }
#endif
    return true;
}

std::filesystem::path manifest_path(const std::filesystem::path& artifact) {
    return artifact / "manifest.v2x";
}

std::filesystem::path unit_path(const std::filesystem::path& artifact, int64_t source_index) {
    std::ostringstream name;
    name << "unit-" << std::setw(12) << std::setfill('0') << source_index << ".v2xf";
    return artifact / name.str();
}

std::filesystem::path tail_path(const std::filesystem::path& artifact) {
    return artifact / "tail.v2xf";
}

std::string processor_name(processors::ProcessorType type) {
    switch (type) {
        case processors::ProcessorType::Libplacebo: return "libplacebo";
        case processors::ProcessorType::RealESRGAN: return "realesrgan";
        case processors::ProcessorType::RealCUGAN: return "realcugan";
        case processors::ProcessorType::RIFE: return "rife";
        default: return "unknown";
    }
}

std::string model_name(const processors::ProcessorConfig& config) {
    switch (config.processor_type) {
        case processors::ProcessorType::Libplacebo:
            return fsutils::wstring_to_u8string(std::get<processors::LibplaceboConfig>(config.config).shader_path);
        case processors::ProcessorType::RealESRGAN:
            return fsutils::wstring_to_u8string(std::get<processors::RealESRGANConfig>(config.config).model_name);
        case processors::ProcessorType::RealCUGAN:
            return fsutils::wstring_to_u8string(std::get<processors::RealCUGANConfig>(config.config).model_name);
        case processors::ProcessorType::RIFE:
            return fsutils::wstring_to_u8string(std::get<processors::RIFEConfig>(config.config).model_name);
        default:
            return {};
    }
}

void add_value(std::ostringstream& output, const std::string& key, int64_t value) {
    output << key << '=' << value << '\n';
}

void add_string(std::ostringstream& output, const std::string& key, const std::string& value) {
    output << key << '=' << hex_encode(value) << '\n';
}

std::string serialize_manifest(
    const ResumeInfo& info,
    uintmax_t input_size,
    int64_t input_mtime,
    uint64_t input_fingerprint,
    int64_t tail_frames
) {
    const auto& processor = info.processor_config;
    const auto& encoder = info.encoder_config;
    std::ostringstream body;
    add_string(body, "signature", MANIFEST_SIGNATURE);
    add_value(body, "version", info.version);
    add_value(body, "status", static_cast<int64_t>(info.status));
    add_string(body, "input", path_string(info.input_path));
    add_value(body, "input_size", static_cast<int64_t>(input_size));
    add_value(body, "input_mtime", input_mtime);
    add_value(body, "input_fingerprint", static_cast<int64_t>(input_fingerprint));
    add_string(body, "output", path_string(info.output_path));
    add_value(body, "processor_type", static_cast<int64_t>(processor.processor_type));
    add_value(body, "width", processor.width);
    add_value(body, "height", processor.height);
    add_value(body, "scaling_factor", processor.scaling_factor);
    add_value(body, "noise_level", processor.noise_level);
    add_value(body, "frame_rate_multiplier", processor.frm_rate_mul);
    body << "scene_threshold=" << std::setprecision(9) << processor.scn_det_thresh << '\n';
    add_string(body, "processor_model", model_name(processor));
    int64_t tta = 0, temporal = 0, uhd = 0, threads = 0, syncgap = 0;
    if (processor.processor_type == processors::ProcessorType::RealESRGAN) {
        tta = std::get<processors::RealESRGANConfig>(processor.config).tta_mode;
    } else if (processor.processor_type == processors::ProcessorType::RealCUGAN) {
        const auto& value = std::get<processors::RealCUGANConfig>(processor.config);
        tta = value.tta_mode;
        threads = value.num_threads;
        syncgap = value.syncgap;
    } else if (processor.processor_type == processors::ProcessorType::RIFE) {
        const auto& value = std::get<processors::RIFEConfig>(processor.config);
        tta = value.tta_mode;
        temporal = value.tta_temporal_mode;
        uhd = value.uhd_mode;
        threads = value.num_threads;
    }
    add_value(body, "tta", tta);
    add_value(body, "tta_temporal", temporal);
    add_value(body, "uhd", uhd);
    add_value(body, "processor_threads", threads);
    add_value(body, "syncgap", syncgap);
    add_string(body, "codec", encoder.codec);
    add_value(body, "recalculate_pts", encoder.recalculate_pts);
    add_value(body, "copy_audio", encoder.copy_audio_streams);
    add_value(body, "copy_subtitles", encoder.copy_subtitle_streams);
    add_value(body, "pixel_format", encoder.pix_fmt);
    add_value(body, "bit_rate", encoder.bit_rate);
    add_value(body, "rc_buffer_size", encoder.rc_buffer_size);
    add_value(body, "rc_min_rate", encoder.rc_min_rate);
    add_value(body, "rc_max_rate", encoder.rc_max_rate);
    add_value(body, "qmin", encoder.qmin);
    add_value(body, "qmax", encoder.qmax);
    add_value(body, "gop_size", encoder.gop_size);
    add_value(body, "max_b_frames", encoder.max_b_frames);
    add_value(body, "keyint_min", encoder.keyint_min);
    add_value(body, "refs", encoder.refs);
    add_value(body, "encoder_threads", encoder.thread_count);
    add_value(body, "delay", encoder.delay);
    add_value(body, "extra_option_count", static_cast<int64_t>(encoder.extra_opts.size()));
    for (size_t index = 0; index < encoder.extra_opts.size(); ++index) {
        add_string(body, "extra_option_key_" + std::to_string(index), encoder.extra_opts[index].first);
        add_string(body, "extra_option_value_" + std::to_string(index), encoder.extra_opts[index].second);
    }
    add_value(body, "vk_device_index", info.vk_device_index);
    add_value(body, "hw_device_type", info.hw_device_type);
    add_value(body, "video_stream_index", info.video_stream_index);
    add_value(body, "source_width", info.source_width);
    add_value(body, "source_height", info.source_height);
    add_value(body, "source_pixel_format", info.source_pixel_format);
    add_value(body, "source_time_base_num", info.source_time_base.num);
    add_value(body, "source_time_base_den", info.source_time_base.den);
    add_value(body, "source_frame_rate_num", info.source_frame_rate.num);
    add_value(body, "source_frame_rate_den", info.source_frame_rate.den);
    add_value(body, "total_source_frames", info.total_source_frames);
    add_value(body, "total_source_frames_exact", info.total_source_frames_exact);
    add_value(body, "completed_source_frames", info.completed_source_frames);
    add_value(body, "completed_output_frames", info.completed_output_frames);
    add_value(body, "tail_frames", tail_frames);
    add_value(body, "checkpoint_timestamp", info.checkpoint_timestamp);
    std::string value = body.str();
    uint64_t hash = hash_bytes(FNV_OFFSET, value.data(), value.size());
    value += "manifest_hash=" + std::to_string(hash) + '\n';
    return value;
}

bool parse_integer(
    const std::map<std::string, std::string>& values,
    const std::string& key,
    int64_t minimum,
    int64_t maximum,
    int64_t& result
) {
    auto iterator = values.find(key);
    if (iterator == values.end()) return false;
    try {
        size_t read = 0;
        long long value = std::stoll(iterator->second, &read);
        if (read != iterator->second.size() || value < minimum || value > maximum) return false;
        result = value;
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_string(
    const std::map<std::string, std::string>& values,
    const std::string& key,
    std::string& result
) {
    auto iterator = values.find(key);
    return iterator != values.end() && hex_decode(iterator->second, result);
}

bool load_manifest(
    const std::filesystem::path& artifact,
    ResumeInfo& info,
    uintmax_t& input_size,
    int64_t& input_mtime,
    uint64_t& input_fingerprint,
    int64_t& tail_frames,
    std::string& error
) {
    std::ifstream stream(manifest_path(artifact), std::ios::binary);
    if (!stream) {
        error = "Resume manifest is missing or unreadable";
        return false;
    }
    std::ostringstream raw_stream;
    raw_stream << stream.rdbuf();
    std::string raw = raw_stream.str();
    size_t hash_position = raw.rfind("manifest_hash=");
    if (hash_position == std::string::npos || hash_position == 0) {
        error = "Resume manifest checksum is missing";
        return false;
    }
    std::string body = raw.substr(0, hash_position);
    std::string hash_text = raw.substr(hash_position + 14);
    if (!hash_text.empty() && hash_text.back() == '\n') hash_text.pop_back();
    try {
        if (std::stoull(hash_text) != hash_bytes(FNV_OFFSET, body.data(), body.size())) {
            error = "Resume manifest checksum is invalid";
            return false;
        }
    } catch (...) {
        error = "Resume manifest checksum is invalid";
        return false;
    }

    std::map<std::string, std::string> values;
    std::istringstream lines(body);
    std::string line;
    while (std::getline(lines, line)) {
        size_t separator = line.find('=');
        if (separator == std::string::npos || separator == 0 ||
            !values.emplace(line.substr(0, separator), line.substr(separator + 1)).second) {
            error = "Resume manifest contains an invalid or duplicate field";
            return false;
        }
    }

    std::string value;
    int64_t number = 0;
    if (!parse_string(values, "signature", value) || value != MANIFEST_SIGNATURE) {
        error = "Resume manifest signature is invalid";
        return false;
    }
    if (!parse_integer(values, "version", 1, std::numeric_limits<uint32_t>::max(), number)) {
        error = "Resume manifest version is invalid";
        return false;
    }
    info.version = static_cast<uint32_t>(number);
    if (info.version != MANIFEST_VERSION) {
        error = info.version > MANIFEST_VERSION ? "Resume manifest uses a newer unsupported version"
                                                : "Resume manifest version is unsupported";
        return false;
    }
    if (!parse_integer(values, "status", 0, 1, number)) return error = "Invalid resume status", false;
    info.status = static_cast<Status>(number);
    if (!parse_string(values, "input", value)) return error = "Invalid input path", false;
    info.input_path = std::filesystem::u8path(value);
    if (!parse_string(values, "output", value)) return error = "Invalid output path", false;
    info.output_path = std::filesystem::u8path(value);
    if (!parse_integer(values, "input_size", 0, std::numeric_limits<int64_t>::max(), number))
        return error = "Invalid input size", false;
    input_size = static_cast<uintmax_t>(number);
    if (!parse_integer(values, "input_mtime", std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max(), input_mtime))
        return error = "Invalid input timestamp", false;
    if (!parse_integer(values, "input_fingerprint", std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max(), number))
        return error = "Invalid input fingerprint", false;
    input_fingerprint = static_cast<uint64_t>(number);

    auto read_int = [&](const char* key, int64_t minimum, int64_t maximum, auto& destination) {
        int64_t parsed = 0;
        if (!parse_integer(values, key, minimum, maximum, parsed)) return false;
        destination = static_cast<std::decay_t<decltype(destination)>>(parsed);
        return true;
    };
    auto& processor = info.processor_config;
    if (!read_int("processor_type", 1, 4, processor.processor_type) ||
        !read_int("width", 0, 65536, processor.width) ||
        !read_int("height", 0, 65536, processor.height) ||
        !read_int("scaling_factor", 0, 16, processor.scaling_factor) ||
        !read_int("noise_level", -1, 16, processor.noise_level) ||
        !read_int("frame_rate_multiplier", 0, 64, processor.frm_rate_mul)) {
        error = "Invalid processor configuration";
        return false;
    }
    try {
        processor.scn_det_thresh = std::stof(values.at("scene_threshold"));
    } catch (...) {
        error = "Invalid scene detection threshold";
        return false;
    }
    if (!std::isfinite(processor.scn_det_thresh) || processor.scn_det_thresh < 0.0f ||
        processor.scn_det_thresh > 100.0f || !parse_string(values, "processor_model", value)) {
        error = "Invalid processor model configuration";
        return false;
    }
    int64_t tta = 0, temporal = 0, uhd = 0, threads = 0, syncgap = 0;
    if (!parse_integer(values, "tta", 0, 1, tta) ||
        !parse_integer(values, "tta_temporal", 0, 1, temporal) ||
        !parse_integer(values, "uhd", 0, 1, uhd) ||
        !parse_integer(values, "processor_threads", 0, INT_MAX, threads) ||
        !parse_integer(values, "syncgap", 0, INT_MAX, syncgap)) {
        error = "Invalid processor options";
        return false;
    }
    switch (processor.processor_type) {
        case processors::ProcessorType::Libplacebo:
            if (processor.width <= 0 || processor.height <= 0) {
                error = "Invalid libplacebo output dimensions";
                return false;
            }
            processor.config = processors::LibplaceboConfig{native_string(value)};
            break;
        case processors::ProcessorType::RealESRGAN:
            if (processor.scaling_factor <= 0) {
                error = "Invalid Real-ESRGAN scaling factor";
                return false;
            }
            processor.config = processors::RealESRGANConfig{tta != 0, native_string(value)};
            break;
        case processors::ProcessorType::RealCUGAN:
            if (processor.scaling_factor <= 0) {
                error = "Invalid Real-CUGAN scaling factor";
                return false;
            }
            processor.config = processors::RealCUGANConfig{
                tta != 0, static_cast<int>(threads), static_cast<int>(syncgap), native_string(value)
            };
            break;
        case processors::ProcessorType::RIFE:
            if (processor.frm_rate_mul < 2) {
                error = "Invalid interpolation multiplier";
                return false;
            }
            processor.config = processors::RIFEConfig{
                tta != 0, temporal != 0, uhd != 0, static_cast<int>(threads), native_string(value)
            };
            break;
        default:
            error = "Unsupported processor type";
            return false;
    }
    auto& encoder = info.encoder_config;
    if (!parse_string(values, "codec", encoder.codec) ||
        !read_int("recalculate_pts", 0, 1, encoder.recalculate_pts) ||
        !read_int("copy_audio", 0, 1, encoder.copy_audio_streams) ||
        !read_int("copy_subtitles", 0, 1, encoder.copy_subtitle_streams) ||
        !read_int("pixel_format", -1, AV_PIX_FMT_NB - 1, encoder.pix_fmt) ||
        !read_int("bit_rate", 0, std::numeric_limits<int64_t>::max(), encoder.bit_rate) ||
        !read_int("rc_buffer_size", 0, INT_MAX, encoder.rc_buffer_size) ||
        !read_int("rc_min_rate", 0, INT_MAX, encoder.rc_min_rate) ||
        !read_int("rc_max_rate", 0, INT_MAX, encoder.rc_max_rate) ||
        !read_int("qmin", -1, INT_MAX, encoder.qmin) ||
        !read_int("qmax", -1, INT_MAX, encoder.qmax) ||
        !read_int("gop_size", -1, INT_MAX, encoder.gop_size) ||
        !read_int("max_b_frames", -1, INT_MAX, encoder.max_b_frames) ||
        !read_int("keyint_min", -1, INT_MAX, encoder.keyint_min) ||
        !read_int("refs", -1, INT_MAX, encoder.refs) ||
        !read_int("encoder_threads", 0, INT_MAX, encoder.thread_count) ||
        !read_int("delay", -1, INT_MAX, encoder.delay)) {
        error = "Invalid encoder configuration";
        return false;
    }
    int64_t option_count = 0;
    if (!parse_integer(values, "extra_option_count", 0, 1024, option_count)) {
        error = "Invalid encoder option count";
        return false;
    }
    for (int64_t index = 0; index < option_count; ++index) {
        std::string key, option_value;
        if (!parse_string(values, "extra_option_key_" + std::to_string(index), key) ||
            !parse_string(values, "extra_option_value_" + std::to_string(index), option_value)) {
            error = "Invalid encoder option";
            return false;
        }
        encoder.extra_opts.emplace_back(std::move(key), std::move(option_value));
    }
    if (!read_int("vk_device_index", 0, UINT32_MAX, info.vk_device_index) ||
        !read_int("hw_device_type", 0, INT_MAX, info.hw_device_type) ||
        !read_int("video_stream_index", 0, INT_MAX, info.video_stream_index) ||
        !read_int("source_width", 1, INT_MAX, info.source_width) ||
        !read_int("source_height", 1, INT_MAX, info.source_height) ||
        !read_int("source_pixel_format", -1, AV_PIX_FMT_NB - 1, info.source_pixel_format) ||
        !read_int("source_time_base_num", 1, INT_MAX, info.source_time_base.num) ||
        !read_int("source_time_base_den", 1, INT_MAX, info.source_time_base.den) ||
        !read_int("source_frame_rate_num", 1, INT_MAX, info.source_frame_rate.num) ||
        !read_int("source_frame_rate_den", 1, INT_MAX, info.source_frame_rate.den) ||
        !read_int("total_source_frames", 0, std::numeric_limits<int64_t>::max(), info.total_source_frames) ||
        !read_int("total_source_frames_exact", 0, 1, info.total_source_frames_exact) ||
        !read_int("completed_source_frames", 0, std::numeric_limits<int64_t>::max(), info.completed_source_frames) ||
        !read_int("completed_output_frames", 0, std::numeric_limits<int64_t>::max(), info.completed_output_frames) ||
        !read_int("tail_frames", 0, INT_MAX, tail_frames) ||
        !read_int("checkpoint_timestamp", 0, std::numeric_limits<int64_t>::max(), info.checkpoint_timestamp)) {
        error = "Invalid source or checkpoint metadata";
        return false;
    }
    if (info.total_source_frames_exact && info.total_source_frames > 0 &&
        info.completed_source_frames > info.total_source_frames) {
        error = "Checkpoint exceeds source frame count";
        return false;
    }
    info.artifact_path = artifact;
    info.processor_name = processor_name(processor.processor_type);
    info.model_name = model_name(processor);
    return true;
}

bool write_manifest(
    const ResumeInfo& info,
    uintmax_t input_size,
    int64_t input_mtime,
    uint64_t input_fingerprint,
    int64_t tail_frames,
    std::string& error
) {
    std::string contents =
        serialize_manifest(info, input_size, input_mtime, input_fingerprint, tail_frames);
    auto destination = manifest_path(info.artifact_path);
    auto temporary = destination;
    temporary += ".tmp";
#ifdef _WIN32
    FILE* file = _wfopen(temporary.c_str(), L"wb");
#else
    FILE* file = std::fopen(temporary.c_str(), "wb");
#endif
    if (!file) {
        error = "Unable to create resume manifest";
        return false;
    }
    bool written = std::fwrite(contents.data(), 1, contents.size(), file) == contents.size();
    bool flushed = written && flush_file(file);
    bool closed = std::fclose(file) == 0;
    if (!flushed || !closed) {
        error = "Unable to flush resume manifest";
        return false;
    }
    return atomic_replace(temporary, destination, error);
}

template <typename T>
bool write_hashed(FILE* file, const T& value, uint64_t& hash) {
    if (std::fwrite(&value, sizeof(value), 1, file) != 1) return false;
    hash = hash_bytes(hash, &value, sizeof(value));
    return true;
}

template <typename T>
bool read_hashed(FILE* file, T& value, uint64_t& hash) {
    if (std::fread(&value, sizeof(value), 1, file) != 1) return false;
    hash = hash_bytes(hash, &value, sizeof(value));
    return true;
}

bool write_unit(
    const std::filesystem::path& destination,
    const std::vector<AVFrame*>& frames,
    std::string& error
) {
    auto temporary = destination;
    temporary += ".tmp";
#ifdef _WIN32
    FILE* file = _wfopen(temporary.c_str(), L"wb");
#else
    FILE* file = std::fopen(temporary.c_str(), "wb");
#endif
    if (!file) return error = "Unable to create checkpoint frame file", false;
    uint64_t hash = FNV_OFFSET;
    uint32_t count = static_cast<uint32_t>(frames.size());
    bool ok = write_hashed(file, UNIT_MAGIC, hash) && write_hashed(file, UNIT_VERSION, hash) &&
              write_hashed(file, count, hash);
    for (AVFrame* frame : frames) {
        int32_t format = frame ? frame->format : AV_PIX_FMT_NONE;
        int32_t width = frame ? frame->width : 0;
        int32_t height = frame ? frame->height : 0;
        int64_t pts = frame ? frame->pts : AV_NOPTS_VALUE;
        int32_t size = frame ? av_image_get_buffer_size(
                                   static_cast<AVPixelFormat>(format), width, height, 1
                               )
                             : -1;
        if (!frame || format < 0 || width <= 0 || height <= 0 || size <= 0) {
            ok = false;
            break;
        }
        std::vector<uint8_t> data(static_cast<size_t>(size));
        if (av_image_copy_to_buffer(
                data.data(),
                size,
                frame->data,
                frame->linesize,
                static_cast<AVPixelFormat>(format),
                width,
                height,
                1
            ) != size) {
            ok = false;
            break;
        }
        ok = write_hashed(file, format, hash) && write_hashed(file, width, hash) &&
             write_hashed(file, height, hash) && write_hashed(file, pts, hash) &&
             write_hashed(file, size, hash) &&
             std::fwrite(data.data(), 1, data.size(), file) == data.size();
        hash = hash_bytes(hash, data.data(), data.size());
        if (!ok) break;
    }
    if (ok) ok = std::fwrite(&hash, sizeof(hash), 1, file) == 1 && flush_file(file);
    bool closed = std::fclose(file) == 0;
    if (!ok || !closed) return error = "Unable to write checkpoint frame file", false;
    return atomic_replace(temporary, destination, error);
}

bool read_unit(
    const std::filesystem::path& path,
    std::vector<AVFrame*>& frames,
    std::string& error
) {
#ifdef _WIN32
    FILE* file = _wfopen(path.c_str(), L"rb");
#else
    FILE* file = std::fopen(path.c_str(), "rb");
#endif
    if (!file) return error = "Checkpoint frame file is missing", false;
    uint64_t hash = FNV_OFFSET, magic = 0;
    uint32_t version = 0, count = 0;
    bool ok = read_hashed(file, magic, hash) && read_hashed(file, version, hash) &&
              read_hashed(file, count, hash) && magic == UNIT_MAGIC && version == UNIT_VERSION &&
              count <= 1024;
    for (uint32_t index = 0; ok && index < count; ++index) {
        int32_t format = 0, width = 0, height = 0, size = 0;
        int64_t pts = 0;
        ok = read_hashed(file, format, hash) && read_hashed(file, width, hash) &&
             read_hashed(file, height, hash) && read_hashed(file, pts, hash) &&
             read_hashed(file, size, hash) && format >= 0 && format < AV_PIX_FMT_NB &&
             width > 0 && width <= 65536 && height > 0 && height <= 65536 && size > 0 &&
             size <= 512 * 1024 * 1024 &&
             size == av_image_get_buffer_size(static_cast<AVPixelFormat>(format), width, height, 1);
        if (!ok) break;
        AVFrame* frame = av_frame_alloc();
        if (!frame) {
            ok = false;
            break;
        }
        frame->format = format;
        frame->width = width;
        frame->height = height;
        frame->pts = pts;
        if (av_frame_get_buffer(frame, 1) < 0) {
            av_frame_free(&frame);
            ok = false;
            break;
        }
        std::vector<uint8_t> data(static_cast<size_t>(size));
        if (std::fread(data.data(), 1, data.size(), file) != data.size()) {
            av_frame_free(&frame);
            ok = false;
            break;
        }
        hash = hash_bytes(hash, data.data(), data.size());
        uint8_t* source_data[4] = {};
        int source_linesize[4] = {};
        if (av_image_fill_arrays(
                source_data,
                source_linesize,
                data.data(),
                static_cast<AVPixelFormat>(format),
                width,
                height,
                1
            ) < 0) {
            av_frame_free(&frame);
            ok = false;
            break;
        }
        av_image_copy(
            frame->data,
            frame->linesize,
            const_cast<const uint8_t**>(source_data),
            source_linesize,
            static_cast<AVPixelFormat>(format),
            width,
            height
        );
        frames.push_back(frame);
    }
    uint64_t stored_hash = 0;
    if (ok) ok = std::fread(&stored_hash, sizeof(stored_hash), 1, file) == 1 && stored_hash == hash &&
                 std::fgetc(file) == EOF;
    std::fclose(file);
    if (!ok) {
        for (AVFrame* frame : frames) av_frame_free(&frame);
        frames.clear();
        error = "Checkpoint frame file is corrupt or incompatible";
    }
    return ok;
}

bool same_processor(
    const processors::ProcessorConfig& left,
    const processors::ProcessorConfig& right
) {
    if (left.processor_type != right.processor_type || left.width != right.width ||
        left.height != right.height || left.scaling_factor != right.scaling_factor ||
        left.noise_level != right.noise_level || left.frm_rate_mul != right.frm_rate_mul ||
        left.scn_det_thresh != right.scn_det_thresh || model_name(left) != model_name(right)) {
        return false;
    }
    switch (left.processor_type) {
        case processors::ProcessorType::Libplacebo:
            return true;
        case processors::ProcessorType::RealESRGAN:
            return std::get<processors::RealESRGANConfig>(left.config).tta_mode ==
                   std::get<processors::RealESRGANConfig>(right.config).tta_mode;
        case processors::ProcessorType::RealCUGAN: {
            const auto& left_config = std::get<processors::RealCUGANConfig>(left.config);
            const auto& right_config = std::get<processors::RealCUGANConfig>(right.config);
            return left_config.tta_mode == right_config.tta_mode &&
                   left_config.num_threads == right_config.num_threads &&
                   left_config.syncgap == right_config.syncgap;
        }
        case processors::ProcessorType::RIFE: {
            const auto& left_config = std::get<processors::RIFEConfig>(left.config);
            const auto& right_config = std::get<processors::RIFEConfig>(right.config);
            return left_config.tta_mode == right_config.tta_mode &&
                   left_config.tta_temporal_mode == right_config.tta_temporal_mode &&
                   left_config.uhd_mode == right_config.uhd_mode &&
                   left_config.num_threads == right_config.num_threads;
        }
        default:
            return false;
    }
}

bool same_encoder(const encoder::EncoderConfig& left, const encoder::EncoderConfig& right) {
    return left.codec == right.codec && left.recalculate_pts == right.recalculate_pts &&
           left.copy_audio_streams == right.copy_audio_streams &&
           left.copy_subtitle_streams == right.copy_subtitle_streams &&
           left.pix_fmt == right.pix_fmt && left.bit_rate == right.bit_rate &&
           left.rc_buffer_size == right.rc_buffer_size && left.rc_min_rate == right.rc_min_rate &&
           left.rc_max_rate == right.rc_max_rate && left.qmin == right.qmin && left.qmax == right.qmax &&
           left.gop_size == right.gop_size && left.max_b_frames == right.max_b_frames &&
           left.keyint_min == right.keyint_min && left.refs == right.refs &&
           left.thread_count == right.thread_count && left.delay == right.delay &&
           left.extra_opts == right.extra_opts;
}

}  // namespace

bool is_resume_artifact(const std::filesystem::path& artifact) {
    std::error_code error;
    const std::filesystem::path normalized = normalized_path(artifact);
    return normalized.extension() == ARTIFACT_EXTENSION &&
           std::filesystem::is_directory(normalized, error) &&
           std::filesystem::is_regular_file(manifest_path(normalized), error);
}

int inspect_resume_artifact(
    const std::filesystem::path& artifact,
    ResumeInfo& info,
    std::string& error
) {
    const std::filesystem::path normalized = normalized_path(artifact);
    if (!is_resume_artifact(normalized)) {
        error = "Path is not a Video2X resume artifact";
        return AVERROR_INVALIDDATA;
    }
    uintmax_t input_size = 0;
    int64_t input_mtime = 0, tail_frames = 0;
    uint64_t input_fingerprint = 0;
    if (!load_manifest(
            normalized, info, input_size, input_mtime, input_fingerprint, tail_frames, error
        )) {
        return AVERROR_INVALIDDATA;
    }
    std::error_code filesystem_error;
    if (!std::filesystem::is_regular_file(info.input_path, filesystem_error) ||
        std::filesystem::file_size(info.input_path, filesystem_error) != input_size ||
        file_mtime(info.input_path, filesystem_error) != input_mtime) {
        error = "Original input is missing or has changed";
        return AVERROR_INVALIDDATA;
    }
    if (!info.input_path.is_absolute() || !info.output_path.is_absolute()) {
        error = "Resume manifest paths must be absolute";
        return AVERROR_INVALIDDATA;
    }
    std::string fingerprint_error;
    if (fingerprint_file(info.input_path, fingerprint_error) != input_fingerprint) {
        error = fingerprint_error.empty() ? "Original input fingerprint does not match"
                                          : fingerprint_error;
        return AVERROR_INVALIDDATA;
    }
    if (info.status == Status::Completed) {
        if (!std::filesystem::is_regular_file(info.output_path, filesystem_error)) {
            error = "Completed resume workspace output is missing";
            return AVERROR_INVALIDDATA;
        }
        return 0;
    }
    if (std::filesystem::exists(info.output_path, filesystem_error)) {
        error = "Intended final output already exists; resume workspace was preserved";
        return AVERROR(EEXIST);
    }
    int64_t expected_width = info.source_width;
    int64_t expected_height = info.source_height;
    if (info.processor_config.processor_type == processors::ProcessorType::Libplacebo) {
        expected_width = info.processor_config.width;
        expected_height = info.processor_config.height;
    } else if (
        info.processor_config.processor_type == processors::ProcessorType::RealESRGAN ||
        info.processor_config.processor_type == processors::ProcessorType::RealCUGAN
    ) {
        expected_width *= info.processor_config.scaling_factor;
        expected_height *= info.processor_config.scaling_factor;
    }
    if (expected_width <= 0 || expected_width > 65536 || expected_height <= 0 ||
        expected_height > 65536) {
        error = "Checkpoint output dimensions are invalid";
        return AVERROR_INVALIDDATA;
    }
    int checkpoint_pixel_format = AV_PIX_FMT_NONE;
    auto validate_frames = [&](const std::vector<AVFrame*>& frames) {
        for (AVFrame* frame : frames) {
            if (frame->width != expected_width || frame->height != expected_height) {
                error = "Checkpoint frame dimensions do not match task configuration";
                return false;
            }
            if (checkpoint_pixel_format == AV_PIX_FMT_NONE) {
                checkpoint_pixel_format = frame->format;
            } else if (checkpoint_pixel_format != frame->format) {
                error = "Checkpoint frame pixel formats are inconsistent";
                return false;
            }
        }
        return true;
    };
    int64_t counted_output_frames = 0;
    for (int64_t index = 0; index < info.completed_source_frames; ++index) {
        std::vector<AVFrame*> frames;
        if (!read_unit(unit_path(normalized, index), frames, error)) return AVERROR_INVALIDDATA;
        const size_t expected_frames =
            info.processor_config.processor_type == processors::ProcessorType::RIFE
                ? static_cast<size_t>(index == 0 ? 1 : info.processor_config.frm_rate_mul)
                : 1;
        if (frames.size() != expected_frames || !validate_frames(frames)) {
            for (AVFrame* frame : frames) av_frame_free(&frame);
            if (error.empty()) error = "Checkpoint processing unit has an invalid frame count";
            return AVERROR_INVALIDDATA;
        }
        counted_output_frames += static_cast<int64_t>(frames.size());
        for (AVFrame* frame : frames) av_frame_free(&frame);
    }
    if (tail_frames > 0) {
        std::vector<AVFrame*> frames;
        if (!read_unit(tail_path(normalized), frames, error) ||
            static_cast<int64_t>(frames.size()) != tail_frames || !validate_frames(frames)) {
            for (AVFrame* frame : frames) av_frame_free(&frame);
            if (error.empty()) error = "Tail checkpoint frame count does not match manifest";
            return AVERROR_INVALIDDATA;
        }
        counted_output_frames += tail_frames;
        for (AVFrame* frame : frames) av_frame_free(&frame);
    }
    if (counted_output_frames != info.completed_output_frames) {
        error = "Checkpoint output frame count does not match manifest";
        return AVERROR_INVALIDDATA;
    }
    return 0;
}

int restart_resume_artifact(
    const std::filesystem::path& artifact,
    ResumeInfo& info,
    std::string& error
) {
    if (inspect_resume_artifact(artifact, info, error) < 0) return AVERROR_INVALIDDATA;
    if (info.status == Status::Completed) {
        error = "Completed resume workspaces cannot be restarted";
        return AVERROR(EINVAL);
    }
    for (const auto& entry : std::filesystem::directory_iterator(artifact)) {
        if (entry.path().filename() == "manifest.v2x") continue;
        std::error_code remove_error;
        std::filesystem::remove_all(entry.path(), remove_error);
        if (remove_error) {
            error = "Unable to remove old checkpoint data: " + remove_error.message();
            return AVERROR(remove_error.value());
        }
    }
    info.status = Status::Active;
    info.completed_source_frames = 0;
    info.completed_output_frames = 0;
    info.checkpoint_timestamp = std::time(nullptr);
    std::error_code filesystem_error;
    uintmax_t size = std::filesystem::file_size(info.input_path, filesystem_error);
    int64_t mtime = file_mtime(info.input_path, filesystem_error);
    uint64_t fingerprint = fingerprint_file(info.input_path, error);
    if (filesystem_error || !error.empty() ||
        !write_manifest(info, size, mtime, fingerprint, 0, error)) {
        return AVERROR_EXTERNAL;
    }
    return 0;
}

Session::Session(
    std::filesystem::path artifact,
    std::filesystem::path input,
    std::filesystem::path output,
    const processors::ProcessorConfig& processor_config,
    const encoder::EncoderConfig& encoder_config,
    uint32_t vk_device_index,
    AVHWDeviceType hw_device_type
) {
    info_.version = MANIFEST_VERSION;
    info_.artifact_path = normalized_path(artifact);
    info_.input_path = normalized_path(input);
    info_.output_path = normalized_path(output);
    info_.processor_config = processor_config;
    info_.encoder_config = encoder_config;
    info_.vk_device_index = vk_device_index;
    info_.hw_device_type = hw_device_type;
}

int Session::initialize(
    AVFormatContext* format,
    AVCodecContext* decoder,
    int video_stream,
    std::string& error
) {
    std::error_code filesystem_error;
    std::filesystem::create_directories(info_.artifact_path, filesystem_error);
    if (filesystem_error) return error = "Unable to create resume workspace", AVERROR_EXTERNAL;
    input_size_ = std::filesystem::file_size(info_.input_path, filesystem_error);
    input_mtime_ = file_mtime(info_.input_path, filesystem_error);
    input_fingerprint_ = fingerprint_file(info_.input_path, error);
    if (filesystem_error || !error.empty()) return AVERROR_EXTERNAL;

    if (std::filesystem::exists(manifest_path(info_.artifact_path), filesystem_error)) {
        ResumeInfo existing;
        if (inspect_resume_artifact(info_.artifact_path, existing, error) < 0) return AVERROR_INVALIDDATA;
        if (existing.status == Status::Completed) {
            error = "Resume workspace is already completed";
            return AVERROR(EEXIST);
        }
        if (existing.input_path != info_.input_path || existing.output_path != info_.output_path ||
            !same_processor(existing.processor_config, info_.processor_config) ||
            !same_encoder(existing.encoder_config, info_.encoder_config) ||
            existing.vk_device_index != info_.vk_device_index ||
            existing.hw_device_type != info_.hw_device_type) {
            error = "Resume workspace task configuration does not match";
            return AVERROR_INVALIDDATA;
        }
        info_ = std::move(existing);
        uintmax_t ignored_size = 0;
        int64_t ignored_mtime = 0;
        uint64_t ignored_fingerprint = 0;
        if (!load_manifest(
                info_.artifact_path,
                info_,
                ignored_size,
                ignored_mtime,
                ignored_fingerprint,
                tail_frame_count_,
                error
            )) {
            return AVERROR_INVALIDDATA;
        }
        resumed_ = info_.completed_source_frames > 0;
    } else {
        info_.video_stream_index = video_stream;
        info_.source_width = decoder->width;
        info_.source_height = decoder->height;
        info_.source_pixel_format = decoder->pix_fmt;
        info_.source_time_base = format->streams[video_stream]->time_base;
        info_.source_frame_rate = avutils::get_video_frame_rate(format, video_stream);
        info_.total_source_frames = avutils::get_video_frame_count(format, video_stream);
        info_.total_source_frames_exact =
            format->streams[video_stream]->nb_frames != AV_NOPTS_VALUE &&
            format->streams[video_stream]->nb_frames > 0;
        info_.checkpoint_timestamp = std::time(nullptr);
        if (!write_manifest(
                info_, input_size_, input_mtime_, input_fingerprint_, tail_frame_count_, error
            )) {
            return AVERROR_EXTERNAL;
        }
    }
    if (info_.video_stream_index != video_stream || info_.source_width != decoder->width ||
        info_.source_height != decoder->height || info_.source_pixel_format != decoder->pix_fmt ||
        av_cmp_q(info_.source_time_base, format->streams[video_stream]->time_base) != 0 ||
        av_cmp_q(info_.source_frame_rate, avutils::get_video_frame_rate(format, video_stream)) != 0) {
        error = "Source video stream no longer matches resume metadata";
        return AVERROR_INVALIDDATA;
    }
    logger()->info(
        "{} task: {} recovered source frames, {} recovered output frames",
        resumed_ ? "Resumed" : "Fresh",
        info_.completed_source_frames,
        info_.completed_output_frames
    );
    return 0;
}

int Session::load_unit(
    int64_t source_index,
    std::vector<AVFrame*>& frames,
    std::string& error
) const {
    return read_unit(unit_path(info_.artifact_path, source_index), frames, error)
               ? 0
               : AVERROR_INVALIDDATA;
}

int Session::store_unit(
    int64_t source_index,
    const std::vector<AVFrame*>& frames,
    std::string& error
) {
    return write_unit(unit_path(info_.artifact_path, source_index), frames, error)
               ? 0
               : AVERROR_EXTERNAL;
}

int Session::load_tail(std::vector<AVFrame*>& frames, std::string& error) const {
    if (tail_frame_count_ == 0) return 0;
    return read_unit(tail_path(info_.artifact_path), frames, error) ? 0 : AVERROR_INVALIDDATA;
}

int Session::store_tail(const std::vector<AVFrame*>& frames, std::string& error) {
    tail_frame_count_ = static_cast<int64_t>(frames.size());
    if (frames.empty()) return 0;
    return write_unit(tail_path(info_.artifact_path), frames, error) ? 0 : AVERROR_EXTERNAL;
}

int Session::checkpoint(
    int64_t source_frames,
    int64_t output_frames,
    bool force,
    std::string& error
) {
    if (!force && source_frames % CHECKPOINT_INTERVAL != 0) return 0;
    info_.completed_source_frames = source_frames;
    info_.completed_output_frames = output_frames;
    info_.checkpoint_timestamp = std::time(nullptr);
    if (!write_manifest(
            info_, input_size_, input_mtime_, input_fingerprint_, tail_frame_count_, error
        )) {
        return AVERROR_EXTERNAL;
    }
    logger()->info(
        "Published checkpoint: {} source frames, {} output frames",
        source_frames,
        output_frames
    );
    return 0;
}

void Session::set_total_source_frames(int64_t source_frames) {
    info_.total_source_frames = source_frames;
    info_.total_source_frames_exact = true;
}

int Session::complete(std::string& error) {
    info_.status = Status::Completed;
    info_.checkpoint_timestamp = std::time(nullptr);
    if (!write_manifest(info_, input_size_, input_mtime_, input_fingerprint_, 0, error))
        return AVERROR_EXTERNAL;
    for (const auto& entry : std::filesystem::directory_iterator(info_.artifact_path)) {
        if (entry.path() == manifest_path(info_.artifact_path)) continue;
        std::error_code remove_error;
        std::filesystem::remove_all(entry.path(), remove_error);
        if (remove_error) logger()->warn("Unable to clean resume data: {}", remove_error.message());
    }
    return 0;
}

std::filesystem::path Session::temporary_output_path() const {
    return info_.artifact_path /
           (std::string("output.part") + info_.output_path.extension().u8string());
}

int Session::publish_output(
    const std::filesystem::path& temporary_output,
    std::string& error
) {
    std::error_code filesystem_error;
    if (std::filesystem::exists(info_.output_path, filesystem_error)) {
        error = "Final output already exists";
        return AVERROR(EEXIST);
    }
    if (!atomic_replace(temporary_output, info_.output_path, error)) return AVERROR_EXTERNAL;
    return complete(error);
}

}  // namespace resume
}  // namespace video2x
