#!/bin/sh

docker run --runtime=nvidia --rm -it \
           --net=foo \
	   --name master \
	   --privileged \
           jetson/ros:robot-base
