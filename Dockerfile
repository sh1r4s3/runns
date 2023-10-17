FROM archlinux AS runns_test

ARG SRC_TAR

# Install necessary packages
RUN pacman -Sy
RUN pacman -S --noconfirm iptables iproute2 gawk gcc binutils m4 autoconf make
# Create a group and a user to run runnsctl
RUN groupadd runns_user
RUN useradd -m -g runns_user -G users runns_user
# Create a group for runns
RUN groupadd runns
# Switch to the user and make necessary directories
USER runns_user:runns_user
# Weee
WORKDIR /home/runns_user
# Copy the server itself (server.tar.gz)
COPY runns.tar.xz .
# Entry script
RUN tar xJf "$SRC_TAR"
# Run entry script
RUN autoconf && ./configure && make && make tests
