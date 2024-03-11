FROM archlinux AS runns_test

ARG SRC

# Install necessary packages
RUN pacman -Sy
RUN pacman -S --noconfirm iptables iproute2 gawk gcc binutils m4 autoconf make cpio
# Create a group and a user to run runnsctl
RUN groupadd runns_user
RUN useradd -m -g runns_user -G users runns_user
# Create a group for runns
RUN groupadd runns
# Switch to the user
USER runns_user:runns_user
WORKDIR /home/runns_user
COPY "$SRC" .
RUN cpio -idv < "$SRC"
# Configure RUNNS
RUN autoconf && ./configure && make
# Prepare netns
#USER root:root
#RUN ./tests/prepare_env.sh
