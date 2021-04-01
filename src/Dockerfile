

##################################################
# Section 1: Build the application
FROM ubuntu:18.04 as builder
MAINTAINER Leith Hobson leith@student.chalmers.se

RUN apt-get update -y && \
    apt-get upgrade -y && \
    apt-get dist-upgrade -y

RUN apt-get install -y --no-install-recommends \
        cmake \
        build-essential

ADD . /opt/sources
WORKDIR /opt/sources

RUN cd /opt/sources && \
    mkdir build && \
    cd build && \
    cmake -D CMAKE_BUILD_TYPE=Release .. && \
    make && make test && cp helloworld /tmp

##################################################
# Section 2: Bundle the application.
FROM ubuntu:18.04
MAINTAINER Leith Hobson leith@student.chalmers.se

RUN apt-get update -y && \
    apt-get upgrade -y && \
    apt-get dist-upgrade -y && \
    # Delete cached sources list to recover wasted storage space
    rm -rf /var/lib/apt/lists/*

WORKDIR /opt
COPY --from=builder /tmp/helloworld .
ENTRYPOINT ["/opt/helloworld"]

