import os
import subprocess
import platform
import sys
import time
import urllib

import importlib.util as importlib_util

from sys import platform
from zipfile import ZipFile
from pathlib import Path

class PythonConfiguration:
    @classmethod
    def Validate(cls):
        print("Validating python...")
        if not cls.__ValidatePython():
            print("Failed to validate python. Exiting early.")
            return # cannot validate further

        packages = ["requests"]
        for packageName in packages:
            if not cls.__ValidatePackage(packageName):
                return # cannot validate further

    @classmethod
    def __ValidatePython(cls, versionMajor = 3, versionMinor = 6):
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
        
        print("Installing {packageName} module...".format(packageName=packageName))
        subprocess.check_call(['python3', '-m', 'pip', 'install', packageName])

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
                print("URL Error encountered: {reason}. Proceeding with backup...{newline}".format(reason=e.reason, newline="\n\n"))
                os.remove(filepath)
                pass
            except urllib.error.HTTPError as e:
                print("HTTP Error  encountered: {code}. Proceeding with backup...{newline}".format(code=e.code, newline="\n\n"))
                os.remove(filepath)
                pass
            except:
                print("Something went wrong. Proceeding with backup...{newline}".format(newline="\n\n"))
                os.remove(filepath)
                pass
        raise ValueError("Failed to download {filepath}".format(filepath=filepath))
    if not(type(url) is str):
        raise TypeError("Argument 'url' must be of type list or string")

    with open(filepath, 'wb') as f:
        import requests
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
            UnzippedFilePath = os.path.abspath("{zipFileLocation}/{zippedFileName}".format(zipFileLocation=zipFileLocation, zippedFileName=zippedFileName))
            os.makedirs(os.path.dirname(UnzippedFilePath), exist_ok=True)
            if not os.path.isfile(UnzippedFilePath):
                zipFileFolder.extract(zippedFileName, path=zipFileLocation, pwd=None)

    if deleteZipFile:
        os.remove(zipFilePath) # delete zip file

def InstallZip(directory, url, version, name):
    permissionGranted = False
    while not permissionGranted:
        reply = str(input("{1:s} not found. Would you like to download {1:s} {0:s}? [Y/N]: ".format(version, name))).lower().strip()[:1]
        if reply == 'n':
            return False
        permissionGranted = (reply == 'y')
    
    if (platform == "win32"):
        path = "{directory}/{name}-{version}.zip".format(directory=directory, name=name, version=version)
        print("Downloading {0:s} to {1:s}".format(url, path))
        DownloadFile(url, path)
        print("Extracting", path)
        UnzipFile(path, deleteZipFile=True)
        print("{name} {version} has been downloaded to '{directory}'".format(name=name, version=version, directory=directory))
    elif (platform == "darwin"):
        deleteZipFile = True
        path = "{directory}/{name}-{version}.dmg".format(directory=directory, name=name, version=version)
        print("Downloading {0:s} to {1:s}".format(url, path))
        DownloadFile(url, path)
        print("Extracting", path)
        os.system("hdiutil attach {path} -quiet".format(path=path))
        os.system("cp -r '/Volumes/{name}/{name}.framework' {directory}".format(name=name, directory=directory))
        os.system("hdiutil detach /Volumes/{name} -quiet".format(name=name))
        if deleteZipFile:
            os.remove(os.path.abspath(path)) # delete zip file
    return True

def ValidateDependency(path, directory, url, version, name):
    if (platform == "win32"):
        return path.exists() or InstallZip(directory, url, version, name)
    elif (platform == "darwin"):
        path = Path("{directory}/{name}.framework".format(directory=directory, name=name))
        return path.exists() or InstallZip(directory, url, version, name)

def ValidateDependencies():
    directory = os.path.dirname(__file__)
    vendor_directory = os.path.join(directory, "../external")
    
    # Ensure that these versions are consistent with those found in the protegon/cmake/FindProtegon.cmake file.
    SDL2_version = "2.26.5"
    SDL2_image_version = "2.6.3"
    SDL2_ttf_version = "2.20.2"
    SDL2_mixer_version = "2.6.3"

    # TODO: Change zip variable name to more generic version.
    if (platform == "win32"):
        SDL2_zip = "https://github.com/libsdl-org/SDL/releases/download/release-{SDL2_version}/SDL2-devel-{SDL2_version}-VC.zip".format(SDL2_version=SDL2_version)
        SDL2_image_zip = "https://github.com/libsdl-org/SDL_image/releases/download/release-{SDL2_image_version}/SDL2_image-devel-{SDL2_image_version}-VC.zip".format(SDL2_image_version=SDL2_image_version)
        SDL2_ttf_zip = "https://github.com/libsdl-org/SDL_ttf/releases/download/release-{SDL2_ttf_version}/SDL2_ttf-devel-{SDL2_ttf_version}-VC.zip".format(SDL2_ttf_version=SDL2_ttf_version)
        SDL2_mixer_zip = "https://github.com/libsdl-org/SDL_mixer/releases/download/release-{SDL2_mixer_version}/SDL2_mixer-devel-{SDL2_mixer_version}-VC.zip".format(SDL2_mixer_version=SDL2_mixer_version)
    elif (platform == "darwin"): # MacOS
        SDL2_zip = "https://github.com/libsdl-org/SDL/releases/download/release-{SDL2_version}/SDL2-{SDL2_version}.dmg".format(SDL2_version=SDL2_version)
        SDL2_image_zip = "https://github.com/libsdl-org/SDL_image/releases/download/release-{SDL2_image_version}/SDL2_image-{SDL2_image_version}.dmg".format(SDL2_image_version=SDL2_image_version)
        SDL2_ttf_zip = "https://github.com/libsdl-org/SDL_ttf/releases/download/release-{SDL2_ttf_version}/SDL2_ttf-{SDL2_ttf_version}.dmg".format(SDL2_ttf_version=SDL2_ttf_version)
        SDL2_mixer_zip = "https://github.com/libsdl-org/SDL_mixer/releases/download/release-{SDL2_mixer_version}/SDL2_mixer-{SDL2_mixer_version}.dmg".format(SDL2_mixer_version=SDL2_mixer_version)

    SDL2_path = Path("{vendor_directory}/SDL2-{SDL2_version}/include/SDL.h".format(vendor_directory=vendor_directory, SDL2_version=SDL2_version));
    SDL2_image_path = Path("{vendor_directory}/SDL2_image-{SDL2_image_version}/include/SDL_image.h".format(vendor_directory=vendor_directory, SDL2_image_version=SDL2_image_version));
    SDL2_mixer_path = Path("{vendor_directory}/SDL2_mixer-{SDL2_mixer_version}/include/SDL_mixer.h".format(vendor_directory=vendor_directory, SDL2_mixer_version=SDL2_mixer_version));
    SDL2_ttf_path = Path("{vendor_directory}/SDL2_ttf-{SDL2_ttf_version}/include/SDL_ttf.h".format(vendor_directory=vendor_directory, SDL2_ttf_version=SDL2_ttf_version));

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
elif platform == "win32" or platform == "darwin": # Windows or MacOS
    dependencies_installed = ValidateDependencies()
    if not dependencies_installed:
        raise Exception("Failed to install a dependency on Windows")
