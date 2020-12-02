# Companion Manager
# Version 1.0.0

#it only supports changes in the express settings
#available options listed below.
#options:
#   output_path: global/path/to/file
#   keep_changes: true/false
#  express:
#    driver: #waifu2x_caffe, waifu2x_converter_cpp, waifu2x_ncnn_vulkan, srmd_ncnn_vulkan, realsr_ncnn_vulkan, anime4kcpp
#    processes: 1
#    scale_ratio: 2
#    output_width: 0
#    output_height: 0
#    preserve_frames: false
#    downscaler_threads: 0

# built-in imports
from importlib import import_module
from multiprocessing import cpu_count
from os import path
from pathlib import Path as pathl

import yaml

def generate_companion(file_path,settings):
    """Genereate companion with specified settings
    """
    with open(file_path,'w') as f:
        f.write('')
    for key in settings.keys():
        if settings[key] == '':
            del settings[key]
    with open(file_path,'a') as f:
        if 'video_name' in settings.keys() or 'video_type' in settings.keys():
            f.write('video:\n')
        if 'video_name' in settings.keys():
            f.write(("  name: '{}'\n").format(settings['video_name']))
        if 'video_type' in settings.keys():
            f.write(("  type: '{}'\n").format(settings['video_type']))
        if 'options_output_path' in settings.keys():
            f.write(("options:\n  output_path: '{}'\n").format(settings['options_output_path']))

class Companion:
    def __init__(self,yaml_path,upscaler=None):
        with open(yaml_path, 'r') as config:
            companion_dict = yaml.load(config, Loader=yaml.FullLoader)
        self.companion_path = yaml_path
        self.companion_dict = companion_dict
        self.driverChange = False
        self.Upscaler = upscaler
        self.keep_changes = True
        self.old = {}

    def get_value(self,location) -> str:
        """ Reads values from yaml

        Arguments:
            location {string} -- value path Ex: 'options/express/scale_ratio'

        Returns:
            value {string or None}
        """
        value = self.companion_dict
        location = location.split("/")
        try:
            while location != []:
                value = value[location.pop(0)]
        except KeyError:
            value = None

        return value 

    def get_int(self,location) -> int:
        """ Reads values from yaml

        Arguments:
            location {string} -- value path Ex: 'options/express/scale_ratio'

        Returns:
            value {int or None}
        """
        value = self.get_value(location)
        if value == None:
            return None
        try:
            return int(value)
        except Exception:
            raise ValueError('Requested int in a non int variable', value)

    def get_bool(self,location) -> bool:
        """ Reads values from yaml

        Arguments:
            location {string} -- value path Ex: 'options/express/scale_ratio'

        Returns:
            value {bool or None}
        """
        value = self.get_value(location)
        if value == None:
            return None
        elif value == True or value == False:
            return value
        else:
            raise ValueError('Requested bool in a non bool variable', value)

    def get_video_type(self):
        return self.get_value('video/type')

    def get_output_path(self):
        output_path = self.get_value('options/output_path')
        if output_path != None:
            output_path = pathl(output_path)
            if output_path.is_absolute() and not output_path.is_file():
                return str(output_path.absolute())
        return None

    def update_upscaler_settings(self):
        #===============================================================================
        #Checking Keep Changes
        if self.keep_changes:
            if not self.get_bool('options/keep_changes'):
                self.keep_changes = False               

        #===============================================================================
        #Updating Driver
        driver = self.get_value('options/express/driver')
        if driver != None:
            if self.Upscaler.driver != driver:
                self.driverChange = True
            if not self.keep_changes:
                self.old['driver'] = self.Upscaler.driver
            self.Upscaler.driver = driver
            
        #===============================================================================
        #Updating Processes
        processes = self.get_int('options/express/processes')
        if processes != None:
            if processes < 1:
                processes = None
            else:
                self.Upscaler.processes = processes
        if self.Upscaler.driver != 'anime4kcpp' and self.Upscaler.processes != 1:
            self.Upscaler.processes = 1

        #===============================================================================
        #Updating Scale ration, width and height
        scale_ratio = self.get_int('options/express/scale_ratio')
        if scale_ratio != None and (scale_ratio == 0 or scale_ratio < 0):
            scale_ratio = None
        output_width = self.get_int('options/express/output_width')
        if output_width != None and (output_width == 0 or output_width < 0):
            output_width = None
        output_height = self.get_int('options/express/output_height')
        if output_height != None and (output_height == 0 or output_height < 0):
            output_height = None
        
        #Sanitazing inputs
        if (output_width != None or output_height != None):
            if output_width == None:
                output_width = self.Upscaler.scale_width
            elif output_height == None:
                output_height = self.Upscaler.scale_height

        if scale_ratio != None and (output_width != None or output_height != None):
            #scale ration is overrided if output_width and output_height are 2 valid numbers
            if output_width == None or output_height == None:
                output_width = output_height = None
            else:
                scale_ratio = None

        if not (scale_ratio == None and output_width == None and output_height == None):
            self.Upscaler.scale_ratio = scale_ratio
            self.Upscaler.scale_width = output_width
            self.Upscaler.scale_height = output_height

        #===============================================================================
        #Updating preserve_frames and downscaler_threads
        preserve_frames = self.get_bool('options/express/preserve_frames')
        downscaler_threads = self.get_int('options/express/downscaler_threads')

        if preserve_frames != None:
            self.Upscaler.preserve_frames = preserve_frames
        if downscaler_threads != None and downscaler_threads >= 0:
            if downscaler_threads == 0:
                downscaler_threads = cpu_count()
            if not self.keep_changes:
                self.old['downscaler_threads'] = self.Upscaler.downscaler_threads
            self.Upscaler.downscaler_threads = downscaler_threads
            
        #print(Upscaler.__dict__)

    def load_new_driver(self,yaml_path):
        # load driver settings from yaml
        with open(yaml_path, 'r') as config:
            config = yaml.load(config, Loader=yaml.FullLoader)

        if not self.keep_changes:
            self.old['driver_settings'] = self.Upscaler.driver_settings
            self.old['driver_object'] = self.Upscaler.driver_object


        self.Upscaler.driver_settings = config[self.Upscaler.driver]
        self.Upscaler.driver_settings['path'] = path.expandvars(self.Upscaler.driver_settings['path'])

        # load driver modules
        DriverWrapperMain = getattr(import_module(f'wrappers.{self.Upscaler.driver}'), 'WrapperMain')
        self.Upscaler.driver_object = DriverWrapperMain(self.Upscaler.driver_settings)

        # load options from upscaler class into driver settings
        self.Upscaler.driver_object.load_configurations(self.Upscaler)

        # check argument sanity before running
        self.Upscaler._check_arguments()

    def last_low_storage(self):
        #called when "Last Low Storage Video" flag is found
        #return the name of the output_video and the path to a list of only new/upscaled clips
        folder_path = pathl(self.companion_path.parents[0])
        #reads and sanitaze already existing files
        with open(f'{folder_path}/AEFiles.txt','r') as f:
            aefiles = f.readlines()
        for index in range(len(aefiles)):
            line = aefiles[index]
            if "\n" in line:
                aefiles[index] = line.replace("\n","")
            if line == "":
                del aefiles[index]
        #compares all files and checks if aready existing
        new_files = []
        for file in folder_path.iterdir():
            if file.name not in aefiles: #idk how the fuck is failing
                if file.suffix != '.txt' and file.suffix != '.yaml':
                    new_files.append(file.name)
        #create new list
        with open(f'{folder_path}/clips.txt','a') as f:
            for file in new_files:
                f.write(f"file '.\{file}'\n")
        return (self.get_value('video/name') , f'{folder_path}/clips.txt')
        
    def rewind(self):
        #restores settings changed by the last companion
        if not self.keep_changes:
            for key in self.old.keys():
                self.Upscaler.__dict__[key] = self.old[key]
            