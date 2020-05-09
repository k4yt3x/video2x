#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Creator: Video2X GUI
Author: K4YT3X
Date Created: May 5, 2020
Last Modified: May 8, 2020
"""

# local imports
from upscaler import Upscaler

# built-in imports
import contextlib
import os
import pathlib
import sys
import tempfile
import time
import traceback
import yaml

# third-party imports
from PyQt5 import QtGui, uic
from PyQt5.QtCore import *
from PyQt5.QtWidgets import *
# QObject, pyqtSlot, pyqtSignal, QRunnable, QThreadPool, QAbstractTableModel, Qt

VERSION = '2.0.0'

LEGAL_INFO = f'''Video2X GUI Version: {VERSION}
Author: K4YT3X
License: GNU GPL v3
Github Page: https://github.com/k4yt3x/video2x
Contact: k4yt3x@k4yt3x.com'''

AVAILABLE_DRIVERS = {
    'Waifu2X Caffe': 'waifu2x_caffe',
    'Waifu2X Converter CPP': 'waifu2x_converter_cpp',
    'Waifu2X NCNN Vulkan': 'waifu2x_ncnn_vulkan',
    'SRMD NCNN Vulkan': 'srmd_ncnn_vulkan',
    'Anime4KCPP': 'anime4kcpp'
}


def resource_path(relative_path: str) -> pathlib.Path:
    try:
        base_path = pathlib.Path(sys._MEIPASS)
    except AttributeError:
        base_path = pathlib.Path(__file__).parent
    return base_path / relative_path


class WorkerSignals(QObject):
    progress = pyqtSignal(tuple)
    error = pyqtSignal(str)
    interrupted = pyqtSignal()
    finished = pyqtSignal()


class ProgressBarWorker(QRunnable):
    def __init__(self, fn, *args, **kwargs):
        super(ProgressBarWorker, self).__init__()
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
        except Exception:
            error_message = traceback.format_exc()
            print(error_message, file=sys.stderr)
            self.signals.error.emit(error_message)
        else:
            self.signals.finished.emit()


class InputTableModel(QAbstractTableModel):
    def __init__(self, data):
        super(InputTableModel, self).__init__()
        self._data = data

    def data(self, index, role):
        if role == Qt.DisplayRole:

            if index.column() == 0:
                return str(self._data[index.row()].absolute())
            else:
                if self._data[index.row()].is_file():
                    return 'File'
                elif self._data[index.row()].is_dir():
                    return 'Folder'
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

        # create thread pool for upscaler workers
        self.threadpool = QThreadPool()

        # set window title and icon
        self.video2x_icon_path = str(resource_path('images/video2x.png'))
        self.setWindowTitle(f'Video2X GUI {VERSION}')
        self.setWindowIcon(QtGui.QIcon(self.video2x_icon_path))

        # menu bar
        self.action_exit = self.findChild(QAction, 'actionExit')
        self.action_exit.triggered.connect(sys.exit)

        self.action_about = self.findChild(QAction, 'actionAbout')
        self.action_about.triggered.connect(lambda: self.show_message(LEGAL_INFO, custom_icon=QtGui.QPixmap(self.video2x_icon_path)))

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
        self.output_line_edit.setText(str((pathlib.Path().cwd() / 'output').absolute()))
        self.output_select_file_button = self.findChild(QPushButton, 'outputSelectFileButton')
        self.output_select_file_button.clicked.connect(self.select_output_file)
        self.output_select_folder_button = self.findChild(QPushButton, 'outputSelectFolderButton')
        self.output_select_folder_button.clicked.connect(self.select_output_folder)

        # config file
        self.config_line_edit = self.findChild(QLineEdit, 'configLineEdit')
        self.enable_line_edit_file_drop(self.config_line_edit)
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
        self.preserve_frames_check_box = self.findChild(QCheckBox, 'preserveFramesCheckBox')

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
        self.waifu2x_converter_cpp_processor_spin_box = self.findChild(QSpinBox, 'waifu2xConverterCppProcessorSpinBox')
        self.waifu2x_converter_cpp_model_combo_box = self.findChild(QComboBox, 'waifu2xConverterCppModelComboBox')
        self.waifu2x_converter_cpp_mode_combo_box = self.findChild(QComboBox, 'waifu2xConverterCppModeComboBox')
        self.waifu2x_converter_cpp_disable_gpu_check_box = self.findChild(QCheckBox, 'disableGpuCheckBox')
        self.waifu2x_converter_cpp_tta_check_box = self.findChild(QCheckBox, 'ttaCheckBox')

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

        # load configurations
        self.load_configurations()

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

        self.update_input_table()
        self.update_output_path()

    def enable_line_edit_file_drop(self, line_edit: QLineEdit):
        line_edit.dragEnterEvent = self.dragEnterEvent
        line_edit.dropEvent = lambda event: line_edit.setText(str(pathlib.Path(event.mimeData().urls()[0].toLocalFile()).absolute()))

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

        # set cache directory path
        if self.config['video2x']['video2x_cache_directory'] is None:
            video2x_cache_directory = str((pathlib.Path(tempfile.gettempdir()) / 'video2x').absolute())
        self.cache_line_edit.setText(video2x_cache_directory)

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
        self.waifu2x_converter_cpp_processor_spin_box.setValue(settings['processor'])
        self.waifu2x_converter_cpp_mode_combo_box.setCurrentText(settings['mode'])
        self.waifu2x_converter_cpp_disable_gpu_check_box.setChecked(settings['disable-gpu'])
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

    def resolve_driver_settings(self):

        # waifu2x-caffe
        self.config['waifu2x_caffe']['path'] = os.path.expandvars(self.waifu2x_caffe_path_line_edit.text())
        self.config['waifu2x_caffe']['mode'] = self.waifu2x_caffe_mode_combo_box.currentText()
        self.config['waifu2x_caffe']['noise_level'] = self.waifu2x_caffe_noise_level_spin_box.value()
        self.config['waifu2x_caffe']['process'] = self.waifu2x_caffe_process_combo_box.currentText()
        self.config['waifu2x_caffe']['model_dir'] = str((pathlib.Path(self.config['waifu2x_caffe']['path']).parent / 'models' / self.waifu2x_caffe_model_combobox.currentText()).absolute())
        self.config['waifu2x_caffe']['crop_size'] = self.waifu2x_caffe_crop_size_spin_box.value()
        self.config['waifu2x_caffe']['output_quality'] = self.waifu2x_caffe_output_depth_spin_box.value()
        self.config['waifu2x_caffe']['output_depth'] = self.waifu2x_caffe_output_depth_spin_box.value()
        self.config['waifu2x_caffe']['batch_size'] = self.waifu2x_caffe_batch_size_spin_box.value()
        self.config['waifu2x_caffe']['gpu'] = self.waifu2x_caffe_gpu_spin_box.value()
        self.config['waifu2x_caffe']['tta'] = int(self.waifu2x_caffe_tta_check_box.isChecked())

        # waifu2x-converter-cpp
        self.config['waifu2x_converter_cpp']['path'] = os.path.expandvars(self.waifu2x_converter_cpp_path_line_edit.text())
        self.config['waifu2x_converter_cpp']['png-compression'] = self.waifu2x_converter_cpp_png_compression_spin_box.value()
        self.config['waifu2x_converter_cpp']['processor'] = self.waifu2x_converter_cpp_processor_spin_box.value()
        self.config['waifu2x_converter_cpp']['model-dir'] = str((pathlib.Path(self.config['waifu2x_converter_cpp']['path']).parent / self.waifu2x_converter_cpp_model_combo_box.currentText()).absolute())
        self.config['waifu2x_converter_cpp']['mode'] = self.waifu2x_converter_cpp_mode_combo_box.currentText()
        self.config['waifu2x_converter_cpp']['disable-gpu'] = bool(self.waifu2x_converter_cpp_disable_gpu_check_box.isChecked())
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

    def update_gui_for_driver(self):
        current_driver = AVAILABLE_DRIVERS[self.driver_combo_box.currentText()]

        # update scale ratio constraints
        if current_driver in ['waifu2x_caffe', 'waifu2x_converter_cpp', 'anime4kcpp']:
            self.scale_ratio_double_spin_box.setMinimum(0.0)
            self.scale_ratio_double_spin_box.setMaximum(999.0)
            self.scale_ratio_double_spin_box.setValue(2.0)
        elif current_driver == 'waifu2x_ncnn_vulkan':
            self.scale_ratio_double_spin_box.setMinimum(1.0)
            self.scale_ratio_double_spin_box.setMaximum(2.0)
            self.scale_ratio_double_spin_box.setValue(2.0)
        elif current_driver == 'srmd_ncnn_vulkan':
            self.scale_ratio_double_spin_box.setMinimum(2.0)
            self.scale_ratio_double_spin_box.setMaximum(4.0)
            self.scale_ratio_double_spin_box.setValue(2.0)

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
        items_to_delete = []
        for index in [i.row() for i in self.input_table_view.selectedIndexes()]:
            items_to_delete.append(self.input_table_data[index])

        for item in items_to_delete:
            self.input_table_data.remove(item)
        self.update_input_table()

    def input_table_clear_all(self):
        self.input_table_data = []
        self.update_input_table()

    def input_table_path_exists(self, input_path):
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

    def update_output_path(self):
        # if there is more than one input
        if len(self.input_table_data) != 1:
            return

        input_path = self.input_table_data[0]
        # give up if input path doesn't exist or isn't a file or a directory
        if not input_path.exists() or not (input_path.is_file() or input_path.is_dir()):
            return

        if input_path.is_file():
            output_path = input_path.parent / f'{input_path.stem}_output.mp4'
        elif input_path.is_dir():
            output_path = input_path.parent / f'{input_path.stem}_output'

        # try up to 1000 times
        output_path_id = 0
        while output_path.exists() and output_path_id <= 1000:
            if input_path.is_file():
                output_path = input_path.parent / pathlib.Path(f'{input_path.stem}_output_{output_path_id}.mp4')
            elif input_path.is_dir():
                output_path = input_path.parent / pathlib.Path(f'{input_path.stem}_output_{output_path_id}')
            output_path_id += 1

        if not output_path.exists():
            self.output_line_edit.setText(str(output_path.absolute()))

    def select_input_file(self):
        if ((input_file := self.select_file('Select Input File')) is None or
                self.input_table_path_exists(input_file)):
            return
        self.input_table_data.append(input_file)
        self.update_output_path()
        self.update_input_table()

    def select_input_folder(self):
        if ((input_folder := self.select_folder('Select Input Folder')) is None or
                self.input_table_path_exists(input_folder)):
            return
        self.input_table_data.append(input_folder)
        self.update_output_path()
        self.update_input_table()

    def select_output_file(self):
        if (output_file := self.select_file('Select Output File')) is None:
            return
        self.output_line_edit.setText(str(output_file.absolute()))

    def select_output_folder(self):
        if (output_folder := self.select_folder('Select Output Folder')) is None:
            return
        self.output_line_edit.setText(str(output_folder.absolute()))

    def select_cache_folder(self):
        if (cache_folder := self.select_folder('Select Cache Folder')) is None:
            return
        self.cache_line_edit.setText(str(cache_folder.absolute()))

    def select_config_file(self):
        if (config_file := self.select_file('Select Config File', filter='(YAML files (*.yaml))')) is None:
            return
        self.config_line_edit.setText(str(config_file.absolute()))
        self.load_configurations()

    def select_driver_binary_path(self, driver_line_edit):
        if (driver_binary_path := self.select_file('Select Driver Binary File')) is None:
            return
        driver_line_edit.setText(str(driver_binary_path.absolute()))

    def show_error(self, message: str):
        QErrorMessage(self).showMessage(message.replace('\n', '<br>'))

    def show_message(self, message: str, custom_icon=None):
        message_box = QMessageBox()
        message_box.setWindowTitle('Message')
        if custom_icon:
            message_box.setIconPixmap(custom_icon.scaled(64, 64))
        else:
            message_box.setIcon(QMessageBox.Information)
        message_box.setText(message)
        message_box.exec_()

    def start_progress_bar(self, progress_callback):

        # initialize progress bar values
        upscale_begin_time = time.time()
        progress_callback.emit((upscale_begin_time, 0, 0, 0, 0, pathlib.Path()))

        # keep querying upscaling process and feed information to callback signal
        while self.upscaler.running:

            progress_callback.emit((upscale_begin_time,
                                    self.upscaler.total_frames_upscaled,
                                    self.upscaler.total_frames,
                                    self.upscaler.total_processed,
                                    self.upscaler.total_videos,
                                    self.upscaler.current_input_video))
            time.sleep(1)

        # upscale process will stop at 99%
        # so it's set to 100 manually when all is done
        progress_callback.emit((upscale_begin_time, 0, 0, 0, 0, pathlib.Path()))

    def set_progress(self, progress_information: tuple):
        upscale_begin_time = progress_information[0]
        total_frames_upscaled = progress_information[1]
        total_frames = progress_information[2]
        total_processed = progress_information[3]
        total_videos = progress_information[4]
        current_input_video = progress_information[5]

        # calculate fields based on frames and time elapsed
        time_elapsed = time.time() - upscale_begin_time
        try:
            rate = total_frames_upscaled / (time.time() - upscale_begin_time)
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
        self.overall_progress_label.setText('Overall Progress: {}/{}'.format(total_processed, total_videos))
        self.overall_progress_bar.setMaximum(total_videos)
        self.overall_progress_bar.setValue(total_processed)
        self.currently_processing_label.setText('Currently Processing: {}'.format(str(current_input_video.name)))

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
                self.show_error('Input path unspecified')
                return
            if self.output_line_edit.text().strip() == '':
                self.show_error('Output path unspecified')
                return

            if len(self.input_table_data) == 1:
                input_directory = self.input_table_data[0]
            else:
                input_directory = self.input_table_data
            output_directory = pathlib.Path(os.path.expandvars(self.output_line_edit.text()))

            # load driver settings from GUI
            self.resolve_driver_settings()

            # load driver settings for the current driver
            self.driver_settings = self.config[AVAILABLE_DRIVERS[self.driver_combo_box.currentText()]]

            self.upscaler = Upscaler(input_path=input_directory,
                                     output_path=output_directory,
                                     driver_settings=self.driver_settings,
                                     ffmpeg_settings=self.ffmpeg_settings)

            # set optional options
            self.upscaler.driver = AVAILABLE_DRIVERS[self.driver_combo_box.currentText()]
            self.upscaler.scale_ratio = self.scale_ratio_double_spin_box.value()
            self.upscaler.processes = self.processes_spin_box.value()
            self.upscaler.video2x_cache_directory = pathlib.Path(os.path.expandvars(self.cache_line_edit.text()))
            self.upscaler.image_format = self.config['video2x']['image_format'].lower()
            self.upscaler.preserve_frames = bool(self.preserve_frames_check_box.isChecked())

            # start progress bar
            if AVAILABLE_DRIVERS[self.driver_combo_box.currentText()] != 'anime4kcpp':
                progress_bar_worker = ProgressBarWorker(self.start_progress_bar)
                progress_bar_worker.signals.progress.connect(self.set_progress)
                self.threadpool.start(progress_bar_worker)

            # run upscaler
            worker = UpscalerWorker(self.upscaler.run)
            worker.signals.error.connect(self.upscale_errored)
            worker.signals.interrupted.connect(self.upscale_interrupted)
            worker.signals.finished.connect(self.upscale_successful)
            self.threadpool.start(worker)
            self.start_button.setEnabled(False)
            self.stop_button.setEnabled(True)

        except Exception:
            self.upscale_errored(traceback.format_exc())

    def upscale_errored(self, error_message):
        self.show_error(f'Upscaler ran into an error:\n{error_message}')
        self.threadpool.waitForDone(5)
        self.start_button.setEnabled(True)
        self.stop_button.setEnabled(False)
        self.reset_progress_display()

    def upscale_interrupted(self):
        self.show_message('Upscale has been interrupted')
        self.threadpool.waitForDone(5)
        self.start_button.setEnabled(True)
        self.stop_button.setEnabled(False)
        self.reset_progress_display()

    def upscale_successful(self):
        # if all threads have finished
        self.threadpool.waitForDone(5)
        self.show_message('Upscale finished successfully, taking {} seconds'.format(round((time.time() - self.begin_time), 5)))
        self.start_button.setEnabled(True)
        self.stop_button.setEnabled(False)
        self.reset_progress_display()

    def stop(self):
        with contextlib.suppress(AttributeError):
            self.upscaler.running = False

    def closeEvent(self, event):
        # try cleaning up temp directories
        self.stop()
        event.accept()


# this file shouldn't be imported
if __name__ == '__main__':
    app = QApplication(sys.argv)
    window = Video2XMainWindow()
    window.show()
    app.exec_()
