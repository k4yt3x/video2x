#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Creator: Video2X GUI
Author: K4YT3X
Date Created: May 5, 2020
Last Modified: September 13, 2020
"""

# local imports
from bilogger import BiLogger
from upscaler import UPSCALER_VERSION
from upscaler import Upscaler
from wrappers.ffmpeg import Ffmpeg

# built-in imports
import contextlib
import json
import mimetypes
import os
import pathlib
import sys
import tempfile
import time
import traceback
import urllib
import yaml

# third-party imports
from PyQt5 import uic
from PyQt5.QtCore import *
from PyQt5.QtGui import *
from PyQt5.QtWidgets import *
import magic

GUI_VERSION = '2.8.0'

LEGAL_INFO = f'''Video2X GUI Version: {GUI_VERSION}\\
Upscaler Version: {UPSCALER_VERSION}\\
Author: K4YT3X\\
License: GNU GPL v3\\
Github Page: [https://github.com/k4yt3x/video2x](https://github.com/k4yt3x/video2x)\\
Contact: [k4yt3x@k4yt3x.com](mailto:k4yt3x@k4yt3x.com)'''

AVAILABLE_DRIVERS = {
    'Waifu2X Caffe': 'waifu2x_caffe',
    'Waifu2X Converter CPP': 'waifu2x_converter_cpp',
    'Waifu2X NCNN Vulkan': 'waifu2x_ncnn_vulkan',
    'SRMD NCNN Vulkan': 'srmd_ncnn_vulkan',
    'RealSR NCNN Vulkan': 'realsr_ncnn_vulkan',
    'Anime4KCPP': 'anime4kcpp'
}

# get current working directory before it is changed by drivers
CWD = pathlib.Path.cwd()


def resource_path(relative_path: str) -> pathlib.Path:
    try:
        base_path = pathlib.Path(sys._MEIPASS)
    except AttributeError:
        base_path = pathlib.Path(__file__).parent
    return base_path / relative_path


class WorkerSignals(QObject):
    progress = pyqtSignal(tuple)
    error = pyqtSignal(Exception)
    interrupted = pyqtSignal()
    finished = pyqtSignal()


class ProgressMonitorWorkder(QRunnable):
    def __init__(self, fn, *args, **kwargs):
        super(ProgressMonitorWorkder, self).__init__()
        self.fn = fn
        self.args = args
        self.kwargs = kwargs
        self.signals = WorkerSignals()
        self.kwargs['progress_callback'] = self.signals.progress

    @pyqtSlot()
    def run(self):
        try:
            self.fn(*self.args, **self.kwargs)
        except Exception:
            pass


class UpscalerWorker(QRunnable):

    def __init__(self, fn, *args, **kwargs):
        super(UpscalerWorker, self).__init__()

        # Store constructor arguments (re-used for processing)
        self.fn = fn
        self.args = args
        self.kwargs = kwargs
        self.signals = WorkerSignals()

    @pyqtSlot()
    def run(self):

        # Retrieve args/kwargs here; and fire processing using them
        try:
            self.fn(*self.args, **self.kwargs)
        except (KeyboardInterrupt, SystemExit):
            self.signals.interrupted.emit()
        except Exception as e:
            traceback.print_exc()
            self.signals.error.emit(e)
        else:
            self.signals.finished.emit()


class InputTableModel(QAbstractTableModel):
    def __init__(self, data):
        super(InputTableModel, self).__init__()
        self._data = data

    def data(self, index, role):
        if role == Qt.DisplayRole:

            file_path = self._data[index.row()]

            if index.column() == 0:
                return str(file_path.absolute())
            else:

                # determine file type
                # if path is a folder
                if file_path.is_dir():
                    return 'Folder'

                # if path is single file
                # determine file type
                elif file_path.is_file():
                    try:
                        input_file_mime_type = magic.from_file(str(file_path.absolute()), mime=True)
                        input_file_type = input_file_mime_type.split('/')[0]
                        input_file_subtype = input_file_mime_type.split('/')[1]
                    except Exception:
                        input_file_type = input_file_subtype = None

                    # in case python-magic fails to detect file type
                    # try guessing file mime type with mimetypes
                    if input_file_type not in ['image', 'video']:
                        input_file_mime_type = mimetypes.guess_type(file_path.name)[0]
                        input_file_type = input_file_mime_type.split('/')[0]
                        input_file_subtype = input_file_mime_type.split('/')[1]

                    if input_file_type == 'image':
                        if input_file_subtype == 'gif':
                            return 'GIF'
                        return 'Image'

                    elif input_file_type == 'video':
                        return 'Video'

                    else:
                        return 'Unknown'

                else:
                    return 'Unknown'

    def rowCount(self, index):
        return len(self._data)

    def columnCount(self, index):
        return 2

    def removeRow(self, index):
        self._data.pop(index)

    def headerData(self, section, orientation, role):
        # section is the index of the column/row.
        if role != Qt.DisplayRole:
            return None

        horizontal_headers = ['File Path', 'Type']

        # return the correspondign header
        if orientation == Qt.Horizontal:
            return horizontal_headers[section]

        # simply return the line number
        if orientation == Qt.Vertical:
            return str(section)


class Video2XMainWindow(QMainWindow):

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        uic.loadUi(str(resource_path('video2x_gui.ui')), self)

        # redirect output to both terminal and log file
        self.log_file = tempfile.TemporaryFile(mode='a+', suffix='.log', prefix='video2x_', encoding='utf-8')
        sys.stdout = BiLogger(sys.stdout, self.log_file)
        sys.stderr = BiLogger(sys.stderr, self.log_file)

        # create thread pool for upscaler workers
        self.threadpool = QThreadPool()

        # set window title and icon
        self.video2x_icon_path = str(resource_path('images/video2x.png'))
        self.setWindowTitle(f'Video2X GUI {GUI_VERSION}')
        self.setWindowIcon(QIcon(self.video2x_icon_path))

        # register shortcut keys
        QShortcut(QKeySequence(Qt.CTRL + Qt.Key_W), self, self.close)
        QShortcut(QKeySequence(Qt.CTRL + Qt.Key_Q), self, self.close)
        QShortcut(QKeySequence(Qt.CTRL + Qt.Key_I), self, self.select_input_file)
        QShortcut(QKeySequence(Qt.CTRL + Qt.Key_O), self, self.select_output_file)
        QShortcut(QKeySequence(Qt.CTRL + Qt.SHIFT + Qt.Key_I), self, self.select_input_folder)
        QShortcut(QKeySequence(Qt.CTRL + Qt.SHIFT + Qt.Key_O), self, self.select_output_folder)

        # menu bar
        self.action_exit = self.findChild(QAction, 'actionExit')
        self.action_exit.triggered.connect(self.close)

        self.action_shortcuts = self.findChild(QAction, 'actionShortcuts')
        self.action_shortcuts.triggered.connect(self.show_shortcuts)

        self.action_about = self.findChild(QAction, 'actionAbout')
        self.action_about.triggered.connect(self.show_about)

        # main tab
        # select input file/folder
        self.input_table_view = self.findChild(QTableView, 'inputTableView')
        self.input_table_view.dragEnterEvent = self.dragEnterEvent
        self.input_table_view.dropEvent = self.dropEvent

        # initialize data in table
        self.input_table_data = []
        self.input_table_model = InputTableModel(self.input_table_data)
        self.input_table_view.setModel(self.input_table_model)
        # stretch file path and fill columns horizontally
        self.input_table_view.horizontalHeader().setSectionResizeMode(0, QHeaderView.Stretch)

        # input table buttons
        self.input_select_file_button = self.findChild(QPushButton, 'inputSelectFileButton')
        self.input_select_file_button.clicked.connect(self.select_input_file)
        self.input_select_folder_button = self.findChild(QPushButton, 'inputSelectFolderButton')
        self.input_select_folder_button.clicked.connect(self.select_input_folder)
        self.input_delete_selected_button = self.findChild(QPushButton, 'inputDeleteSelectedButton')
        self.input_delete_selected_button.clicked.connect(self.input_table_delete_selected)
        self.input_clear_all_button = self.findChild(QPushButton, 'inputClearAllButton')
        self.input_clear_all_button.clicked.connect(self.input_table_clear_all)

        # other paths selection
        # select output file/folder
        self.output_line_edit = self.findChild(QLineEdit, 'outputLineEdit')
        self.enable_line_edit_file_drop(self.output_line_edit)
        self.output_line_edit.setText(str((CWD / 'output').absolute()))
        self.output_select_file_button = self.findChild(QPushButton, 'outputSelectFileButton')
        self.output_select_file_button.clicked.connect(self.select_output_file)
        self.output_select_folder_button = self.findChild(QPushButton, 'outputSelectFolderButton')
        self.output_select_folder_button.clicked.connect(self.select_output_folder)

        # config file
        self.config_line_edit = self.findChild(QLineEdit, 'configLineEdit')
        self.enable_line_edit_file_drop(self.config_line_edit)

        if getattr(sys, 'frozen', False):
            self.config_line_edit.setText(str((pathlib.Path(sys.executable).parent / 'video2x.yaml').absolute()))
        elif __file__:
            self.config_line_edit.setText(str((pathlib.Path(__file__).parent / 'video2x.yaml').absolute()))

        self.config_select_file_button = self.findChild(QPushButton, 'configSelectButton')
        self.config_select_file_button.clicked.connect(self.select_config_file)

        # cache directory
        self.cache_line_edit = self.findChild(QLineEdit, 'cacheLineEdit')
        self.enable_line_edit_file_drop(self.cache_line_edit)
        self.cache_select_folder_button = self.findChild(QPushButton, 'cacheSelectFolderButton')
        self.cache_select_folder_button.clicked.connect(self.select_cache_folder)

        # express settings
        self.driver_combo_box = self.findChild(QComboBox, 'driverComboBox')
        self.driver_combo_box.currentTextChanged.connect(self.update_gui_for_driver)
        self.processes_spin_box = self.findChild(QSpinBox, 'processesSpinBox')
        self.scale_ratio_double_spin_box = self.findChild(QDoubleSpinBox, 'scaleRatioDoubleSpinBox')
        self.output_width_spin_box = self.findChild(QSpinBox, 'outputWidthSpinBox')
        self.output_width_spin_box.valueChanged.connect(self.mutually_exclude_scale_ratio_resolution)
        self.output_height_spin_box = self.findChild(QSpinBox, 'outputHeightSpinBox')
        self.output_height_spin_box.valueChanged.connect(self.mutually_exclude_scale_ratio_resolution)
        self.output_file_name_format_string_line_edit = self.findChild(QLineEdit, 'outputFileNameFormatStringLineEdit')
        self.image_output_extension_line_edit = self.findChild(QLineEdit, 'imageOutputExtensionLineEdit')
        self.video_output_extension_line_edit = self.findChild(QLineEdit, 'videoOutputExtensionLineEdit')
        self.preserve_frames_check_box = self.findChild(QCheckBox, 'preserveFramesCheckBox')

        # frame preview
        self.frame_preview_show_preview_check_box = self.findChild(QCheckBox, 'framePreviewShowPreviewCheckBox')
        self.frame_preview_keep_aspect_ratio_check_box = self.findChild(QCheckBox, 'framePreviewKeepAspectRatioCheckBox')
        self.frame_preview_label = self.findChild(QLabel, 'framePreviewLabel')

        # currently processing
        self.currently_processing_label = self.findChild(QLabel, 'currentlyProcessingLabel')
        self.current_progress_bar = self.findChild(QProgressBar, 'currentProgressBar')
        self.time_elapsed_label = self.findChild(QLabel, 'timeElapsedLabel')
        self.time_remaining_label = self.findChild(QLabel, 'timeRemainingLabel')
        self.rate_label = self.findChild(QLabel, 'rateLabel')
        self.frames_label = self.findChild(QLabel, 'framesLabel')

        # overall progress
        self.overall_progress_bar = self.findChild(QProgressBar, 'overallProgressBar')
        self.overall_progress_label = self.findChild(QLabel, 'overallProgressLabel')
        self.start_button = self.findChild(QPushButton, 'startButton')
        self.start_button.clicked.connect(self.start)
        self.stop_button = self.findChild(QPushButton, 'stopButton')
        self.stop_button.clicked.connect(self.stop)

        # driver settings
        # waifu2x-caffe
        self.waifu2x_caffe_path_line_edit = self.findChild(QLineEdit, 'waifu2xCaffePathLineEdit')
        self.enable_line_edit_file_drop(self.waifu2x_caffe_path_line_edit)
        self.waifu2x_caffe_path_select_button = self.findChild(QPushButton, 'waifu2xCaffePathSelectButton')
        self.waifu2x_caffe_path_select_button.clicked.connect(lambda: self.select_driver_binary_path(self.waifu2x_caffe_path_line_edit))
        self.waifu2x_caffe_mode_combo_box = self.findChild(QComboBox, 'waifu2xCaffeModeComboBox')
        self.waifu2x_caffe_noise_level_spin_box = self.findChild(QSpinBox, 'waifu2xCaffeNoiseLevelSpinBox')
        self.waifu2x_caffe_process_combo_box = self.findChild(QComboBox, 'waifu2xCaffeProcessComboBox')
        self.waifu2x_caffe_model_combobox = self.findChild(QComboBox, 'waifu2xCaffeModelComboBox')
        self.waifu2x_caffe_crop_size_spin_box = self.findChild(QSpinBox, 'waifu2xCaffeCropSizeSpinBox')
        self.waifu2x_caffe_output_quality_spin_box = self.findChild(QSpinBox, 'waifu2xCaffeOutputQualitySpinBox')
        self.waifu2x_caffe_output_depth_spin_box = self.findChild(QSpinBox, 'waifu2xCaffeOutputDepthSpinBox')
        self.waifu2x_caffe_batch_size_spin_box = self.findChild(QSpinBox, 'waifu2xCaffeBatchSizeSpinBox')
        self.waifu2x_caffe_gpu_spin_box = self.findChild(QSpinBox, 'waifu2xCaffeGpuSpinBox')
        self.waifu2x_caffe_tta_check_box = self.findChild(QCheckBox, 'waifu2xCaffeTtaCheckBox')

        # waifu2x-converter-cpp
        self.waifu2x_converter_cpp_path_line_edit = self.findChild(QLineEdit, 'waifu2xConverterCppPathLineEdit')
        self.enable_line_edit_file_drop(self.waifu2x_converter_cpp_path_line_edit)
        self.waifu2x_converter_cpp_path_edit_button = self.findChild(QPushButton, 'waifu2xConverterCppPathSelectButton')
        self.waifu2x_converter_cpp_path_edit_button.clicked.connect(lambda: self.select_driver_binary_path(self.waifu2x_converter_cpp_path_line_edit))
        self.waifu2x_converter_cpp_png_compression_spin_box = self.findChild(QSpinBox, 'waifu2xConverterCppPngCompressionSpinBox')
        self.waifu2x_converter_cpp_image_quality_spin_box = self.findChild(QSpinBox, 'waifu2xConverterCppImageQualitySpinBox')
        self.waifu2x_converter_cpp_block_size_spin_box = self.findChild(QSpinBox, 'waifu2xConverterCppBlockSizeSpinBox')
        self.waifu2x_converter_cpp_processor_spin_box = self.findChild(QSpinBox, 'waifu2xConverterCppProcessorSpinBox')
        self.waifu2x_converter_cpp_model_combo_box = self.findChild(QComboBox, 'waifu2xConverterCppModelComboBox')
        self.waifu2x_converter_cpp_noise_level_spin_box = self.findChild(QSpinBox, 'waifu2xConverterCppNoiseLevelSpinBox')
        self.waifu2x_converter_cpp_mode_combo_box = self.findChild(QComboBox, 'waifu2xConverterCppModeComboBox')
        self.waifu2x_converter_cpp_log_level_spin_box = self.findChild(QSpinBox, 'waifu2xConverterCppLogLevelSpinBox')
        self.waifu2x_converter_cpp_disable_gpu_check_box = self.findChild(QCheckBox, 'waifu2xConverterCppDisableGpuCheckBox')
        self.waifu2x_converter_cpp_force_opencl_check_box = self.findChild(QCheckBox, 'waifu2xConverterCppForceOpenclCheckBox')
        self.waifu2x_converter_cpp_tta_check_box = self.findChild(QCheckBox, 'waifu2xConverterCppTtaCheckBox')

        # waifu2x-ncnn-vulkan
        self.waifu2x_ncnn_vulkan_path_line_edit = self.findChild(QLineEdit, 'waifu2xNcnnVulkanPathLineEdit')
        self.enable_line_edit_file_drop(self.waifu2x_ncnn_vulkan_path_line_edit)
        self.waifu2x_ncnn_vulkan_path_select_button = self.findChild(QPushButton, 'waifu2xNcnnVulkanPathSelectButton')
        self.waifu2x_ncnn_vulkan_path_select_button.clicked.connect(lambda: self.select_driver_binary_path(self.waifu2x_ncnn_vulkan_path_line_edit))
        self.waifu2x_ncnn_vulkan_noise_level_spin_box = self.findChild(QSpinBox, 'waifu2xNcnnVulkanNoiseLevelSpinBox')
        self.waifu2x_ncnn_vulkan_tile_size_spin_box = self.findChild(QSpinBox, 'waifu2xNcnnVulkanTileSizeSpinBox')
        self.waifu2x_ncnn_vulkan_model_combo_box = self.findChild(QComboBox, 'waifu2xNcnnVulkanModelComboBox')
        self.waifu2x_ncnn_vulkan_gpu_id_spin_box = self.findChild(QSpinBox, 'waifu2xNcnnVulkanGpuIdSpinBox')
        self.waifu2x_ncnn_vulkan_jobs_line_edit = self.findChild(QLineEdit, 'waifu2xNcnnVulkanJobsLineEdit')
        self.waifu2x_ncnn_vulkan_tta_check_box = self.findChild(QCheckBox, 'waifu2xNcnnVulkanTtaCheckBox')

        # srmd-ncnn-vulkan
        self.srmd_ncnn_vulkan_path_line_edit = self.findChild(QLineEdit, 'srmdNcnnVulkanPathLineEdit')
        self.enable_line_edit_file_drop(self.srmd_ncnn_vulkan_path_line_edit)
        self.srmd_ncnn_vulkan_path_select_button = self.findChild(QPushButton, 'srmdNcnnVulkanPathSelectButton')
        self.srmd_ncnn_vulkan_path_select_button.clicked.connect(lambda: self.select_driver_binary_path(self.srmd_ncnn_vulkan_path_line_edit))
        self.srmd_ncnn_vulkan_noise_level_spin_box = self.findChild(QSpinBox, 'srmdNcnnVulkanNoiseLevelSpinBox')
        self.srmd_ncnn_vulkan_tile_size_spin_box = self.findChild(QSpinBox, 'srmdNcnnVulkanTileSizeSpinBox')
        self.srmd_ncnn_vulkan_model_combo_box = self.findChild(QComboBox, 'srmdNcnnVulkanModelComboBox')
        self.srmd_ncnn_vulkan_gpu_id_spin_box = self.findChild(QSpinBox, 'srmdNcnnVulkanGpuIdSpinBox')
        self.srmd_ncnn_vulkan_jobs_line_edit = self.findChild(QLineEdit, 'srmdNcnnVulkanJobsLineEdit')
        self.srmd_ncnn_vulkan_tta_check_box = self.findChild(QCheckBox, 'srmdNcnnVulkanTtaCheckBox')

        # realsr-ncnn-vulkan
        self.realsr_ncnn_vulkan_path_line_edit = self.findChild(QLineEdit, 'realsrNcnnVulkanPathLineEdit')
        self.enable_line_edit_file_drop(self.realsr_ncnn_vulkan_path_line_edit)
        self.realsr_ncnn_vulkan_path_select_button = self.findChild(QPushButton, 'realsrNcnnVulkanPathSelectButton')
        self.realsr_ncnn_vulkan_path_select_button.clicked.connect(lambda: self.select_driver_binary_path(self.realsr_ncnn_vulkan_path_line_edit))
        self.realsr_ncnn_vulkan_tile_size_spin_box = self.findChild(QSpinBox, 'realsrNcnnVulkanTileSizeSpinBox')
        self.realsr_ncnn_vulkan_model_combo_box = self.findChild(QComboBox, 'realsrNcnnVulkanModelComboBox')
        self.realsr_ncnn_vulkan_gpu_id_spin_box = self.findChild(QSpinBox, 'realsrNcnnVulkanGpuIdSpinBox')
        self.realsr_ncnn_vulkan_jobs_line_edit = self.findChild(QLineEdit, 'realsrNcnnVulkanJobsLineEdit')
        self.realsr_ncnn_vulkan_tta_check_box = self.findChild(QCheckBox, 'realsrNcnnVulkanTtaCheckBox')

        # anime4k
        self.anime4kcpp_path_line_edit = self.findChild(QLineEdit, 'anime4kCppPathLineEdit')
        self.enable_line_edit_file_drop(self.anime4kcpp_path_line_edit)
        self.anime4kcpp_path_select_button = self.findChild(QPushButton, 'anime4kCppPathSelectButton')
        self.anime4kcpp_path_select_button.clicked.connect(lambda: self.select_driver_binary_path(self.anime4kcpp_path_line_edit))
        self.anime4kcpp_passes_spin_box = self.findChild(QSpinBox, 'anime4kCppPassesSpinBox')
        self.anime4kcpp_push_color_count_spin_box = self.findChild(QSpinBox, 'anime4kCppPushColorCountSpinBox')
        self.anime4kcpp_strength_color_spin_box = self.findChild(QDoubleSpinBox, 'anime4kCppStrengthColorSpinBox')
        self.anime4kcpp_strength_gradient_spin_box = self.findChild(QDoubleSpinBox, 'anime4kCppStrengthGradientSpinBox')
        self.anime4kcpp_threads_spin_box = self.findChild(QSpinBox, 'anime4kCppThreadsSpinBox')
        self.anime4kcpp_pre_filters_spin_box = self.findChild(QSpinBox, 'anime4kCppPreFiltersSpinBox')
        self.anime4kcpp_post_filters_spin_box = self.findChild(QSpinBox, 'anime4kCppPostFiltersSpinBox')
        self.anime4kcpp_platform_id_spin_box = self.findChild(QSpinBox, 'anime4kCppPlatformIdSpinBox')
        self.anime4kcpp_device_id_spin_box = self.findChild(QSpinBox, 'anime4kCppDeviceIdSpinBox')
        self.anime4kcpp_codec_combo_box = self.findChild(QComboBox, 'anime4kCppCodecComboBox')
        self.anime4kcpp_fast_mode_check_box = self.findChild(QCheckBox, 'anime4kCppFastModeCheckBox')
        self.anime4kcpp_pre_processing_check_box = self.findChild(QCheckBox, 'anime4kCppPreProcessingCheckBox')
        self.anime4kcpp_post_processing_check_box = self.findChild(QCheckBox, 'anime4kCppPostProcessingCheckBox')
        self.anime4kcpp_gpu_mode_check_box = self.findChild(QCheckBox, 'anime4kCppGpuModeCheckBox')
        self.anime4kcpp_cnn_mode_check_box = self.findChild(QCheckBox, 'anime4kCppCnnModeCheckBox')
        self.anime4kcpp_hdn_check_box = self.findChild(QCheckBox, 'anime4kCppHdnCheckBox')
        self.anime4kcpp_hdn_level_spin_box = self.findChild(QSpinBox, 'anime4kCppHdnLevelSpinBox')
        self.anime4kcpp_force_fps_double_spin_box = self.findChild(QDoubleSpinBox, 'anime4kCppForceFpsDoubleSpinBox')
        self.anime4kcpp_disable_progress_check_box = self.findChild(QCheckBox, 'anime4kCppDisableProgressCheckBox')
        self.anime4kcpp_alpha_check_box = self.findChild(QCheckBox, 'anime4kCppAlphaCheckBox')

        # FFmpeg settings
        # global options
        self.ffmpeg_path_line_edit = self.findChild(QLineEdit, 'ffmpegPathLineEdit')
        self.enable_line_edit_file_drop(self.ffmpeg_path_line_edit)
        self.ffmpeg_path_select_button = self.findChild(QPushButton, 'ffmpegPathSelectButton')
        self.ffmpeg_path_select_button.clicked.connect(lambda: self.select_driver_binary_path(self.ffmpeg_path_line_edit))
        self.ffmpeg_intermediate_file_name_line_edit = self.findChild(QLineEdit, 'ffmpegIntermediateFileNameLineEdit')

        # extract frames
        self.ffmpeg_extract_frames_output_options_pixel_format_line_edit = self.findChild(QLineEdit, 'ffmpegExtractFramesOutputOptionsPixelFormatLineEdit')
        self.ffmpeg_extract_frames_hardware_acceleration_check_box = self.findChild(QCheckBox, 'ffmpegExtractFramesHardwareAccelerationCheckBox')

        # assemble video
        self.ffmpeg_assemble_video_input_options_force_format_line_edit = self.findChild(QLineEdit, 'ffmpegAssembleVideoInputOptionsForceFormatLineEdit')
        self.ffmpeg_assemble_video_output_options_video_codec_line_edit = self.findChild(QLineEdit, 'ffmpegAssembleVideoOutputOptionsVideoCodecLineEdit')
        self.ffmpeg_assemble_video_output_options_pixel_format_line_edit = self.findChild(QLineEdit, 'ffmpegAssembleVideoOutputOptionsPixelFormatLineEdit')
        self.ffmpeg_assemble_video_output_options_crf_spin_box = self.findChild(QSpinBox, 'ffmpegAssembleVideoOutputOptionsCrfSpinBox')
        self.ffmpeg_assemble_video_output_options_tune_combo_box = self.findChild(QComboBox, 'ffmpegAssembleVideoOutputOptionsTuneComboBox')
        self.ffmpeg_assemble_video_output_options_bitrate_line_edit = self.findChild(QLineEdit, 'ffmpegAssembleVideoOutputOptionsBitrateLineEdit')
        self.ffmpeg_assemble_video_output_options_ensure_divisible_check_box = self.findChild(QCheckBox, 'ffmpegAssembleVideoOutputOptionsEnsureDivisibleCheckBox')
        self.ffmpeg_assemble_video_hardware_acceleration_check_box = self.findChild(QCheckBox, 'ffmpegAssembleVideoHardwareAccelerationCheckBox')

        # migrate_streams
        self.ffmpeg_migrate_streams_output_options_mapping_video_check_box_check_box = self.findChild(QCheckBox, 'ffmpegMigrateStreamsOutputOptionsMappingVideoCheckBox')
        self.ffmpeg_migrate_streams_output_options_mapping_audio_check_box_check_box = self.findChild(QCheckBox, 'ffmpegMigrateStreamsOutputOptionsMappingAudioCheckBox')
        self.ffmpeg_migrate_streams_output_options_mapping_subtitle_check_box_check_box = self.findChild(QCheckBox, 'ffmpegMigrateStreamsOutputOptionsMappingSubtitleCheckBox')
        self.ffmpeg_migrate_streams_output_options_mapping_data_check_box_check_box = self.findChild(QCheckBox, 'ffmpegMigrateStreamsOutputOptionsMappingDataCheckBox')
        self.ffmpeg_migrate_streams_output_options_mapping_font_check_box_check_box = self.findChild(QCheckBox, 'ffmpegMigrateStreamsOutputOptionsMappingFontCheckBox')
        self.ffmpeg_migrate_streams_output_options_pixel_format_line_edit = self.findChild(QLineEdit, 'ffmpegMigrateStreamsOutputOptionsPixelFormatLineEdit')
        self.ffmpeg_migrate_streams_output_options_frame_interpolation_spin_box = self.findChild(QSpinBox, 'ffmpegMigrateStreamsOutputOptionsFrameInterpolationSpinBox')
        self.ffmpeg_migrate_streams_output_options_frame_interpolation_spin_box.valueChanged.connect(self.mutually_exclude_frame_interpolation_stream_copy)
        self.ffmpeg_migrate_streams_output_options_frame_interpolation_spin_box.textChanged.connect(self.mutually_exclude_frame_interpolation_stream_copy)
        self.ffmpeg_migrate_streams_output_options_copy_streams_check_box = self.findChild(QCheckBox, 'ffmpegMigrateStreamsOutputOptionsCopyStreamsCheckBox')
        self.ffmpeg_migrate_streams_output_options_copy_known_metadata_tags_check_box = self.findChild(QCheckBox, 'ffmpegMigrateStreamsOutputOptionsCopyKnownMetadataTagsCheckBox')
        self.ffmpeg_migrate_streams_output_options_copy_arbitrary_metadata_tags_check_box = self.findChild(QCheckBox, 'ffmpegMigrateStreamsOutputOptionsCopyArbitraryMetadataTagsCheckBox')
        self.ffmpeg_migrate_streams_hardware_acceleration_check_box = self.findChild(QCheckBox, 'ffmpegMigrateStreamsHardwareAccelerationCheckBox')

        # Gifski settings
        self.gifski_path_line_edit = self.findChild(QLineEdit, 'gifskiPathLineEdit')
        self.enable_line_edit_file_drop(self.gifski_path_line_edit)
        self.gifski_quality_spin_box = self.findChild(QSpinBox, 'gifskiQualitySpinBox')
        self.gifski_fast_check_box = self.findChild(QCheckBox, 'gifskiFastCheckBox')
        self.gifski_once_check_box = self.findChild(QCheckBox, 'gifskiOnceCheckBox')
        self.gifski_quiet_check_box = self.findChild(QCheckBox, 'gifskiQuietCheckBox')

        # Tools
        self.ffprobe_plain_text_edit = self.findChild(QPlainTextEdit, 'ffprobePlainTextEdit')
        self.ffprobe_plain_text_edit.dropEvent = self.show_ffprobe_output

        # load configurations after GUI initialization
        self.load_configurations()

    def load_configurations(self):

        # get config file path from line edit
        config_file_path = pathlib.Path(os.path.expandvars(self.config_line_edit.text()))

        # if file doesn't exist, return
        if not config_file_path.is_file():
            QErrorMessage(self).showMessage('Video2X configuration file not found, please specify manually.')
            return

        # read configuration dict from config file
        self.config = self.read_config(config_file_path)

        # load FFmpeg settings
        self.ffmpeg_settings = self.config['ffmpeg']
        self.ffmpeg_settings['ffmpeg_path'] = str(pathlib.Path(os.path.expandvars(self.ffmpeg_settings['ffmpeg_path'])).absolute())

        # read Gifski configuration
        self.gifski_settings = self.config['gifski']
        self.gifski_settings['gifski_path'] = str(pathlib.Path(os.path.expandvars(self.gifski_settings['gifski_path'])).absolute())

        # set cache directory path
        if self.config['video2x']['video2x_cache_directory'] is None:
            self.config['video2x']['video2x_cache_directory'] = str((pathlib.Path(tempfile.gettempdir()) / 'video2x').absolute())
        self.cache_line_edit.setText(self.config['video2x']['video2x_cache_directory'])

        self.output_file_name_format_string_line_edit.setText(self.config['video2x']['output_file_name_format_string'])
        self.image_output_extension_line_edit.setText(self.config['video2x']['image_output_extension'])
        self.video_output_extension_line_edit.setText(self.config['video2x']['video_output_extension'])

        # load preserve frames settings
        self.preserve_frames_check_box.setChecked(self.config['video2x']['preserve_frames'])
        self.start_button.setEnabled(True)

        # waifu2x-caffe
        settings = self.config['waifu2x_caffe']
        self.waifu2x_caffe_path_line_edit.setText(str(pathlib.Path(os.path.expandvars(settings['path'])).absolute()))
        self.waifu2x_caffe_mode_combo_box.setCurrentText(settings['mode'])
        self.waifu2x_caffe_noise_level_spin_box.setValue(settings['noise_level'])
        self.waifu2x_caffe_process_combo_box.setCurrentText(settings['process'])
        self.waifu2x_caffe_crop_size_spin_box.setValue(settings['crop_size'])
        self.waifu2x_caffe_output_quality_spin_box.setValue(settings['output_quality'])
        self.waifu2x_caffe_output_depth_spin_box.setValue(settings['output_depth'])
        self.waifu2x_caffe_batch_size_spin_box.setValue(settings['batch_size'])
        self.waifu2x_caffe_gpu_spin_box.setValue(settings['gpu'])
        self.waifu2x_caffe_tta_check_box.setChecked(bool(settings['tta']))

        # waifu2x-converter-cpp
        settings = self.config['waifu2x_converter_cpp']
        self.waifu2x_converter_cpp_path_line_edit.setText(str(pathlib.Path(os.path.expandvars(settings['path'])).absolute()))
        self.waifu2x_converter_cpp_png_compression_spin_box.setValue(settings['png-compression'])
        self.waifu2x_converter_cpp_image_quality_spin_box.setValue(settings['image-quality'])
        self.waifu2x_converter_cpp_block_size_spin_box.setValue(settings['block-size'])
        self.waifu2x_converter_cpp_processor_spin_box.setValue(settings['processor'])
        self.waifu2x_converter_cpp_noise_level_spin_box.setValue(settings['noise-level'])
        self.waifu2x_converter_cpp_mode_combo_box.setCurrentText(settings['mode'])
        self.waifu2x_converter_cpp_log_level_spin_box.setValue(settings['log-level'])
        self.waifu2x_converter_cpp_disable_gpu_check_box.setChecked(settings['disable-gpu'])
        self.waifu2x_converter_cpp_force_opencl_check_box.setChecked(settings['force-OpenCL'])
        self.waifu2x_converter_cpp_tta_check_box.setChecked(bool(settings['tta']))

        # waifu2x-ncnn-vulkan
        settings = self.config['waifu2x_ncnn_vulkan']
        self.waifu2x_ncnn_vulkan_path_line_edit.setText(str(pathlib.Path(os.path.expandvars(settings['path'])).absolute()))
        self.waifu2x_ncnn_vulkan_noise_level_spin_box.setValue(settings['n'])
        self.waifu2x_ncnn_vulkan_tile_size_spin_box.setValue(settings['t'])
        self.waifu2x_ncnn_vulkan_gpu_id_spin_box.setValue(settings['g'])
        self.waifu2x_ncnn_vulkan_jobs_line_edit.setText(settings['j'])
        self.waifu2x_ncnn_vulkan_tta_check_box.setChecked(settings['x'])

        # srmd-ncnn-vulkan
        settings = self.config['srmd_ncnn_vulkan']
        self.srmd_ncnn_vulkan_path_line_edit.setText(str(pathlib.Path(os.path.expandvars(settings['path'])).absolute()))
        self.srmd_ncnn_vulkan_noise_level_spin_box.setValue(settings['n'])
        self.srmd_ncnn_vulkan_tile_size_spin_box.setValue(settings['t'])
        self.srmd_ncnn_vulkan_gpu_id_spin_box.setValue(settings['g'])
        self.srmd_ncnn_vulkan_jobs_line_edit.setText(settings['j'])
        self.srmd_ncnn_vulkan_tta_check_box.setChecked(settings['x'])

        # realsr-ncnn-vulkan
        settings = self.config['realsr_ncnn_vulkan']
        self.realsr_ncnn_vulkan_path_line_edit.setText(str(pathlib.Path(os.path.expandvars(settings['path'])).absolute()))
        self.realsr_ncnn_vulkan_tile_size_spin_box.setValue(settings['t'])
        self.realsr_ncnn_vulkan_gpu_id_spin_box.setValue(settings['g'])
        self.realsr_ncnn_vulkan_jobs_line_edit.setText(settings['j'])
        self.realsr_ncnn_vulkan_tta_check_box.setChecked(settings['x'])

        # anime4k
        settings = self.config['anime4kcpp']
        self.anime4kcpp_path_line_edit.setText(str(pathlib.Path(os.path.expandvars(settings['path'])).absolute()))
        self.anime4kcpp_passes_spin_box.setValue(settings['passes'])
        self.anime4kcpp_push_color_count_spin_box.setValue(settings['pushColorCount'])
        self.anime4kcpp_strength_color_spin_box.setValue(settings['strengthColor'])
        self.anime4kcpp_strength_gradient_spin_box.setValue(settings['strengthGradient'])
        self.anime4kcpp_threads_spin_box.setValue(settings['threads'])
        self.anime4kcpp_pre_filters_spin_box.setValue(settings['preFilters'])
        self.anime4kcpp_post_filters_spin_box.setValue(settings['postFilters'])
        self.anime4kcpp_platform_id_spin_box.setValue(settings['platformID'])
        self.anime4kcpp_device_id_spin_box.setValue(settings['deviceID'])
        self.anime4kcpp_codec_combo_box.setCurrentText(settings['codec'])
        self.anime4kcpp_fast_mode_check_box.setChecked(settings['fastMode'])
        self.anime4kcpp_pre_processing_check_box.setChecked(settings['preprocessing'])
        self.anime4kcpp_post_processing_check_box.setChecked(settings['postprocessing'])
        self.anime4kcpp_gpu_mode_check_box.setChecked(settings['GPUMode'])
        self.anime4kcpp_cnn_mode_check_box.setChecked(settings['CNNMode'])
        self.anime4kcpp_hdn_check_box.setChecked(settings['HDN'])
        self.anime4kcpp_hdn_level_spin_box.setValue(settings['HDNLevel'])
        self.anime4kcpp_force_fps_double_spin_box.setValue(settings['forceFps'])
        self.anime4kcpp_disable_progress_check_box.setChecked(settings['disableProgress'])
        self.anime4kcpp_alpha_check_box.setChecked(settings['alpha'])

        # ffmpeg
        # global options
        settings = self.config['ffmpeg']
        self.ffmpeg_path_line_edit.setText(str(pathlib.Path(os.path.expandvars(settings['ffmpeg_path'])).absolute()))
        self.ffmpeg_intermediate_file_name_line_edit.setText(settings['intermediate_file_name'])

        # extract frames
        settings = self.config['ffmpeg']['extract_frames']
        self.ffmpeg_extract_frames_output_options_pixel_format_line_edit.setText(settings['output_options']['-pix_fmt'])

        # assemble video
        settings = self.config['ffmpeg']['assemble_video']
        self.ffmpeg_assemble_video_input_options_force_format_line_edit.setText(settings['input_options']['-f'])
        self.ffmpeg_assemble_video_output_options_video_codec_line_edit.setText(settings['output_options']['-vcodec'])
        self.ffmpeg_assemble_video_output_options_pixel_format_line_edit.setText(settings['output_options']['-pix_fmt'])
        self.ffmpeg_assemble_video_output_options_crf_spin_box.setValue(settings['output_options']['-crf'])
        self.ffmpeg_assemble_video_output_options_tune_combo_box.setCurrentText(settings['output_options']['-tune'])
        self.ffmpeg_assemble_video_output_options_bitrate_line_edit.setText(settings['output_options']['-b:v'])

        # migrate streams
        settings = self.config['ffmpeg']['migrate_streams']
        self.ffmpeg_migrate_streams_output_options_pixel_format_line_edit.setText(settings['output_options']['-pix_fmt'])

        # Gifski
        settings = self.config['gifski']
        self.gifski_path_line_edit.setText(str(pathlib.Path(os.path.expandvars(settings['gifski_path'])).absolute()))
        self.gifski_quality_spin_box.setValue(settings['quality'])
        self.gifski_fast_check_box.setChecked(settings['fast'])
        self.gifski_once_check_box.setChecked(settings['once'])
        self.gifski_quiet_check_box.setChecked(settings['quiet'])

    def resolve_driver_settings(self):

        # waifu2x-caffe
        self.config['waifu2x_caffe']['path'] = os.path.expandvars(self.waifu2x_caffe_path_line_edit.text())
        self.config['waifu2x_caffe']['mode'] = self.waifu2x_caffe_mode_combo_box.currentText()
        self.config['waifu2x_caffe']['noise_level'] = self.waifu2x_caffe_noise_level_spin_box.value()
        self.config['waifu2x_caffe']['process'] = self.waifu2x_caffe_process_combo_box.currentText()
        self.config['waifu2x_caffe']['model_dir'] = str((pathlib.Path(self.config['waifu2x_caffe']['path']).parent / 'models' / self.waifu2x_caffe_model_combobox.currentText()).absolute())
        self.config['waifu2x_caffe']['crop_size'] = self.waifu2x_caffe_crop_size_spin_box.value()
        self.config['waifu2x_caffe']['output_quality'] = self.waifu2x_caffe_output_quality_spin_box.value()
        self.config['waifu2x_caffe']['output_depth'] = self.waifu2x_caffe_output_depth_spin_box.value()
        self.config['waifu2x_caffe']['batch_size'] = self.waifu2x_caffe_batch_size_spin_box.value()
        self.config['waifu2x_caffe']['gpu'] = self.waifu2x_caffe_gpu_spin_box.value()
        self.config['waifu2x_caffe']['tta'] = int(self.waifu2x_caffe_tta_check_box.isChecked())

        # waifu2x-converter-cpp
        self.config['waifu2x_converter_cpp']['path'] = os.path.expandvars(self.waifu2x_converter_cpp_path_line_edit.text())
        self.config['waifu2x_converter_cpp']['png-compression'] = self.waifu2x_converter_cpp_png_compression_spin_box.value()
        self.config['waifu2x_converter_cpp']['image-quality'] = self.waifu2x_converter_cpp_image_quality_spin_box.value()
        self.config['waifu2x_converter_cpp']['block-size'] = self.waifu2x_converter_cpp_block_size_spin_box.value()
        self.config['waifu2x_converter_cpp']['processor'] = self.waifu2x_converter_cpp_processor_spin_box.value()
        self.config['waifu2x_converter_cpp']['model-dir'] = str((pathlib.Path(self.config['waifu2x_converter_cpp']['path']).parent / self.waifu2x_converter_cpp_model_combo_box.currentText()).absolute())
        self.config['waifu2x_converter_cpp']['noise-level'] = self.waifu2x_converter_cpp_noise_level_spin_box.value()
        self.config['waifu2x_converter_cpp']['mode'] = self.waifu2x_converter_cpp_mode_combo_box.currentText()
        self.config['waifu2x_converter_cpp']['log-level'] = self.waifu2x_converter_cpp_log_level_spin_box.value()
        self.config['waifu2x_converter_cpp']['disable-gpu'] = bool(self.waifu2x_converter_cpp_disable_gpu_check_box.isChecked())
        self.config['waifu2x_converter_cpp']['force-OpenCL'] = bool(self.waifu2x_converter_cpp_force_opencl_check_box.isChecked())
        self.config['waifu2x_converter_cpp']['tta'] = int(self.waifu2x_converter_cpp_tta_check_box.isChecked())

        # waifu2x-ncnn-vulkan
        self.config['waifu2x_ncnn_vulkan']['path'] = os.path.expandvars(self.waifu2x_ncnn_vulkan_path_line_edit.text())
        self.config['waifu2x_ncnn_vulkan']['n'] = self.waifu2x_ncnn_vulkan_noise_level_spin_box.value()
        self.config['waifu2x_ncnn_vulkan']['t'] = self.waifu2x_ncnn_vulkan_tile_size_spin_box.value()
        self.config['waifu2x_ncnn_vulkan']['m'] = str((pathlib.Path(self.config['waifu2x_ncnn_vulkan']['path']).parent / self.waifu2x_ncnn_vulkan_model_combo_box.currentText()).absolute())
        self.config['waifu2x_ncnn_vulkan']['g'] = self.waifu2x_ncnn_vulkan_gpu_id_spin_box.value()
        self.config['waifu2x_ncnn_vulkan']['j'] = self.waifu2x_ncnn_vulkan_jobs_line_edit.text()
        self.config['waifu2x_ncnn_vulkan']['x'] = self.waifu2x_ncnn_vulkan_tta_check_box.isChecked()

        # srmd-ncnn-vulkan
        self.config['srmd_ncnn_vulkan']['path'] = os.path.expandvars(self.srmd_ncnn_vulkan_path_line_edit.text())
        self.config['srmd_ncnn_vulkan']['n'] = self.srmd_ncnn_vulkan_noise_level_spin_box.value()
        self.config['srmd_ncnn_vulkan']['t'] = self.srmd_ncnn_vulkan_tile_size_spin_box.value()
        self.config['srmd_ncnn_vulkan']['m'] = str((pathlib.Path(self.config['srmd_ncnn_vulkan']['path']).parent / self.srmd_ncnn_vulkan_model_combo_box.currentText()).absolute())
        self.config['srmd_ncnn_vulkan']['g'] = self.srmd_ncnn_vulkan_gpu_id_spin_box.value()
        self.config['srmd_ncnn_vulkan']['j'] = self.srmd_ncnn_vulkan_jobs_line_edit.text()
        self.config['srmd_ncnn_vulkan']['x'] = self.srmd_ncnn_vulkan_tta_check_box.isChecked()

        # realsr-ncnn-vulkan
        self.config['realsr_ncnn_vulkan']['path'] = os.path.expandvars(self.realsr_ncnn_vulkan_path_line_edit.text())
        self.config['realsr_ncnn_vulkan']['t'] = self.realsr_ncnn_vulkan_tile_size_spin_box.value()
        self.config['realsr_ncnn_vulkan']['m'] = str((pathlib.Path(self.config['realsr_ncnn_vulkan']['path']).parent / self.realsr_ncnn_vulkan_model_combo_box.currentText()).absolute())
        self.config['realsr_ncnn_vulkan']['g'] = self.realsr_ncnn_vulkan_gpu_id_spin_box.value()
        self.config['realsr_ncnn_vulkan']['j'] = self.realsr_ncnn_vulkan_jobs_line_edit.text()
        self.config['realsr_ncnn_vulkan']['x'] = self.realsr_ncnn_vulkan_tta_check_box.isChecked()

        # anime4k
        self.config['anime4kcpp']['path'] = os.path.expandvars(self.anime4kcpp_path_line_edit.text())
        self.config['anime4kcpp']['passes'] = self.anime4kcpp_passes_spin_box.value()
        self.config['anime4kcpp']['pushColorCount'] = self.anime4kcpp_push_color_count_spin_box.value()
        self.config['anime4kcpp']['strengthColor'] = self.anime4kcpp_strength_color_spin_box.value()
        self.config['anime4kcpp']['strengthGradient'] = self.anime4kcpp_strength_gradient_spin_box.value()
        self.config['anime4kcpp']['threads'] = self.anime4kcpp_threads_spin_box.value()
        self.config['anime4kcpp']['preFilters'] = self.anime4kcpp_pre_filters_spin_box.value()
        self.config['anime4kcpp']['postFilters'] = self.anime4kcpp_post_filters_spin_box.value()
        self.config['anime4kcpp']['platformID'] = self.anime4kcpp_platform_id_spin_box.value()
        self.config['anime4kcpp']['deviceID'] = self.anime4kcpp_device_id_spin_box.value()
        self.config['anime4kcpp']['codec'] = self.anime4kcpp_codec_combo_box.currentText()
        self.config['anime4kcpp']['fastMode'] = bool(self.anime4kcpp_fast_mode_check_box.isChecked())
        self.config['anime4kcpp']['preprocessing'] = bool(self.anime4kcpp_pre_processing_check_box.isChecked())
        self.config['anime4kcpp']['postprocessing'] = bool(self.anime4kcpp_post_processing_check_box.isChecked())
        self.config['anime4kcpp']['GPUMode'] = bool(self.anime4kcpp_gpu_mode_check_box.isChecked())
        self.config['anime4kcpp']['CNNMode'] = bool(self.anime4kcpp_cnn_mode_check_box.isChecked())
        self.config['anime4kcpp']['HDN'] = bool(self.anime4kcpp_hdn_check_box.isChecked())
        self.config['anime4kcpp']['HDNLevel'] = self.anime4kcpp_hdn_level_spin_box.value()
        self.config['anime4kcpp']['forceFps'] = self.anime4kcpp_force_fps_double_spin_box.value()
        self.config['anime4kcpp']['disableProgress'] = bool(self.anime4kcpp_disable_progress_check_box.isChecked())
        self.config['anime4kcpp']['alpha'] = bool(self.anime4kcpp_alpha_check_box.isChecked())

        # ffmpeg
        self.config['ffmpeg']['ffmpeg_path'] = os.path.expandvars(self.ffmpeg_path_line_edit.text())
        self.config['ffmpeg']['intermediate_file_name'] = self.ffmpeg_intermediate_file_name_line_edit.text()

        # extract frames
        self.config['ffmpeg']['extract_frames']['output_options']['-pix_fmt'] = self.ffmpeg_extract_frames_output_options_pixel_format_line_edit.text()
        if self.ffmpeg_extract_frames_hardware_acceleration_check_box.isChecked():
            self.config['ffmpeg']['extract_frames']['-hwaccel'] = 'auto'
        else:
            self.config['ffmpeg']['extract_frames'].pop('-hwaccel', None)

        # assemble video
        self.config['ffmpeg']['assemble_video']['input_options']['-f'] = self.ffmpeg_assemble_video_input_options_force_format_line_edit.text()
        self.config['ffmpeg']['assemble_video']['output_options']['-vcodec'] = self.ffmpeg_assemble_video_output_options_video_codec_line_edit.text()
        self.config['ffmpeg']['assemble_video']['output_options']['-pix_fmt'] = self.ffmpeg_assemble_video_output_options_pixel_format_line_edit.text()
        self.config['ffmpeg']['assemble_video']['output_options']['-crf'] = self.ffmpeg_assemble_video_output_options_crf_spin_box.value()
        self.config['ffmpeg']['assemble_video']['output_options']['-tune'] = self.ffmpeg_assemble_video_output_options_tune_combo_box.currentText()
        if self.ffmpeg_assemble_video_output_options_bitrate_line_edit.text() != '':
            self.config['ffmpeg']['assemble_video']['output_options']['-b:v'] = self.ffmpeg_assemble_video_output_options_bitrate_line_edit.text()
        else:
            self.config['ffmpeg']['assemble_video']['output_options']['-b:v'] = None

        if self.ffmpeg_assemble_video_output_options_ensure_divisible_check_box.isChecked():
            # if video filter is enabled and is not empty and is not equal to divisible by two filter
            # append divisible by two filter to the end of existing filter
            if ('-vf' in self.config['ffmpeg']['assemble_video']['output_options'] and
                    len(self.config['ffmpeg']['assemble_video']['output_options']['-vf']) > 0 and
                    self.config['ffmpeg']['assemble_video']['output_options']['-vf'] != 'pad=ceil(iw/2)*2:ceil(ih/2)*2'):
                self.config['ffmpeg']['assemble_video']['output_options']['-vf'] += ',pad=ceil(iw/2)*2:ceil(ih/2)*2'
            else:
                self.config['ffmpeg']['assemble_video']['output_options']['-vf'] = 'pad=ceil(iw/2)*2:ceil(ih/2)*2'
        else:
            self.config['ffmpeg']['assemble_video']['output_options'].pop('-vf', None)

        if self.ffmpeg_assemble_video_hardware_acceleration_check_box.isChecked():
            self.config['ffmpeg']['assemble_video']['-hwaccel'] = 'auto'
        else:
            self.config['ffmpeg']['assemble_video'].pop('-hwaccel', None)

        # migrate streams

        self.config['ffmpeg']['migrate_streams']['output_options']['-map'] = []
        if self.ffmpeg_migrate_streams_output_options_mapping_video_check_box_check_box.isChecked():
            self.config['ffmpeg']['migrate_streams']['output_options']['-map'].append('0:v?')
        if self.ffmpeg_migrate_streams_output_options_mapping_audio_check_box_check_box.isChecked():
            self.config['ffmpeg']['migrate_streams']['output_options']['-map'].append('1:a?')
        if self.ffmpeg_migrate_streams_output_options_mapping_subtitle_check_box_check_box.isChecked():
            self.config['ffmpeg']['migrate_streams']['output_options']['-map'].append('1:s?')
        if self.ffmpeg_migrate_streams_output_options_mapping_data_check_box_check_box.isChecked():
            self.config['ffmpeg']['migrate_streams']['output_options']['-map'].append('1:d?')
        if self.ffmpeg_migrate_streams_output_options_mapping_font_check_box_check_box.isChecked():
            self.config['ffmpeg']['migrate_streams']['output_options']['-map'].append('1:t?')

        # if the list is empty, delete the key
        # otherwise parser will run into an error (key with no value)
        if len(self.config['ffmpeg']['migrate_streams']['output_options']['-map']) == 0:
            self.config['ffmpeg']['migrate_streams']['output_options'].pop('-map', None)

        self.config['ffmpeg']['migrate_streams']['output_options']['-pix_fmt'] = self.ffmpeg_migrate_streams_output_options_pixel_format_line_edit.text()

        fps = self.ffmpeg_migrate_streams_output_options_frame_interpolation_spin_box.value()
        if fps > 0:
            if ('-vf' in self.config['ffmpeg']['migrate_streams']['output_options'] and
                    len(self.config['ffmpeg']['migrate_streams']['output_options']['-vf']) > 0 and
                    'minterpolate=' not in self.config['ffmpeg']['migrate_streams']['output_options']['-vf']):
                self.config['ffmpeg']['migrate_streams']['output_options']['-vf'] += f',minterpolate=\'fps={fps}\''
            else:
                self.config['ffmpeg']['migrate_streams']['output_options']['-vf'] = f'minterpolate=\'fps={fps}\''
        else:
            self.config['ffmpeg']['migrate_streams']['output_options'].pop('-vf', None)

        # copy source codec
        if self.ffmpeg_migrate_streams_output_options_copy_streams_check_box.isChecked():
            self.config['ffmpeg']['migrate_streams']['output_options']['-c'] = 'copy'
        else:
            self.config['ffmpeg']['migrate_streams']['output_options'].pop('-c', None)

        # copy known metadata
        if self.ffmpeg_migrate_streams_output_options_copy_known_metadata_tags_check_box.isChecked():
            self.config['ffmpeg']['migrate_streams']['output_options']['-map_metadata'] = 0
        else:
            self.config['ffmpeg']['migrate_streams']['output_options'].pop('-map_metadata', None)

        # copy arbitrary metadata
        if self.ffmpeg_migrate_streams_output_options_copy_arbitrary_metadata_tags_check_box.isChecked():
            self.config['ffmpeg']['migrate_streams']['output_options']['-movflags'] = 'use_metadata_tags'
        else:
            self.config['ffmpeg']['migrate_streams']['output_options'].pop('-movflags', None)

        # hardware acceleration
        if self.ffmpeg_migrate_streams_hardware_acceleration_check_box.isChecked():
            self.config['ffmpeg']['migrate_streams']['-hwaccel'] = 'auto'
        else:
            self.config['ffmpeg']['migrate_streams'].pop('-hwaccel', None)

        # Gifski
        self.config['gifski']['gifski_path'] = os.path.expandvars(self.gifski_path_line_edit.text())
        self.config['gifski']['quality'] = self.gifski_quality_spin_box.value()
        self.config['gifski']['fast'] = self.gifski_fast_check_box.isChecked()
        self.config['gifski']['once'] = self.gifski_once_check_box.isChecked()
        self.config['gifski']['quiet'] = self.gifski_quiet_check_box.isChecked()

    def dragEnterEvent(self, event):
        if event.mimeData().hasUrls():
            event.accept()
        else:
            event.ignore()

    def dropEvent(self, event):
        input_paths = [pathlib.Path(u.toLocalFile()) for u in event.mimeData().urls()]
        for path in input_paths:
            if (path.is_file() or path.is_dir()) and not self.input_table_path_exists(path):
                self.input_table_data.append(path)

        self.update_output_path()
        self.update_input_table()

    def enable_line_edit_file_drop(self, line_edit: QLineEdit):
        line_edit.dragEnterEvent = self.dragEnterEvent
        line_edit.dropEvent = lambda event: line_edit.setText(str(pathlib.Path(event.mimeData().urls()[0].toLocalFile()).absolute()))

    def show_ffprobe_output(self, event):
        input_paths = [pathlib.Path(u.toLocalFile()) for u in event.mimeData().urls()]
        if not input_paths[0].is_file():
            return

        ffmpeg_object = Ffmpeg(self.ffmpeg_settings)
        file_info_json = ffmpeg_object.probe_file_info(input_paths[0])
        self.ffprobe_plain_text_edit.setPlainText(json.dumps(file_info_json, indent=2))

    @staticmethod
    def read_config(config_file: pathlib.Path) -> dict:
        """ read video2x configurations from config file

        Arguments:
            config_file {pathlib.Path} -- video2x configuration file pathlib.Path

        Returns:
            dict -- dictionary of video2x configuration
        """

        with open(config_file, 'r') as config:
            return yaml.load(config, Loader=yaml.FullLoader)

    def mutually_exclude_scale_ratio_resolution(self):
        if self.output_width_spin_box.value() != 0 or self.output_height_spin_box.value() != 0:
            self.scale_ratio_double_spin_box.setDisabled(True)
        elif self.output_width_spin_box.value() == 0 and self.output_height_spin_box.value() == 0:
            self.scale_ratio_double_spin_box.setDisabled(False)

    def mutually_exclude_frame_interpolation_stream_copy(self):
        if self.ffmpeg_migrate_streams_output_options_frame_interpolation_spin_box.value() > 0:
            self.ffmpeg_migrate_streams_output_options_copy_streams_check_box.setChecked(False)
            self.ffmpeg_migrate_streams_output_options_copy_streams_check_box.setDisabled(True)
        else:
            self.ffmpeg_migrate_streams_output_options_copy_streams_check_box.setChecked(True)
            self.ffmpeg_migrate_streams_output_options_copy_streams_check_box.setDisabled(False)

    def update_gui_for_driver(self):
        current_driver = AVAILABLE_DRIVERS[self.driver_combo_box.currentText()]

        # update preferred processes/threads count
        if current_driver == 'anime4kcpp':
            self.processes_spin_box.setValue(16)
        else:
            self.processes_spin_box.setValue(1)

    def update_input_table(self):
        # HACK: use insertRow, removeRow and signals
        del self.input_table_model
        self.input_table_model = InputTableModel(self.input_table_data)
        self.input_table_view.setModel(self.input_table_model)

    def input_table_delete_selected(self):
        indexes_to_delete = [i.row() for i in self.input_table_view.selectedIndexes()]
        for index in sorted(indexes_to_delete, reverse=True):
            del self.input_table_data[index]

        self.update_output_path()
        self.update_input_table()

    def input_table_clear_all(self):
        self.input_table_data = []
        self.update_output_path()
        self.update_input_table()

    def input_table_path_exists(self, input_path: pathlib.Path) -> bool:
        for path in self.input_table_data:
            # not using Path.samefile since file may not exist
            if str(path.absolute()) == str(input_path.absolute()):
                return True
        return False

    def select_file(self, *args, **kwargs) -> pathlib.Path:
        file_selected = QFileDialog.getOpenFileName(self, *args, **kwargs)
        if not isinstance(file_selected, tuple) or file_selected[0] == '':
            return None
        return pathlib.Path(file_selected[0])

    def select_folder(self, *args, **kwargs) -> pathlib.Path:
        folder_selected = QFileDialog.getExistingDirectory(self, *args, **kwargs)
        if folder_selected == '':
            return None
        return pathlib.Path(folder_selected)

    def select_save_file(self, *args, **kwargs) -> pathlib.Path:
        save_file_selected = QFileDialog.getSaveFileName(self, *args, **kwargs)
        if not isinstance(save_file_selected, tuple) or save_file_selected[0] == '':
            return None
        return pathlib.Path(save_file_selected[0])

    def update_output_path(self):
        # if input list is empty
        # clear output path
        if len(self.input_table_data) == 0:
            self.output_line_edit.setText('')

        # if there are multiple output files
        # use cwd/output directory for output
        elif len(self.input_table_data) > 1:
            self.output_line_edit.setText(str((CWD / 'output').absolute()))

        # if there's only one input file
        # generate output file/directory name automatically
        elif len(self.input_table_data) == 1:
            input_path = self.input_table_data[0]
            # give up if input path doesn't exist or isn't a file or a directory
            if not input_path.exists() or not (input_path.is_file() or input_path.is_dir()):
                return

            if input_path.is_file():

                # generate suffix automatically
                try:
                    input_file_mime_type = magic.from_file(str(input_path.absolute()), mime=True)
                    input_file_type = input_file_mime_type.split('/')[0]
                    input_file_subtype = input_file_mime_type.split('/')[1]
                except Exception:
                    input_file_type = input_file_subtype = None

                # in case python-magic fails to detect file type
                # try guessing file mime type with mimetypes
                if input_file_type not in ['image', 'video']:
                    input_file_mime_type = mimetypes.guess_type(input_path.name)[0]
                    input_file_type = input_file_mime_type.split('/')[0]
                    input_file_subtype = input_file_mime_type.split('/')[1]

                # if input file is an image
                if input_file_type == 'image':

                    # if file is a gif, use .gif
                    if input_file_subtype == 'gif':
                        suffix = '.gif'

                    # otherwise, use .png by default for all images
                    else:
                        suffix = self.image_output_extension_line_edit.text()

                # if input is video, use .mp4 as output by default
                elif input_file_type == 'video':
                    suffix = self.video_output_extension_line_edit.text()

                # if failed to detect file type
                # use input file's suffix
                else:
                    suffix = input_path.suffix

                output_path = input_path.parent / self.output_file_name_format_string_line_edit.text().format(original_file_name=input_path.stem, extension=suffix)

            elif input_path.is_dir():
                output_path = input_path.parent / self.output_file_name_format_string_line_edit.text().format(original_file_name=input_path.stem, extension='')

            # try a new name with a different file ID
            output_path_id = 0
            while output_path.exists():
                if input_path.is_file():
                    output_path = input_path.parent / pathlib.Path(f'{input_path.stem}_output_{output_path_id}{suffix}')
                elif input_path.is_dir():
                    output_path = input_path.parent / pathlib.Path(f'{input_path.stem}_output_{output_path_id}')
                output_path_id += 1

            if not output_path.exists():
                self.output_line_edit.setText(str(output_path.absolute()))

    def select_input_file(self):
        input_file = self.select_file('Select Input File')
        if (input_file is None or self.input_table_path_exists(input_file)):
            return
        self.input_table_data.append(input_file)
        self.update_output_path()
        self.update_input_table()

    def select_input_folder(self):
        input_folder = self.select_folder('Select Input Folder')
        if (input_folder is None or self.input_table_path_exists(input_folder)):
            return
        self.input_table_data.append(input_folder)
        self.update_output_path()
        self.update_input_table()

    def select_output_file(self):
        output_file = self.select_file('Select Output File')
        if output_file is None:
            return
        self.output_line_edit.setText(str(output_file.absolute()))

    def select_output_folder(self):
        output_folder = self.select_folder('Select Output Folder')
        if output_folder is None:
            return
        self.output_line_edit.setText(str(output_folder.absolute()))

    def select_cache_folder(self):
        cache_folder = self.select_folder('Select Cache Folder')
        if cache_folder is None:
            return
        self.cache_line_edit.setText(str(cache_folder.absolute()))

    def select_config_file(self):
        config_file = self.select_file('Select Config File', filter='(YAML files (*.yaml))')
        if config_file is None:
            return
        self.config_line_edit.setText(str(config_file.absolute()))
        self.load_configurations()

    def select_driver_binary_path(self, driver_line_edit: QLineEdit):
        driver_binary_path = self.select_file('Select Driver Binary File')
        if driver_binary_path is None:
            return
        driver_line_edit.setText(str(driver_binary_path.absolute()))

    def show_shortcuts(self):
        message_box = QMessageBox(self)
        message_box.setWindowTitle('Video2X Shortcuts')
        message_box.setTextFormat(Qt.MarkdownText)
        shortcut_information = '''**Ctrl+W**:\tExit application\\
**Ctrl+Q**:\tExit application\\
**Ctrl+I**:\tOpen select input file dialog\\
**Ctrl+O**:\tOpen select output file dialog\\
**Ctrl+Shift+I**:\tOpen select input folder dialog\\
**Ctrl+Shift+O**:\tOpen select output folder dialog'''
        message_box.setText(shortcut_information)
        message_box.exec_()

    def show_about(self):
        message_box = QMessageBox(self)
        message_box.setWindowTitle('About Video2X')
        message_box.setIconPixmap(QPixmap(self.video2x_icon_path).scaled(64, 64))
        message_box.setTextFormat(Qt.MarkdownText)
        message_box.setText(LEGAL_INFO)
        message_box.exec_()

    def show_information(self, message: str):
        message_box = QMessageBox(self)
        message_box.setWindowTitle('Information')
        message_box.setIcon(QMessageBox.Information)
        message_box.setText(message)
        message_box.exec_()

    def show_warning(self, message: str):
        message_box = QMessageBox(self)
        message_box.setWindowTitle('Warning')
        message_box.setIcon(QMessageBox.Warning)
        message_box.setText(message)
        message_box.exec_()

    def show_error(self, exception: Exception):

        def _process_button_press(button_pressed):
            # if the user pressed the save button, save log file to destination
            if button_pressed.text() == 'Save':
                log_file_saving_path = self.select_save_file('Select Log File Saving Destination', 'video2x_error.log')
                if log_file_saving_path is not None:
                    with open(log_file_saving_path, 'w', encoding='utf-8') as log_file:
                        self.log_file.seek(0)
                        log_file.write(self.log_file.read())

        # QErrorMessage(self).showMessage(message.replace('\n', '<br>'))
        message_box = QMessageBox(self)
        message_box.setWindowTitle('Error')
        message_box.setIcon(QMessageBox.Critical)
        message_box.setTextFormat(Qt.MarkdownText)

        error_message = '''Upscaler ran into an error:\\
{}\\
Check the console output or the log file for details.\\
You can [submit an issue on GitHub](https://github.com/k4yt3x/video2x/issues/new?assignees=K4YT3X&labels=bug&template=bug-report.md&title={}) to report this error.\\
It\'s highly recommended to attach the log file.\\
You can click \"Save\" to save the log file.'''
        message_box.setText(error_message.format(exception, urllib.parse.quote(str(exception))))

        message_box.setStandardButtons(QMessageBox.Save | QMessageBox.Close)
        message_box.setDefaultButton(QMessageBox.Save)
        message_box.buttonClicked.connect(_process_button_press)
        message_box.exec_()

    def progress_monitor(self, progress_callback: pyqtSignal):

        # initialize progress bar values
        progress_callback.emit((time.time(), 0, 0, 0, 0, 0, [], pathlib.Path(), pathlib.Path()))

        # keep querying upscaling process and feed information to callback signal
        while self.upscaler.running:

            progress_callback.emit((self.upscaler.current_processing_starting_time,
                                    self.upscaler.total_frames_upscaled,
                                    self.upscaler.total_frames,
                                    self.upscaler.total_processed,
                                    self.upscaler.total_files,
                                    self.upscaler.current_pass,
                                    self.upscaler.scaling_jobs,
                                    self.upscaler.current_input_file,
                                    self.upscaler.last_frame_upscaled))
            time.sleep(1)

        # upscale process will stop at 99%
        # so it's set to 100 manually when all is done
        progress_callback.emit((time.time(),
                                self.upscaler.total_frames,
                                self.upscaler.total_frames,
                                self.upscaler.total_files,
                                self.upscaler.total_files,
                                len(self.upscaler.scaling_jobs),
                                self.upscaler.scaling_jobs,
                                pathlib.Path(),
                                pathlib.Path()))

    def set_progress(self, progress_information: tuple):
        current_processing_starting_time = progress_information[0]
        total_frames_upscaled = progress_information[1]
        total_frames = progress_information[2]
        total_processed = progress_information[3]
        total_files = progress_information[4]
        current_pass = progress_information[5]
        scaling_jobs = progress_information[6]
        current_input_file = progress_information[7]
        last_frame_upscaled = progress_information[8]

        # calculate fields based on frames and time elapsed
        time_elapsed = time.time() - current_processing_starting_time
        try:
            rate = total_frames_upscaled / time_elapsed
            time_remaining = (total_frames - total_frames_upscaled) / rate
        except Exception:
            rate = 0.0
            time_remaining = 0.0

        # set calculated values in GUI
        self.current_progress_bar.setMaximum(total_frames)
        self.current_progress_bar.setValue(total_frames_upscaled)
        self.frames_label.setText('Frames: {}/{}'.format(total_frames_upscaled, total_frames))
        self.time_elapsed_label.setText('Time Elapsed: {}'.format(time.strftime("%H:%M:%S", time.gmtime(time_elapsed))))
        self.time_remaining_label.setText('Time Remaining: {}'.format(time.strftime("%H:%M:%S", time.gmtime(time_remaining))))
        self.rate_label.setText('Rate (FPS): {}'.format(round(rate, 2)))
        self.overall_progress_label.setText('Overall Progress: {}/{}'.format(total_processed, total_files))
        self.overall_progress_bar.setMaximum(total_files)
        self.overall_progress_bar.setValue(total_processed)
        self.currently_processing_label.setText('Currently Processing: {} (pass {}/{})'.format(str(current_input_file.name), current_pass, len(scaling_jobs)))

        # if show frame is checked, show preview image
        if self.frame_preview_show_preview_check_box.isChecked() and last_frame_upscaled.is_file():
            last_frame_pixmap = QPixmap(str(last_frame_upscaled.absolute()))
            # the -2 here behind geometry subtracts frame size from width and height
            self.frame_preview_label.setPixmap(last_frame_pixmap.scaled(self.frame_preview_label.width() - 2,
                                                                        self.frame_preview_label.height() - 2,
                                                                        Qt.KeepAspectRatio))

            # if keep aspect ratio is checked, don't stretch image
            if self.frame_preview_keep_aspect_ratio_check_box.isChecked():
                self.frame_preview_label.setScaledContents(False)
            else:
                self.frame_preview_label.setScaledContents(True)

            # display image in label
            self.frame_preview_label.show()

        # if show frame is unchecked, clear image
        elif self.frame_preview_show_preview_check_box.isChecked() is False:
            self.frame_preview_label.clear()

    def reset_progress_display(self):
        # reset progress display UI elements
        self.current_progress_bar.setMaximum(100)
        self.current_progress_bar.setValue(0)
        self.frames_label.setText('Frames: {}/{}'.format(0, 0))
        self.time_elapsed_label.setText('Time Elapsed: {}'.format(time.strftime("%H:%M:%S", time.gmtime(0))))
        self.time_remaining_label.setText('Time Remaining: {}'.format(time.strftime("%H:%M:%S", time.gmtime(0))))
        self.rate_label.setText('Rate (FPS): {}'.format(0.0))
        self.overall_progress_label.setText('Overall Progress: {}/{}'.format(0, 0))
        self.overall_progress_bar.setMaximum(100)
        self.overall_progress_bar.setValue(0)
        self.currently_processing_label.setText('Currently Processing:')

    def start(self):

        # start execution
        try:
            # start timer
            self.begin_time = time.time()

            # resolve input and output directories from GUI
            if len(self.input_table_data) == 0:
                self.show_warning('Input path unspecified')
                return
            if self.output_line_edit.text().strip() == '':
                self.show_warning('Output path unspecified')
                return

            if len(self.input_table_data) == 1:
                input_directory = self.input_table_data[0]
            else:
                input_directory = self.input_table_data

            # resolve output directory
            output_directory = pathlib.Path(os.path.expandvars(self.output_line_edit.text()))

            # load driver settings from GUI
            self.resolve_driver_settings()

            # load driver settings for the current driver
            self.driver_settings = self.config[AVAILABLE_DRIVERS[self.driver_combo_box.currentText()]]

            # get scale ratio or resolution
            if self.scale_ratio_double_spin_box.isEnabled():
                scale_ratio = self.scale_ratio_double_spin_box.value()
                scale_width = scale_height = None

            else:
                scale_ratio = None
                scale_width = self.output_width_spin_box.value()
                scale_height = self.output_height_spin_box.value()

            self.upscaler = Upscaler(
                # required parameters
                input_path=input_directory,
                output_path=output_directory,
                driver_settings=self.driver_settings,
                ffmpeg_settings=self.ffmpeg_settings,
                gifski_settings=self.gifski_settings,

                # optional parameters
                driver=AVAILABLE_DRIVERS[self.driver_combo_box.currentText()],
                scale_ratio=scale_ratio,
                scale_width=scale_width,
                scale_height=scale_height,
                processes=self.processes_spin_box.value(),
                video2x_cache_directory=pathlib.Path(os.path.expandvars(self.cache_line_edit.text())),
                extracted_frame_format=self.config['video2x']['extracted_frame_format'].lower(),
                image_output_extension=self.image_output_extension_line_edit.text(),
                video_output_extension=self.video_output_extension_line_edit.text(),
                preserve_frames=bool(self.preserve_frames_check_box.isChecked())
            )

            # run upscaler
            worker = UpscalerWorker(self.upscaler.run)
            worker.signals.error.connect(self.upscale_errored)
            worker.signals.interrupted.connect(self.upscale_interrupted)
            worker.signals.finished.connect(self.upscale_successful)
            self.threadpool.start(worker)

            # start progress monitoring
            progress_bar_worker = ProgressMonitorWorkder(self.progress_monitor)
            progress_bar_worker.signals.progress.connect(self.set_progress)
            self.threadpool.start(progress_bar_worker)

            self.start_button.setEnabled(False)
            self.stop_button.setEnabled(True)

        except Exception as e:
            traceback.print_exc()
            self.upscale_errored(e)

    def upscale_errored(self, exception: Exception):
        # send stop signal in case it's not triggered
        with contextlib.suppress(AttributeError):
            self.upscaler.running = False

        self.show_error(exception)
        self.threadpool.waitForDone(5)
        self.start_button.setEnabled(True)
        self.stop_button.setEnabled(False)
        self.reset_progress_display()

    def upscale_interrupted(self):
        self.show_information('Upscale has been interrupted')
        self.threadpool.waitForDone(5)
        self.start_button.setEnabled(True)
        self.stop_button.setEnabled(False)
        self.reset_progress_display()

    def upscale_successful(self):
        # if all threads have finished
        self.threadpool.waitForDone(5)
        self.show_information('Upscale finished successfully, taking {} seconds'.format(round((time.time() - self.begin_time), 5)))
        self.start_button.setEnabled(True)
        self.stop_button.setEnabled(False)
        self.reset_progress_display()

    def stop(self):

        try:
            # if upscaler is running, ask the user for confirmation
            if self.upscaler.running is True:
                confirmation = QMessageBox.question(self,
                                                    'Stopping Confirmation',
                                                    'Are you sure you want to want to stop the upscaling process?',
                                                    QMessageBox.Yes,
                                                    QMessageBox.No)
                # if the user indeed wants to stop processing
                if confirmation == QMessageBox.Yes:
                    with contextlib.suppress(AttributeError):
                        self.upscaler.running = False
                    return True
                # if the user doesn't want ot stop processing
                else:
                    return False

            # if the upscaler is not running
            else:
                return True

        # if an AttributeError happens
        # that means the upscaler object haven't been created yet
        except AttributeError:
            return True

    def closeEvent(self, event: QCloseEvent):
        # try cleaning up temp directories
        if self.stop():
            event.accept()
        else:
            event.ignore()


# this file shouldn't be imported
if __name__ == '__main__':
    try:
        app = QApplication(sys.argv)
        window = Video2XMainWindow()
        window.show()
        app.exec_()

    # on GUI exception, print error message in console
    # and hold window open using input()
    except Exception:
        traceback.print_exc()
        input('Press enter to close')
