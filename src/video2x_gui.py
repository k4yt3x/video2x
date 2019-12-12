#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Creator: Video2X GUI
Author: K4YT3X
Date Created: July 27, 2019
Last Modified: December 11, 2019

Description: A simple GUI for Video2X made with tkinter.
"""

# local imports
from exceptions import *
from upscaler import Upscaler

# built-in imports
from tkinter import *
from tkinter import messagebox
from tkinter import ttk
from tkinter.filedialog import *
import contextlib
import pathlib
import sys
import tempfile
import threading
import time
import yaml

VERSION = '1.1.3'

VIDEO2X_CONFIG = pathlib.Path(sys.argv[0]).parent.absolute() / 'video2x.yaml'

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

DEMUXER_EXTENSIONS = {'3dostr', '4xm', 'aa', 'aac', 'ac3', 'acm', 'act',
                      'adf', 'adp', 'ads', 'adx', 'aea', 'afc', 'aiff', 'aix', 'alaw',
                      'alias_pix', 'alsa', 'amr', 'amrnb', 'amrwb', 'anm', 'apc', 'ape',
                      'apng', 'aptx', 'aptx_hd', 'aqtitle', 'asf', 'asf_o', 'ass', 'ast',
                      'au', 'avi', 'avisynth', 'avr', 'avs', 'avs2', 'bethsoftvid', 'bfi',
                      'bfstm', 'bin', 'bink', 'bit', 'bmp_pipe', 'bmv', 'boa', 'brender_pix',
                      'brstm', 'c93', 'caf', 'cavsvideo', 'cdg', 'cdxl', 'cine', 'codec2',
                      'codec2raw', 'concat', 'dash', 'data', 'daud', 'dcstr', 'dds_pipe',
                      'dfa', 'dirac', 'dnxhd', 'dpx_pipe', 'dsf', 'dsicin', 'dss', 'dts',
                      'dtshd', 'dv', 'dvbsub', 'dvbtxt', 'dxa', 'ea', 'ea_cdata', 'eac3',
                      'epaf', 'exr_pipe', 'f32be', 'f32le', 'f64be', 'f64le', 'fbdev',
                      'ffmetadata', 'film_cpk', 'filmstrip', 'fits', 'flac', 'flic', 'flv',
                      'frm', 'fsb', 'g722', 'g723_1', 'g726', 'g726le', 'g729', 'gdv', 'genh',
                      'gif', 'gsm', 'gxf', 'h261', 'h263', 'h264', 'hevc', 'hls', 'applehttp',
                      'hnm', 'ico', 'idcin', 'idf', 'iec61883', 'iff', 'ilbc', 'image2',
                      'image2pipe', 'ingenient', 'ipmovie', 'ircam', 'iss', 'iv8', 'ivf',
                      'ivr', 'j2k_pipe', 'jack', 'jacosub', 'jpeg_pipe', 'jpegls_pipe',
                      'jv', 'kmsgrab', 'lavfi', 'libcdio', 'libdc1394', 'libgme', 'libopenmpt',
                      'live_flv', 'lmlm4', 'loas', 'lrc', 'lvf', 'lxf', 'm4v', 'matroska', 'webm',
                      'mgsts', 'microdvd', 'mjpeg', 'mjpeg_2000', 'mlp', 'mlv', 'mm', 'mmf',
                      'mov', 'mp4', 'm4a', '3gp', '3g2', 'mj2', 'mp3', 'mpc', 'mpc8', 'mpeg',
                      'mpegts', 'mpegtsraw', 'mpegvideo', 'mpjpeg', 'mpl2', 'mpsub', 'msf',
                      'msnwctcp', 'mtaf', 'mtv', 'mulaw', 'musx', 'mv', 'mvi', 'mxf', 'mxg',
                      'nc', 'nistsphere', 'nsp', 'nsv', 'nut', 'nuv', 'ogg', 'oma', 'openal',
                      'oss', 'paf', 'pam_pipe', 'pbm_pipe', 'pcx_pipe', 'pgm_pipe', 'pgmyuv_pipe',
                      'pictor_pipe', 'pjs', 'pmp', 'png_pipe', 'ppm_pipe', 'psd_pipe', 'psxstr',
                      'pulse', 'pva', 'pvf', 'qcp', 'qdraw_pipe', 'r3d', 'rawvideo', 'realtext',
                      'redspark', 'rl2', 'rm', 'roq', 'rpl', 'rsd', 'rso', 'rtp', 'rtsp',
                      's16be', 's16le', 's24be', 's24le', 's32be', 's32le', 's337m', 's8',
                      'sami', 'sap', 'sbc', 'sbg', 'scc', 'sdp', 'sdr2', 'sds', 'sdx', 'ser',
                      'sgi_pipe', 'shn', 'siff', 'sln', 'smjpeg', 'smk', 'smush', 'sndio',
                      'sol', 'sox', 'spdif', 'srt', 'stl', 'subviewer', 'subviewer1', 'sunrast_pipe',
                      'sup', 'svag', 'svg_pipe', 'swf', 'tak', 'tedcaptions', 'thp', 'tiertexseq',
                      'tiff_pipe', 'tmv', 'truehd', 'tta', 'tty', 'txd', 'ty', 'u16be', 'u16le',
                      'u24be', 'u24le', 'u32be', 'u32le', 'u8', 'v210', 'v210x', 'vag', 'vc1',
                      'vc1test', 'vidc', 'video4linux2', 'v4l2', 'vivo', 'vmd', 'vobsub', 'voc',
                      'vpk', 'vplayer', 'vqf', 'w64', 'wav', 'wc3movie', 'webm_dash_manifest',
                      'webp_pipe', 'webvtt', 'wsaud', 'wsd', 'wsvqa', 'wtv', 'wv', 'wve', 'x11grab',
                      'xa', 'xbin', 'xmv', 'xpm_pipe', 'xvag', 'xwd_pipe', 'xwma', 'yop', 'yuv4mpegpipe'}


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
        self.preserve_frames.set(False)
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

        try:
            # start timer
            begin_time = time.time()

            # read configuration file
            config = read_config(VIDEO2X_CONFIG)
            config = absolutify_paths(config)

            input_file = pathlib.Path(self.input_file.get())
            output_file = pathlib.Path(self.output_file.get())
            driver = AVAILABLE_DRIVERS[self.driver.get()]

            # load specified driver's config into driver_settings
            driver_settings = config[driver]

            # if executable doesn't exist, show warning
            if not pathlib.Path(driver_settings['path']).is_file() and not pathlib.Path(f'{driver_settings["path"]}.exe').is_file():
                messagebox.showerror('Error', 'Specified driver directory doesn\'t exist\nPlease check the configuration file settings')
                raise FileNotFoundError(driver_settings['path'])

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

            self.upscaler = Upscaler(input_video=input_file, output_video=output_file, method=method, driver_settings=driver_settings, ffmpeg_settings=ffmpeg_settings)

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
        
        except Exception as e:
            messagebox.showerror('Error', f'Upscaler ran into an error:\n{e}')

            # try cleaning up temp directories
            with contextlib.suppress(Exception):
                self.upscaler.cleanup_temp_directories()


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

        # remove input file extension
        input_filename = str(self.input_file.get())
        for extension in DEMUXER_EXTENSIONS:
            if input_filename.endswith(f'.{extension}'):
                input_filename = input_filename[:-1 - len(extension)]

        # try to set an output file name automatically
        output_file = pathlib.Path(f'{input_filename}_output.mp4')

        output_file_id = 0
        while output_file.is_file() and output_file_id <= 10:
            output_file = pathlib.Path(f'{input_filename}_output_{output_file_id}.mp4')
            output_file_id += 1
        
        if not output_file.exists():
            self.output_file.set(str(output_file))

    def _select_output(self):
        self.output_file.set(asksaveasfilename(title='Select Output File'))


def read_config(config_file):
    """ Reads configuration file

    Returns a dictionary read by parsing Video2X config.
    """
    with open(config_file, 'r') as raw_config:
        config = yaml.load(raw_config, Loader=yaml.FullLoader)
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
    if not re.match('^[a-z]:', config['waifu2x_caffe']['path'], re.IGNORECASE):
        config['waifu2x_caffe']['path'] = current_directory / config['waifu2x_caffe']['path']

    # check waifu2x-converter-cpp path
    if not re.match('^[a-z]:', config['waifu2x_converter']['path'], re.IGNORECASE):
        config['waifu2x_converter']['path'] = current_directory / config['waifu2x_converter']['path']

    # check waifu2x_ncnn_vulkan path
    if not re.match('^[a-z]:', config['waifu2x_ncnn_vulkan']['path'], re.IGNORECASE):
        config['waifu2x_ncnn_vulkan']['path'] = current_directory / config['waifu2x_ncnn_vulkan']['path']

    # check ffmpeg path
    if not re.match('^[a-z]:', config['ffmpeg']['ffmpeg_path'], re.IGNORECASE):
        config['ffmpeg']['ffmpeg_path'] = current_directory / config['ffmpeg']['ffmpeg_path']

    # check video2x cache path
    if config['video2x']['video2x_cache_directory']:
        if not re.match('^[a-z]:', config['video2x']['video2x_cache_directory'], re.IGNORECASE):
            config['video2x']['video2x_cache_directory'] = current_directory / config['video2x']['video2x_cache_directory']

    return config


video2x_gui = Video2xGui()
