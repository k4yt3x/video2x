#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Creator: Video2X QT
Author: K4YT3X
Date Created: May 5, 2020
Last Modified: May 6, 2020
"""

# local imports
from upscaler import Upscaler

# built-in imports
import pathlib
import sys

# built-in imports
import contextlib
import re
import shutil
import tempfile
import threading
import time
import traceback
import yaml

# third-party imports
from PyQt5 import QtWidgets, QtGui
from PyQt5 import uic
from PyQt5.QtCore import QObject, pyqtSlot, pyqtSignal, QRunnable, QThreadPool

VERSION = '2.0.0'

LEGAL_INFO = f'''Video2X GUI Version: {VERSION}
Author: K4YT3X
License: GNU GPL v3
Github Page: https://github.com/k4yt3x/video2x
Contact: k4yt3x@k4yt3x.com'''

AVAILABLE_DRIVERS = {
    'Waifu2X Caffe': 'waifu2x_caffe',
    'Waifu2X Converter CPP': 'waifu2x_converter_cpp',
    'Waifu2x NCNN Vulkan': 'waifu2x_ncnn_vulkan',
    'SRMD NCNN Vulkan': 'srmd_ncnn_vulkan',
    'Anime4KCPP': 'anime4kcpp'
}


class UpscalerSignals(QObject):
    finished = pyqtSignal()
    error = pyqtSignal(tuple)
    result = pyqtSignal(object)

class Worker(QRunnable):

    def __init__(self, fn, *args, **kwargs):
        super(Worker, self).__init__()

        # Store constructor arguments (re-used for processing)
        self.fn = fn
        self.args = args
        self.kwargs = kwargs
        self.signals = UpscalerSignals()

    @pyqtSlot()
    def run(self):

        # Retrieve args/kwargs here; and fire processing using them
        try:
            result = self.fn(*self.args, **self.kwargs)
        except Exception:
            traceback.print_exc()
            exctype, value = sys.exc_info()[:2]
            self.signals.error.emit((exctype, value, traceback.format_exc()))
        else:
            self.signals.result.emit(result)  # Return the result of the processing
        finally:
            self.signals.finished.emit()  # Done

class Video2XMainWindow(QtWidgets.QMainWindow):

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        uic.loadUi('video2x_gui.ui', self)

        self.video2x_icon_path = str((pathlib.Path(__file__).parent / 'images' / 'video2x.png').absolute())
        self.setWindowTitle(f'Video2X GUI {VERSION}')
        self.setWindowIcon(QtGui.QIcon(self.video2x_icon_path))

        # menu bar
        self.action_exit = self.findChild(QtWidgets.QAction, 'actionExit')
        self.action_exit.triggered.connect(sys.exit)

        self.action_about = self.findChild(QtWidgets.QAction, 'actionAbout')
        self.action_about.triggered.connect(lambda: self.show_message(LEGAL_INFO, custom_icon=QtGui.QPixmap(self.video2x_icon_path)))

        # main tab
        # select input file/folder
        self.input_line_edit = self.findChild(QtWidgets.QLineEdit, 'inputLineEdit')
        self.input_select_file_button = self.findChild(QtWidgets.QPushButton, 'inputSelectFileButton')
        self.input_select_file_button.clicked.connect(self.select_input_file)
        self.input_select_folder_button = self.findChild(QtWidgets.QPushButton, 'inputSelectFolderButton')
        self.input_select_folder_button.clicked.connect(self.select_input_folder)

        # select output file/folder
        self.output_line_edit = self.findChild(QtWidgets.QLineEdit, 'outputLineEdit')
        self.output_select_file_button = self.findChild(QtWidgets.QPushButton, 'outputSelectFileButton')
        self.output_select_file_button.clicked.connect(self.select_output_file)
        self.output_select_folder_button = self.findChild(QtWidgets.QPushButton, 'outputSelectFolderButton')
        self.output_select_folder_button.clicked.connect(self.select_output_folder)

        # config file
        self.config_line_edit = self.findChild(QtWidgets.QLineEdit, 'configLineEdit')
        self.config_line_edit.setText(str((pathlib.Path(__file__).parent / 'video2x.yaml').absolute()))
        self.config_select_file_button = self.findChild(QtWidgets.QPushButton, 'configSelectButton')
        self.config_select_file_button.clicked.connect(self.select_config_file)

        # cache directory
        self.cache_line_edit = self.findChild(QtWidgets.QLineEdit, 'cacheLineEdit')
        self.cache_select_folder_button = self.findChild(QtWidgets.QPushButton, 'cacheSelectFolderButton')
        self.cache_select_folder_button.clicked.connect(self.select_cache_folder)

        # express settings
        self.driver_combo_box = self.findChild(QtWidgets.QComboBox, 'driverComboBox')
        self.processes_spin_box = self.findChild(QtWidgets.QSpinBox, 'processesSpinBox')
        self.scale_ratio_double_spin_box = self.findChild(QtWidgets.QDoubleSpinBox, 'scaleRatioDoubleSpinBox')
        self.preserve_frames_check_box = self.findChild(QtWidgets.QCheckBox, 'preserveFramesCheckBox')

        # progress bar and start/stop controls
        self.progress_bar = self.findChild(QtWidgets.QProgressBar, 'progressBar')
        self.start_button = self.findChild(QtWidgets.QPushButton, 'startButton')
        self.start_button.clicked.connect(self.upscale)
        self.stop_button = self.findChild(QtWidgets.QPushButton, 'stopButton')
        self.stop_button.clicked.connect(self.stop)

        # driver settings
        # waifu2x-caffe
        self.waifu2x_caffe_path_line_edit = self.findChild(QtWidgets.QLineEdit, 'waifu2xCaffePathLineEdit')
        self.waifu2x_caffe_path_select_button = self.findChild(QtWidgets.QPushButton, 'waifu2xCaffePathSelectButton')
        self.waifu2x_caffe_mode_combo_box = self.findChild(QtWidgets.QComboBox, 'waifu2xCaffeModeComboBox')
        self.waifu2x_caffe_noise_level_spin_box = self.findChild(QtWidgets.QSpinBox, 'waifu2xCaffeNoiseLevelSpinBox')
        self.waifu2x_caffe_process_combo_box = self.findChild(QtWidgets.QComboBox, 'waifu2xCaffeProcessComboBox')
        self.waifu2x_caffe_model_combobox = self.findChild(QtWidgets.QComboBox, 'waifu2xCaffeModelComboBox')
        self.waifu2x_caffe_crop_size_spin_box = self.findChild(QtWidgets.QSpinBox, 'waifu2xCaffeCropSizeSpinBox')
        self.waifu2x_caffe_output_quality_spin_box = self.findChild(QtWidgets.QSpinBox, 'waifu2xCaffeOutputQualitySpinBox')
        self.waifu2x_caffe_output_depth_spin_box = self.findChild(QtWidgets.QSpinBox, 'waifu2xCaffeOutputDepthSpinBox')
        self.waifu2x_caffe_batch_size_spin_box = self.findChild(QtWidgets.QSpinBox, 'waifu2xCaffeBatchSizeSpinBox')
        self.waifu2x_caffe_gpu_spin_box = self.findChild(QtWidgets.QSpinBox, 'waifu2xCaffeGpuSpinBox')
        self.waifu2x_caffe_tta_check_box = self.findChild(QtWidgets.QCheckBox, 'waifu2xCaffeTtaCheckBox')

        # waifu2x-converter-cpp
        self.waifu2x_converter_cpp_path_line_edit = self.findChild(QtWidgets.QLineEdit, 'waifu2xConverterCppPathLineEdit')
        self.waifu2x_converter_cpp_png_compression_spin_box = self.findChild(QtWidgets.QSpinBox, 'waifu2xConverterCppPngCompressionSpinBox')
        self.waifu2x_converter_cpp_processor_spin_box = self.findChild(QtWidgets.QSpinBox, 'waifu2xConverterCppProcessorSpinBox')
        self.waifu2x_converter_cpp_model_combo_box = self.findChild(QtWidgets.QComboBox, 'waifu2xConverterCppModelComboBox')
        self.waifu2x_converter_cpp_mode_combo_box = self.findChild(QtWidgets.QComboBox, 'waifu2xConverterCppModeComboBox')
        self.waifu2x_converter_cpp_disable_gpu_check_box = self.findChild(QtWidgets.QCheckBox, 'disableGpuCheckBox')
        self.waifu2x_converter_cpp_tta_check_box = self.findChild(QtWidgets.QCheckBox, 'ttaCheckBox')

        # load configurations
        self.load_configurations()

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
        config_file_path = pathlib.Path(self.config_line_edit.text())

        # if file doesn't exist, return
        if not config_file_path.is_file():
            QtWidgets.QErrorMessage(self).showMessage('Video2X configuration file not found, please specify manually.')
            return

        # read configuration dict from config file
        self.config = self.read_config(config_file_path)

        # load FFmpeg settings
        self.ffmpeg_settings = self.config['ffmpeg']

        # load cache directory, create it if necessary
        if self.config['video2x']['video2x_cache_directory'] is not None:
            video2x_cache_directory = pathlib.Path(self.config['video2x']['video2x_cache_directory'])
        else:
            video2x_cache_directory = pathlib.Path(tempfile.gettempdir()) / 'video2x'

        if video2x_cache_directory.exists() and not video2x_cache_directory.is_dir():
            self.show_error('Specified cache directory is a file/link')
            raise FileExistsError('Specified cache directory is a file/link')

        # if cache directory doesn't exist
        # ask the user if it should be created
        elif not video2x_cache_directory.exists():
            try:
                video2x_cache_directory.mkdir(parents=True, exist_ok=True)
            except Exception as exception:
                self.show_error(f'Unable to create cache directory: {video2x_cache_directory}')
                raise exception
        self.cache_line_edit.setText(str(video2x_cache_directory.absolute()))

        # load preserve frames settings
        self.preserve_frames_check_box.setChecked(self.config['video2x']['preserve_frames'])
        self.start_button.setEnabled(True)

        # waifu2x-caffe
        settings = self.config['waifu2x_caffe']
        self.waifu2x_caffe_path_line_edit.setText(str(pathlib.Path(settings['path']).absolute()))
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
        self.waifu2x_converter_cpp_path_line_edit.setText(str(pathlib.Path(settings['path']).absolute()))
        self.waifu2x_converter_cpp_png_compression_spin_box.setValue(settings['png-compression'])
        self.waifu2x_converter_cpp_processor_spin_box.setValue(settings['processor'])
        self.waifu2x_converter_cpp_mode_combo_box.setCurrentText(settings['mode'])
        self.waifu2x_converter_cpp_disable_gpu_check_box.setChecked(settings['disable-gpu'])
        self.waifu2x_converter_cpp_tta_check_box.setChecked(bool(settings['tta']))

    def resolve_driver_settings(self):
        
        # waifu2x-caffe
        self.config['waifu2x_caffe']['path'] = self.waifu2x_caffe_path_line_edit.text()
        self.config['waifu2x_caffe']['mode'] = self.waifu2x_caffe_mode_combo_box.currentText()
        self.config['waifu2x_caffe']['noise_level'] = self.waifu2x_caffe_noise_level_spin_box.value()
        self.config['waifu2x_caffe']['process'] = self.waifu2x_caffe_process_combo_box.currentText()
        self.config['waifu2x_caffe']['model_dir'] = str((pathlib.Path(self.config['waifu2x_caffe']['path']).parent / 'models' / self.waifu2x_caffe_model_combobox.currentText()).absolute())
        self.config['waifu2x_caffe']['crop_size'] = self.waifu2x_caffe_crop_size_spin_box.value()
        self.config['waifu2x_caffe']['output_quality'] = self.waifu2x_caffe_output_depth_spin_box.value()
        self.config['waifu2x_caffe']['output_depth'] = self.waifu2x_caffe_output_depth_spin_box.value()
        self.config['waifu2x_caffe']['batch_size'] = self.waifu2x_caffe_batch_size_spin_box.value()
        self.config['waifu2x_caffe']['gpu'] = self.waifu2x_caffe_gpu_spin_box.value()
        self.config['waifu2x_caffe']['tta'] = int(self.waifu2x_caffe_tta_check_box.checkState())

        # waifu2x-converter-cpp
        self.config['waifu2x_converter_cpp']['path'] = self.waifu2x_converter_cpp_path_line_edit.text()
        self.config['waifu2x_converter_cpp']['png-compression'] = self.waifu2x_converter_cpp_png_compression_spin_box.value()
        self.config['waifu2x_converter_cpp']['processor'] = self.waifu2x_converter_cpp_processor_spin_box.value()
        self.config['waifu2x_converter_cpp']['model-dir'] = str((pathlib.Path(self.config['waifu2x_converter_cpp']['path']).parent / self.waifu2x_converter_cpp_model_combo_box.currentText()).absolute())
        self.config['waifu2x_converter_cpp']['mode'] = self.waifu2x_converter_cpp_mode_combo_box.currentText()
        self.config['waifu2x_converter_cpp']['disable-gpu'] = bool(self.waifu2x_converter_cpp_disable_gpu_check_box.checkState())
        self.config['waifu2x_converter_cpp']['tta'] = int(self.waifu2x_converter_cpp_tta_check_box.checkState())

    def select_input_file(self):
        input_file = QtWidgets.QFileDialog.getOpenFileName(self, 'Select Input File', )
        if not isinstance(input_file, tuple) or input_file[0] == '':
            return

        input_file = pathlib.Path(input_file[0])

        self.input_line_edit.setText(str(input_file.absolute()))

        # try to set an output file name automatically
        output_file = input_file.parent / f'{input_file.stem}_output.mp4'

        output_file_id = 0
        while output_file.is_file() and output_file_id <= 10:
            output_file = input_file.parent / pathlib.Path(f'{input_file.stem}_output_{output_file_id}.mp4')
            output_file_id += 1

        if not output_file.exists():
            self.output_line_edit.setText(str(output_file.absolute()))

    def select_input_folder(self):
        input_folder = QtWidgets.QFileDialog.getExistingDirectory(self, 'Select Input Folder')
        if input_folder == '':
            return

        input_folder = pathlib.Path(input_folder)

        self.input_line_edit.setText(str(input_folder.absolute()))

        # try to set an output file name automatically
        output_folder = input_folder.parent / f'{input_folder.stem}_output'

        output_file_id = 0
        while output_folder.is_dir() and output_file_id <= 10:
            output_folder = input_folder.parent / pathlib.Path(f'{input_folder.stem}_output_{output_file_id}')
            output_file_id += 1

        if not output_folder.exists():
            self.output_line_edit.setText(str(output_folder.absolute()))

    def select_output_file(self):
        output_file = QtWidgets.QFileDialog.getOpenFileName(self, 'Select Output File')
        if not isinstance(output_file, tuple):
            return

        self.output_line_edit.setText(str(pathlib.Path(output_file[0]).absolute()))

    def select_output_folder(self):
        output_folder = QtWidgets.QFileDialog.getExistingDirectory(self, 'Select Output Folder')
        if output_folder == '':
            return

        self.output_line_edit.setText(str(pathlib.Path(output_folder).absolute()))

    def select_cache_folder(self):
        cache_folder = QtWidgets.QFileDialog.getExistingDirectory(self, 'Select Cache Folder')
        if cache_folder == '':
            return

        self.cache_line_edit.setText(str(pathlib.Path(cache_folder).absolute()))

    def select_config_file(self):
        config_file = QtWidgets.QFileDialog.getOpenFileName(self, 'Select Config File', filter='(YAML files (*.yaml))')
        if not isinstance(config_file, tuple):
            return

        self.config_line_edit.setText(str(pathlib.Path(config_file[0]).absolute()))
        self.load_configurations()

    def show_error(self, message: str):
        QtWidgets.QErrorMessage(self).showMessage(message.replace('\n', '<br>'))

    def show_message(self, message: str, custom_icon=None):
        message_box = QtWidgets.QMessageBox()
        message_box.setWindowTitle('Message')
        if custom_icon:
            message_box.setIconPixmap(custom_icon.scaled(64, 64))
        else:
            message_box.setIcon(QtWidgets.QMessageBox.Information)
        message_box.setText(message)
        message_box.exec_()

    def start_progress_bar(self):

        # initialize variables early
        self.upscaler.progress_bar_exit_signal = False
        self.upscaler.total_frames_upscaled = 0
        self.upscaler.total_frames = 1

        # initialize progress bar values
        self.progress_bar.setValue(0)

        while not self.upscaler.progress_bar_exit_signal:
            self.progress_bar.setValue(int(100 * self.upscaler.total_frames_upscaled / self.upscaler.total_frames))
            time.sleep(1)
        self.progress_bar.setValue(100)

    def upscale(self):

        # start execution
        try:
            # start timer
            self.begin_time = time.time()

            # resolve input and output directories from GUI
            input_directory = pathlib.Path(self.input_line_edit.text())
            output_directory = pathlib.Path(self.output_line_edit.text())

            # create thread pool for upscaler workers
            self.threadpool = QThreadPool()
            self.workers = []

            # load driver settings from GUI
            self.resolve_driver_settings()

            # load driver settings for the current driver
            self.driver_settings = self.config[AVAILABLE_DRIVERS[self.driver_combo_box.currentText()]]

            # if input specified is a single file
            if input_directory.is_file():

                # upscale single video file

                # check for input output format mismatch
                if output_directory.is_dir():
                    self.show_error('Input and output path type mismatch\n\
                                     Input is single file but output is directory')
                    raise Exception('input output path type mismatch')
                if not re.search(r'.*\..*$', str(output_directory)):
                    self.show_error('No suffix found in output file path\n\
                                     Suffix must be specified for FFmpeg')
                    raise Exception('No suffix specified')

                self.upscaler = Upscaler(input_video=input_directory,
                                         output_video=output_directory,
                                         driver_settings=self.driver_settings,
                                         ffmpeg_settings=self.ffmpeg_settings)

                # set optional options
                self.upscaler.driver = AVAILABLE_DRIVERS[self.driver_combo_box.currentText()]
                self.upscaler.scale_ratio = self.scale_ratio_double_spin_box.value()
                self.upscaler.processes = self.processes_spin_box.value()
                self.upscaler.video2x_cache_directory = pathlib.Path(self.cache_line_edit.text())
                self.upscaler.image_format = self.config['video2x']['image_format'].lower()
                self.upscaler.preserve_frames = bool(self.preserve_frames_check_box.checkState())

                # start progress bar
                if AVAILABLE_DRIVERS[self.driver_combo_box.currentText()] != 'anime4kcpp':
                    progress_bar_worker = Worker(self.start_progress_bar)
                    self.threadpool.start(progress_bar_worker)

                # run upscaler
                worker = Worker(self.upscaler.run)
                worker.signals.finished.connect(self.upscale_completed)
                self.workers.append(worker)
                self.threadpool.start(worker)
                self.start_button.setEnabled(False)
                # self.stop_button.setEnabled(True)

            # if input specified is a directory
            elif input_directory.is_dir():
                # upscale videos in a directory

                # make output directory if it doesn't exist
                output_directory.mkdir(parents=True, exist_ok=True)

                for input_video in [f for f in input_directory.iterdir() if f.is_file()]:
                    output_video = output_directory / input_video.name
                    self.upscaler = Upscaler(input_video=input_video,
                                             output_video=output_video,
                                             driver_settings=self.driver_settings,
                                             ffmpeg_settings=self.ffmpeg_settings)

                    # set optional options
                    self.upscaler.driver = AVAILABLE_DRIVERS[self.driver_combo_box.currentText()]
                    self.upscaler.scale_ratio = self.scale_ratio_double_spin_box.value()
                    self.upscaler.processes = self.processes_spin_box.value()
                    self.upscaler.video2x_cache_directory = pathlib.Path(self.cache_line_edit.text())
                    self.upscaler.image_format = self.config['video2x']['image_format'].lower()
                    self.upscaler.preserve_frames = bool(self.preserve_frames_check_box.checkState())

                    # start progress bar
                    if AVAILABLE_DRIVERS[self.driver_combo_box.currentText()] != 'anime4kcpp':
                        progress_bar_worker = Worker(self.start_progress_bar)
                        self.threadpool.start(progress_bar_worker)

                    # run upscaler
                    worker = Worker(self.upscaler.run)
                    worker.signals.finished.connect(self.upscale_completed)
                    self.threadpool.start(worker)
                    self.start_button.setEnabled(False)
            else:
                self.show_error('Input path is neither a file nor a directory')
                raise FileNotFoundError(f'{input_directory} is neither file nor directory')

        except Exception:
            error_message = traceback.format_exc()
            self.show_error(f'Upscaler ran into an error:\n{error_message}')
            print(error_message, file=sys.stderr)

            # try cleaning up temp directories
            with contextlib.suppress(Exception):
                self.upscaler.progress_bar_exit_signal = True
                self.upscaler.cleanup_temp_directories()

    def upscale_completed(self):
        # if all threads have finished
        if self.threadpool.activeThreadCount() == 0:
            self.show_message('Program completed, taking {} seconds'.format(round((time.time() - self.begin_time), 5)))
            # remove Video2X cache directory
            with contextlib.suppress(FileNotFoundError):
                if not bool(self.preserve_frames_check_box.checkState()):
                    shutil.rmtree(pathlib.Path(self.cache_line_edit.text()))
            self.start_button.setEnabled(True)

    def stop(self):
        # TODO unimplemented yet
        pass


app = QtWidgets.QApplication(sys.argv)
window = Video2XMainWindow()
window.show()
app.exec_()
