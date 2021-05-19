# This is a Dockerfile

##################################################
# Section 1: Build the application
FROM ubuntu:18.04 as builder

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update -y && \
    apt-get upgrade -y && \
    apt-get dist-upgrade -y

RUN apt-get install -y --no-install-recommends \
    cmake \
    build-essential \
    ca-certificates \
    libopencv-dev 

ADD . /opt/sources
WORKDIR /opt/sources

RUN mkdir build && \
    cd build && \
    cmake -D CMAKE_BUILD_TYPE=Release -D CMAKE_INSTALL_PREFIX=/tmp .. && \
    make && make install &&\
    cp Group17-Steering-Wheel-Angle /tmp

# Section 1 end.
##################################################
# Section 2: Bundle the application.
FROM ubuntu:18.04

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update -y && \
    apt-get upgrade -y && \
    apt-get dist-upgrade -y

RUN apt-get install -y --no-install-recommends \
    libopencv-core3.2 \
    libopencv-highgui3.2 \
    libopencv-imgproc3.2 

WORKDIR /opt
COPY --from=builder /tmp/Group17-Steering-Wheel-Angle .
# The application entry point
ENTRYPOINT ["/opt/Group17-Steering-Wheel-Angle"]
# End of section 2
