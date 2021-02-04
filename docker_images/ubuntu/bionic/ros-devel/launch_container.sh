docker run -it --rm \
	-e DISPLAY=$DISPLAY \
       	-v /tmp/.X11-unix:/tmp/.X11-unix \
	-v /home/mhuertas/Work/Robotin-Project/robotin_project:/root/catkin_ws/src/robotin_project \
	--env ROS_MASTER_URI=http://192.168.1.61:11311 \
       	-v /dev:/dev \
	--network=host \
	--privileged \
	ubuntu-melodic/ros:ros-devel \
    bash



