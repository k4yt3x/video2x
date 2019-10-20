#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Video2x GUI
Author: K4YT3X
Date Created: July 27, 2019
Last Modified: August 17, 2019

Description: GUI for Video2X
"""

# local imports
from exceptions import *
from upscaler import Upscaler

# built-in imports
from tkinter import *
from tkinter import messagebox
from tkinter import ttk
from tkinter.filedialog import *
import json
import pathlib
import tempfile
import threading
import time

VERSION = '1.1.1'

LEGAL_INFO = f'''Video2X GUI Version: {VERSION}
Author: K4YT3X
License: GNU GPL v3
Github Page: https://github.com/k4yt3x/video2x
Contact: k4yt3x@k4yt3x.com'''

# global static variables
AVAILABLE_METHODS = {
    'GPU': 'gpu',
    'CUDNN': 'cudnn',
    'CPU': 'cpu'
}

AVAILABLE_DRIVERS = {
    'Waifu2X Caffe': 'waifu2x_caffe',
    'Waifu2X Converter CPP': 'waifu2x_converter',
    'Waifu2x NCNN Vulkan': 'waifu2x_ncnn_vulkan',
    'Anime4K': 'anime4k'
}

IMAGE_FORMATS = {'PNG', 'JPG'}


class Video2xGui():

    def __init__(self):

        self.running = False

        # create main window
        self.main_window = Tk()
        self.main_window.title(f'Video2X GUI {VERSION}')
        self.main_frame = Frame()
        self.main_frame.pack(fill=BOTH, expand=True)

        # add menu bar
        self.menu_bar = Menu(self.main_frame)

        # file menu
        self.file_menu = Menu(self.menu_bar, tearoff=0)
        self.file_menu.add_command(label='Exit', command=self.main_frame.quit)
        self.menu_bar.add_cascade(label='File', menu=self.file_menu)

        # help menu
        self.help_menu = Menu(self.menu_bar, tearoff=0)
        self.help_menu.add_command(label='About', command=self._display_help)
        self.menu_bar.add_cascade(label='Help', menu=self.help_menu)

        self.main_window.config(menu=self.menu_bar)

        # file frame
        self.file_frame = Frame(self.main_frame)
        self.file_frame.pack(fill=X, padx=5, pady=5, expand=True)

        # input file
        self.input_file = StringVar()
        label_text = StringVar()
        label_text.set('Input File')
        Label(self.file_frame, textvariable=label_text, relief=RIDGE, width=10).grid(row=0, column=0, padx=5, pady=5, sticky=W)
        Entry(self.file_frame, textvariable=self.input_file, width=60).grid(row=0, column=1, padx=5, pady=5, sticky=W)
        Button(self.file_frame, text='Select', command=self._select_input).grid(row=0, column=2, padx=5, pady=5, sticky=W)

        # output file
        self.output_file = StringVar()
        label_text = StringVar()
        label_text.set('Output File')
        Label(self.file_frame, textvariable=label_text, relief=RIDGE, width=10).grid(row=1, column=0, padx=5, pady=5, sticky=W)
        Entry(self.file_frame, textvariable=self.output_file, width=60).grid(row=1, column=1, padx=5, pady=5, sticky=W)
        Button(self.file_frame, text='Select', command=self._select_output).grid(row=1, column=2, padx=5, pady=5, sticky=W)

        # options
        self.options_frame = Frame()
        # self.options_left.pack(fill=X, padx=5, pady=5, expand=True)
        self.options_frame.pack(fill=X, padx=5, pady=5, expand=True)

        self.options_left = Frame(self.options_frame)
        # self.options_left.pack(fill=X, padx=5, pady=5, expand=True)
        self.options_left.grid(row=0, column=0, padx=5, pady=5, sticky=N)

        # width
        self.width = IntVar()
        # self.width.set(1920)
        Label(self.options_left, text='Width', relief=RIDGE, width=15).grid(row=0, column=0, padx=2, pady=3)
        width_field = Entry(self.options_left, textvariable=self.width)
        width_field.grid(row=0, column=1, padx=2, pady=3, sticky=W)

        # height
        self.height = IntVar()
        # self.height.set(1080)
        Label(self.options_left, text='Height', relief=RIDGE, width=15).grid(row=1, column=0, padx=2, pady=3)
        height_field = Entry(self.options_left, textvariable=self.height)
        height_field.grid(row=1, column=1, padx=2, pady=3, sticky=W)

        # scale ratio
        self.scale_ratio = DoubleVar()
        # self.scale_ratio.set(2.0)
        Label(self.options_left, text='Scale Ratio', relief=RIDGE, width=15).grid(row=2, column=0, padx=2, pady=3)
        scale_ratio_field = Entry(self.options_left, textvariable=self.scale_ratio)
        scale_ratio_field.grid(row=2, column=1, padx=2, pady=3, sticky=W)

        # image format
        self.image_format = StringVar(self.options_left)
        self.image_format.set('PNG')
        Label(self.options_left, text='Image Format', relief=RIDGE, width=15).grid(row=3, column=0, padx=2, pady=3)
        image_format_menu = OptionMenu(self.options_left, self.image_format, *IMAGE_FORMATS)
        image_format_menu.grid(row=3, column=1, padx=2, pady=3, sticky=W)

        # options
        self.options_right = Frame(self.options_frame)
        # self.options_left.pack(fill=X, padx=5, pady=5, expand=True)
        self.options_right.grid(row=0, column=1, padx=5, pady=5, sticky=N)

        # threads
        self.threads = IntVar()
        self.threads.set(1)
        Label(self.options_right, text='Threads', relief=RIDGE, width=15).grid(row=0, column=0, padx=2, pady=3)
        threads_field = Entry(self.options_right, textvariable=self.threads)
        threads_field.grid(row=0, column=1, padx=2, pady=3, sticky=W)

        # method
        self.method = StringVar(self.options_left)
        self.method.set('GPU')
        Label(self.options_right, text='Method', relief=RIDGE, width=15).grid(row=1, column=0, padx=2, pady=3)
        method_menu = OptionMenu(self.options_right, self.method, *AVAILABLE_METHODS)
        method_menu.grid(row=1, column=1, padx=2, pady=3, sticky=W)

        # driver
        self.driver = StringVar(self.options_left)
        self.driver.set('Waifu2X Caffe')
        Label(self.options_right, text='Driver', relief=RIDGE, width=15).grid(row=2, column=0, padx=2, pady=3)
        driver_menu = OptionMenu(self.options_right, self.driver, *AVAILABLE_DRIVERS)
        driver_menu.grid(row=2, column=1, padx=2, pady=3, sticky=W)

        # preserve frames
        self.preserve_frames = BooleanVar(self.options_left)
        self.preserve_frames.set(True)
        Label(self.options_right, text='Preserve Frames', relief=RIDGE, width=15).grid(row=3, column=0, padx=2, pady=3)
        preserve_frames_menu = OptionMenu(self.options_right, self.preserve_frames, *{True, False})
        preserve_frames_menu.grid(row=3, column=1, padx=2, pady=3, sticky=W)

        # progress bar
        self.progress_bar_frame = Frame()
        self.progress_bar_frame.pack(fill=X, padx=5, pady=5, expand=True)

        self.progress_bar = ttk.Progressbar(self.progress_bar_frame, orient='horizontal', length=100, mode='determinate')
        self.progress_bar.pack(fill=X)

        # start button frame
        self.start_frame = Frame()
        self.start_frame.pack(fill=X, padx=5, pady=5, expand=True)

        # start button
        self.start_button_text = StringVar()
        self.start_button_text.set('Start')
        Button(self.start_frame, textvariable=self.start_button_text, command=self._launch_upscaling, width=20).pack(side=RIGHT)

        self.main_frame.mainloop()
    
    def _display_help(self):
        messagebox.showinfo('About', LEGAL_INFO)
    
    def _launch_upscaling(self):

        # prevent launching multiple instances
        if self.running:
            messagebox.showerror('Error', 'Video2X is already running')
            return

        # arguments sanity check
        if self.input_file.get() == '':
            messagebox.showerror('Error', 'You must specify input video file/directory path')
            return
        if self.output_file.get() == '':
            messagebox.showerror('Error', 'You must specify output video file/directory path')
            return
        if (self.driver.get() in ['Waifu2X Converter CPP', 'Waifu2x NCNN Vulkan', 'Anime4K']) and self.width.get() and self.height.get():
            messagebox.showerror('Error', f'Selected driver \"{self.driver.get()}\" accepts only scaling ratio')
            return
        if self.driver.get() == 'waifu2x_ncnn_vulkan' and (self.scale_ratio.get() > 2 or not self.scale_ratio.get().is_integer()):
            messagebox.showerror('Error', 'Scaling ratio must be 1 or 2 for waifu2x_ncnn_vulkan')
            return
        if (self.width.get() or self.height.get()) and self.scale_ratio.get():
            messagebox.showerror('Error', 'You can only specify either scaling ratio or output width and height')
            return
        if (self.width.get() and not self.height.get()) or (not self.width.get() and self.height.get()):
            messagebox.showerror('Error', 'You must specify both width and height')
            return
        if (not self.width.get() or not self.height.get()) and not self.scale_ratio.get():
            messagebox.showerror('Error', 'You must specify either output dimensions or scaling ratio')
            return

        upscale = threading.Thread(target=self._upscale)
        upscale.start()
        self.running = True
        self.start_button_text.set('Running')

    def _upscale(self):

        # start timer
        begin_time = time.time()

        # read configuration file
        config = read_config('video2x.json')
        config = absolutify_paths(config)

        input_file = pathlib.Path(self.input_file.get())
        output_file = pathlib.Path(self.output_file.get())
        driver = AVAILABLE_DRIVERS[self.driver.get()]

        if driver == 'waifu2x_caffe':
            waifu2x_settings = config['waifu2x_caffe']
            if not pathlib.Path(waifu2x_settings['waifu2x_caffe_path']).is_file():
                messagebox.showerror('Error', 'Specified waifu2x-caffe directory doesn\'t exist\nPlease check the configuration file settings')
                raise FileNotFoundError(waifu2x_settings['waifu2x_caffe_path'])
        elif driver == 'waifu2x_converter':
            waifu2x_settings = config['waifu2x_converter']
            if not pathlib.Path(waifu2x_settings['waifu2x_converter_path']).is_dir():
                messagebox.showerror('Error', 'Specified waifu2x-converter-cpp directory doesn\'t exist\nPlease check the configuration file settings')
                raise FileNotFoundError(waifu2x_settings['waifu2x_converter_path'])
        elif driver == 'waifu2x_ncnn_vulkan':
            waifu2x_settings = config['waifu2x_ncnn_vulkan']
            if not pathlib.Path(waifu2x_settings['waifu2x_ncnn_vulkan_path']).is_file():
                messagebox.showerror('Error', 'Specified waifu2x_ncnn_vulkan directory doesn\'t exist\nPlease check the configuration file settings')
                raise FileNotFoundError(waifu2x_settings['waifu2x_ncnn_vulkan_path'])
        elif driver == 'anime4k':
            waifu2x_settings = config['anime4k']
            if not pathlib.Path(waifu2x_settings['anime4k_path']).is_file():
                messagebox.showerror('Error', 'Specified Anime4K directory doesn\'t exist\nPlease check the configuration file settings')
                raise FileNotFoundError(waifu2x_settings['anime4k_path'])

        # read FFmpeg configuration
        ffmpeg_settings = config['ffmpeg']

        # load video2x settings
        image_format = config['video2x']['image_format'].lower()
        preserve_frames = config['video2x']['preserve_frames']

        # load cache directory
        if isinstance(config['video2x']['video2x_cache_directory'], str):
            video2x_cache_directory = pathlib.Path(config['video2x']['video2x_cache_directory'])
        else:
            video2x_cache_directory = pathlib.Path(tempfile.gettempdir()) / 'video2x'

        if video2x_cache_directory.exists() and not video2x_cache_directory.is_dir():
            messagebox.showerror('Error', 'Specified cache directory is a file/link')
            raise FileExistsError('Specified cache directory is a file/link')

        elif not video2x_cache_directory.exists():
            # try creating the cache directory
            if messagebox.askyesno('Question', f'Specified cache directory {video2x_cache_directory} does not exist\nCreate directory?'):
                try:
                    video2x_cache_directory.mkdir(parents=True, exist_ok=True)

                # there can be a number of exceptions here
                # PermissionError, FileExistsError, etc.
                # therefore, we put a catch-them-all here
                except Exception as e:
                    messagebox.showerror('Error', f'Unable to create {video2x_cache_directory}\nAborting...')
                    raise e
            else:
                raise FileNotFoundError('Could not create cache directory')

        # load more settings from gui
        width = self.width.get()
        height = self.height.get()
        scale_ratio = self.scale_ratio.get()
        image_format = self.image_format.get()
        threads = self.threads.get()
        method = AVAILABLE_METHODS[self.method.get()]
        preserve_frames = self.preserve_frames.get()

        self.upscaler = Upscaler(input_video=input_file, output_video=output_file, method=method, waifu2x_settings=waifu2x_settings, ffmpeg_settings=ffmpeg_settings)

        # set optional options
        self.upscaler.waifu2x_driver = driver
        self.upscaler.scale_width = width
        self.upscaler.scale_height = height
        self.upscaler.scale_ratio = scale_ratio
        self.upscaler.model_dir = None
        self.upscaler.threads = threads
        self.upscaler.video2x_cache_directory = video2x_cache_directory
        self.upscaler.image_format = image_format
        self.upscaler.preserve_frames = preserve_frames

        # run upscaler
        self.upscaler.create_temp_directories()

        # start progress bar
        progress_bar = threading.Thread(target=self._progress_bar)
        progress_bar.start()

        # start upscaling
        self.upscaler.run()
        self.upscaler.cleanup_temp_directories()

        # show message when upscaling completes
        messagebox.showinfo('Info', f'Upscaling Completed\nTime Taken: {round((time.time() - begin_time), 5)} seconds')
        self.progress_bar['value'] = 100
        self.running = False
        self.start_button_text.set('Start')

    def _progress_bar(self):
        """ This method prints a progress bar

        This method prints a progress bar by keeping track
        of the amount of frames in the input directory
        and the output directory. This is originally
        suggested by @ArmandBernard.
        """
        # initialize variables early
        self.upscaler.progress_bar_exit_signal = False
        self.upscaler.total_frames_upscaled = 0
        self.upscaler.total_frames = 1

        # initialize progress bar values
        self.progress_bar['value'] = 0

        while not self.upscaler.progress_bar_exit_signal:
            self.progress_bar['value'] = int(100 * self.upscaler.total_frames_upscaled / self.upscaler.total_frames)
            time.sleep(1)

    def _select_input(self):
        self.input_file.set(askopenfilename(title='Select Input File'))

        # try to set an output file name automatically
        output_file = pathlib.Path(f'{self.input_file.get()}_output.mp4')

        output_file_id = 0
        while output_file.is_file() and output_file_id <= 10:
            output_file = pathlib.Path(f'{self.input_file.get()}_output_{output_file_id}.mp4')
            output_file_id += 1
        
        if not output_file.exists():
            self.output_file.set(str(output_file))

    def _select_output(self):
        self.output_file.set(asksaveasfilename(title='Select Output File'))


def read_config(config_file):
    """ Reads configuration file

    Returns a dictionary read by JSON.
    """
    with open(config_file, 'r') as raw_config:
        config = json.load(raw_config)
        return config


def absolutify_paths(config):
    """ Check to see if paths to binaries are absolute

    This function checks if paths to binary files are absolute.
    If not, then absolutify the path.

    Arguments:
        config {dict} -- configuration file dictionary

    Returns:
        dict -- configuration file dictionary
    """
    current_directory = pathlib.Path(sys.argv[0]).parent.absolute()

    # check waifu2x-caffe path
    if not re.match('^[a-z]:', config['waifu2x_caffe']['waifu2x_caffe_path'], re.IGNORECASE):
        config['waifu2x_caffe']['waifu2x_caffe_path'] = current_directory / config['waifu2x_caffe']['waifu2x_caffe_path']

    # check waifu2x-converter-cpp path
    if not re.match('^[a-z]:', config['waifu2x_converter']['waifu2x_converter_path'], re.IGNORECASE):
        config['waifu2x_converter']['waifu2x_converter_path'] = current_directory / config['waifu2x_converter']['waifu2x_converter_path']

    # check waifu2x_ncnn_vulkan path
    if not re.match('^[a-z]:', config['waifu2x_ncnn_vulkan']['waifu2x_ncnn_vulkan_path'], re.IGNORECASE):
        config['waifu2x_ncnn_vulkan']['waifu2x_ncnn_vulkan_path'] = current_directory / config['waifu2x_ncnn_vulkan']['waifu2x_ncnn_vulkan_path']

    # check ffmpeg path
    if not re.match('^[a-z]:', config['ffmpeg']['ffmpeg_path'], re.IGNORECASE):
        config['ffmpeg']['ffmpeg_path'] = current_directory / config['ffmpeg']['ffmpeg_path']

    # check video2x cache path
    if config['video2x']['video2x_cache_directory']:
        if not re.match('^[a-z]:', config['video2x']['video2x_cache_directory'], re.IGNORECASE):
            config['video2x']['video2x_cache_directory'] = current_directory / config['video2x']['video2x_cache_directory']

    return config


video2x_gui = Video2xGui()
