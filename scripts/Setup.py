import os
import subprocess
import platform
import sys
import requests
import time
import urllib

import importlib.util as importlib_util

from sys import platform
from zipfile import ZipFile
from pathlib import Path

class PythonConfiguration:
    @classmethod
    def Validate(cls):
        if not cls.__ValidatePython():
            return # cannot validate further

        for packageName in ["requests"]:
            if not cls.__ValidatePackage(packageName):
                return # cannot validate further

    @classmethod
    def __ValidatePython(cls, versionMajor = 3, versionMinor = 3):
        if sys.version is not None:
            print("Python version {0:d}.{1:d}.{2:d} detected.".format( \
                sys.version_info.major, sys.version_info.minor, sys.version_info.micro))
            if sys.version_info.major < versionMajor or (sys.version_info.major == versionMajor and sys.version_info.minor < versionMinor):
                print("Python version too low, expected version {0:d}.{1:d} or higher.".format( \
                    versionMajor, versionMinor))
                return False
            return True

    @classmethod
    def __ValidatePackage(cls, packageName):
        if importlib_util.find_spec(packageName) is None:
            return cls.__InstallPackage(packageName)
        return True

    @classmethod
    def __InstallPackage(cls, packageName):
        permissionGranted = False
        while not permissionGranted:
            reply = str(input("Would you like to install Python package '{0:s}'? [Y/N]: ".format(packageName))).lower().strip()[:1]
            if reply == 'n':
                return False
            permissionGranted = (reply == 'y')
        
        print(f"Installing {packageName} module...")
        subprocess.check_call(['python', '-m', 'pip', 'install', packageName])

        return cls.__ValidatePackage(packageName)

def DownloadFile(url, filepath):
    path = filepath
    filepath = os.path.abspath(filepath)
    os.makedirs(os.path.dirname(filepath), exist_ok=True)
            
    if (type(url) is list):
        for url_option in url:
            print("Downloading", url_option)
            try:
                DownloadFile(url_option, filepath)
                return
            except urllib.error.URLError as e:
                print(f"URL Error encountered: {e.reason}. Proceeding with backup...\n\n")
                os.remove(filepath)
                pass
            except urllib.error.HTTPError as e:
                print(f"HTTP Error  encountered: {e.code}. Proceeding with backup...\n\n")
                os.remove(filepath)
                pass
            except:
                print(f"Something went wrong. Proceeding with backup...\n\n")
                os.remove(filepath)
                pass
        raise ValueError(f"Failed to download {filepath}")
    if not(type(url) is str):
        raise TypeError("Argument 'url' must be of type list or string")

    with open(filepath, 'wb') as f:
        headers = {'User-Agent': "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_4) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/83.0.4103.97 Safari/537.36"}
        response = requests.get(url, headers=headers, stream=True)
        total = response.headers.get('content-length')
        if total is None:
            f.write(response.content)
        else:
            total = int(total)
            for data in response.iter_content(chunk_size=max(int(total/1000), 1024*1024)):
                f.write(data)
                
def UnzipFile(filepath, deleteZipFile=True):
    zipFilePath = os.path.abspath(filepath) # get full path of files
    zipFileLocation = os.path.dirname(zipFilePath)
    zipFileContent = dict()
    with ZipFile(zipFilePath, 'r') as zipFileFolder:
        for name in zipFileFolder.namelist():
            zipFileContent[name] = zipFileFolder.getinfo(name).file_size
        for zippedFileName, zippedFileSize in zipFileContent.items():
            UnzippedFilePath = os.path.abspath(f"{zipFileLocation}/{zippedFileName}")
            os.makedirs(os.path.dirname(UnzippedFilePath), exist_ok=True)
            if not os.path.isfile(UnzippedFilePath):
                zipFileFolder.extract(zippedFileName, path=zipFileLocation, pwd=None)

    if deleteZipFile:
        os.remove(zipFilePath) # delete zip file

def InstallZip(directory, zip_url, version, name):
    permissionGranted = False
    while not permissionGranted:
        reply = str(input("{1:s} not found. Would you like to download {1:s} {0:s}? [Y/N]: ".format(version, name))).lower().strip()[:1]
        if reply == 'n':
            return False
        permissionGranted = (reply == 'y')

    path = f"{directory}/{name}-{version}-windows.zip"
    print("Downloading {0:s} to {1:s}".format(zip_url, path))
    DownloadFile(zip_url, path)
    print("Extracting", path)
    UnzipFile(path, deleteZipFile=True)
    print(f"{name} {version} has been downloaded to '{directory}'")
    return True

def ValidateDependency(path, directory, zip, version, name):
    return path.exists() or InstallZip(directory, zip, version, name)

def ValidateDependencies():
    directory = os.path.dirname(__file__)
    vendor_directory = os.path.join(directory, "../vendor")
    SDL2_version = "2.23.1"
    SDL2_image_version = "2.0.5"
    SDL2_ttf_version = "2.0.15"
    SDL2_mixer_version = "2.0.4"
    SDL2_zip = f"https://github.com/libsdl-org/SDL/releases/download/prerelease-2.23.1/SDL2-devel-2.23.1-VC.zip"
    #SDL2_zip = f"https://www.libsdl.org/release/SDL2-devel-{SDL2_version}-VC.zip"
    SDL2_image_zip = f"https://www.libsdl.org/projects/SDL_image/release/SDL2_image-devel-{SDL2_image_version}-VC.zip"
    SDL2_ttf_zip = f"https://www.libsdl.org/projects/SDL_ttf/release/SDL2_ttf-devel-{SDL2_ttf_version}-VC.zip"
    SDL2_mixer_zip = f"https://www.libsdl.org/projects/SDL_mixer/release/SDL2_mixer-devel-{SDL2_mixer_version}-VC.zip"
    SDL2_path = Path(f"{vendor_directory}/SDL2-{SDL2_version}/include/SDL.h");
    SDL2_image_path = Path(f"{vendor_directory}/SDL2_image-{SDL2_image_version}/include/SDL_image.h");
    SDL2_mixer_path = Path(f"{vendor_directory}/SDL2_mixer-{SDL2_mixer_version}/include/SDL_mixer.h");
    SDL2_ttf_path = Path(f"{vendor_directory}/SDL2_ttf-{SDL2_ttf_version}/include/SDL_ttf.h");
    SDL2_found = ValidateDependency(SDL2_path, vendor_directory, SDL2_zip, SDL2_version, "SDL2")
    SDL2_image_found = ValidateDependency(SDL2_image_path, vendor_directory, SDL2_image_zip, SDL2_image_version, "SDL2_image")
    SDL2_ttf_found = ValidateDependency(SDL2_ttf_path, vendor_directory, SDL2_ttf_zip, SDL2_ttf_version, "SDL2_ttf")
    SDL2_mixer_found = ValidateDependency(SDL2_mixer_path, vendor_directory, SDL2_mixer_zip, SDL2_mixer_version, "SDL2_mixer")
    return SDL2_found and SDL2_image_found and SDL2_ttf_found and SDL2_mixer_found

PythonConfiguration.Validate()

if platform == "linux" or platform == "linux2": # Linux
    os.system("sudo apt install -y libsdl2-dev libsdl2-2.0-0 \
                       libjpeg-dev libwebp-dev libtiff5-dev libsdl2-image-dev libsdl2-image-2.0-0 \
                       libmikmod-dev libfishsound1-dev libsmpeg-dev liboggz2-dev libflac-dev libfluidsynth-dev libsdl2-mixer-dev libsdl2-mixer-2.0-0 \
                       libfreetype6-dev libsdl2-ttf-dev libsdl2-ttf-2.0-0")
    print("Installed SDL modules on Linux")
elif platform == "darwin": # OS X
    raise Exception("Cannot automatically install SDL for OS X")
elif platform == "win32": # Windows
    dependencies_installed = ValidateDependencies()
    if not dependencies_installed:
        raise Exception("Failed to install a dependency on Windows")