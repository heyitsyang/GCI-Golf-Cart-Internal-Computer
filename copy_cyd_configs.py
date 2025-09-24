#
# Library updates overwrite configuration file with defaults, so this copy
# program copies customized configuration files lv_conf.h and User_Setup.h
# into their proper folders.  
# 
# lv_conf.h only copies if the destination file does not already exist.  
# This mechanism prevents this program from endlessly copying (platformio build flag
# doesn't work as you would expect), so delete the destination .pio/libdeps/cyd/lv.conf 
# file first to cause a copy before the next build.
#
# Customizations to should be applied to the files in the NECESSARY TEMPLATE FILES folder
#

Import("env")

from shutil import copyfile
import os

# Optional: Add a check to ensure this runs only during a "build" action
# This prevents it from running for unrelated PlatformIO tasks like
# IntelliSense indexing or Serial Monitor initialization.
# if not env.get('PIOPLATFORM_BUILDTARGET') == 'build':
#     print(f"Not a build action - exiting")
#     Exit(0) 

print("Copying CYD configuration files...")

# Define source and destination paths
template_dir = os.path.join(env.get("PROJECT_DIR"), "NECESSARY TEMPLATE FILES")

# Verify the existence of the template directory
if not os.path.isdir(template_dir):
    print(f"Error: Template directory not found: {template_dir}")
    Exit(1)

user_setup_src = os.path.join(template_dir, "User_Setup.h")
user_setup_dest = os.path.join(env.get("PROJECT_LIBDEPS_DIR"), env.get("PIOENV"), "TFT_eSPI", "User_Setup.h")

# Ensure destination directory exists
os.makedirs(os.path.dirname(user_setup_dest), exist_ok=True)

# Copy TFT_eSPI configuration file
try:
    copyfile(user_setup_src, user_setup_dest)
    print(f"Copied {user_setup_src} to {user_setup_dest}")
    print("TFT_eSPI configuration file copied successfully.")
except FileNotFoundError as e:
    print(f"Error copying file: {e}. Check if source files exist and paths are correct.")
    Exit(1)
except Exception as e:
    print(f"An unexpected error occurred during file copying: {e}")
    Exit(1)


