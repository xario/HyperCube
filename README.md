### Warning 
Please make sure to take a backup of your HyperCube config prior to running the `update.sh` script. 
You can backup your config by going to the configuration site (e.g. http://hypercube5), selecting the `General` tab and clicking the `Download` button.

If anything goes wrong during the update, you can go to the configuration site and `General` tab to upload your backup using the `Upload` button.

### Update instructions
1. Make sure that you have the HyperCube plugged in before running the `update.sh` script.
2. Make sure that you have the latest changes pulled from the Git repository. If you are not sure, you can run `git pull`.
3. Run the update script by using the following command: `bash update.sh`
4. If this is the first time you have run the script on your computer (or if you have cleared the docker cache), you will need to build the helper Docker image. This can be a rather lengthy process the first time around, but future builds should be fairly quick, if the only changes are to the code (both React and Arduino code).
5. When the build is complete, the update script will ask for the number of your HyperCube. This can be found by looking under the base of the HyperCube. It is only the number e.g. [1-9] that has to be entered, not the full name of the cube.
6. After entering the number of the HyperCube, the script will continue on and download the current configuration of the Cube.
7. First the script will extract any WiFi, Timer, Time, Clockify and Cube configuration, as it need to be merged with the new React build files when a new data image is created and uploaded to the Cube.
8. Next the React build files will be added to the data dir such that the updated files will be included in the data image.
9. Then the Arduino code will be complied and uploaded to the Cube.
10. Finally, the data image will be generated and uploaded to the Cube as well.
11. The process should now be completed, and you have an updated HyperCube.

### Logging instructions
1. Run the `setup-usb-hooks.sh` script to allow the logging to begin when a HyperCube is plugged in.
2. When the script asks for a password enter the password for the user currently logged in.
3. In order to start logging: disconnect the cube and reconnect it.
4. To verify that the logging has begun, run the `docker ps` command. You should see a running container with the name `hyperlog`.
5. The log output from the HyperCube will be appended to a file in your home directory. You can see the logs being added by running `tail -f ~/cube.log`.