# Firmware build environment for the Cortex-M7 core of the NXP i.MX8M Plus
FROM ubuntu:24.04

ARG GCC_VERSION=14.3.rel1
ARG USERNAME=dev
ARG USER_UID=1000
ARG USER_GID=1000
ARG TARGETARCH

ENV DEBIAN_FRONTEND=noninteractive

# Build tools, debugger, serial console, and Python for west/SDK tooling
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build make \
        git curl ca-certificates xz-utils bzip2 unzip \
        python3 python3-pip python3-venv \
        gdb-multiarch openocd picocom \
        device-tree-compiler sudo \
    && rm -rf /var/lib/apt/lists/*

# Arm GNU Toolchain (arm-none-eabi) — official release from Arm
RUN case "${TARGETARCH:-amd64}" in \
        amd64) HOST=x86_64 ;; \
        arm64) HOST=aarch64 ;; \
        *) echo "Unsupported arch: ${TARGETARCH}" && exit 1 ;; \
    esac \
    && curl -fL "https://developer.arm.com/-/media/Files/downloads/gnu/${GCC_VERSION}/binrel/arm-gnu-toolchain-${GCC_VERSION}-${HOST}-arm-none-eabi.tar.xz" \
        -o /tmp/gcc.tar.xz \
    && mkdir -p /opt/arm-gnu-toolchain \
    && tar -xJf /tmp/gcc.tar.xz -C /opt/arm-gnu-toolchain --strip-components=1 \
    && rm /tmp/gcc.tar.xz

ENV ARMGCC_DIR=/opt/arm-gnu-toolchain
ENV PATH="${ARMGCC_DIR}/bin:${PATH}"

# west plus the SDK's Python deps (jinja2 for export_app, kconfiglib helpers, etc.)
# sdk-requirements.txt is a copy of mcuxsdk/scripts/requirements-base.txt
COPY sdk-requirements.txt /tmp/sdk-requirements.txt
RUN python3 -m venv /opt/venv \
    && /opt/venv/bin/pip install --no-cache-dir west -r /tmp/sdk-requirements.txt \
    && rm /tmp/sdk-requirements.txt
ENV PATH="/opt/venv/bin:${PATH}"

# Lets freestanding projects (west export_app) find the SDK in the mounted workspace
ENV SdkRootDirPath=/workspace/mcuxpresso-sdk/mcuxsdk

# Non-root user matching the host UID/GID so mounted files keep sane ownership
RUN (getent passwd ${USER_UID} | cut -d: -f1 | xargs -r userdel -r) || true \
    && (getent group ${USER_GID} || groupadd -g ${USER_GID} ${USERNAME}) \
    && useradd -m -u ${USER_UID} -g ${USER_GID} -s /bin/bash ${USERNAME} \
    && usermod -aG dialout ${USERNAME} \
    && echo "${USERNAME} ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/${USERNAME}

USER ${USERNAME}
WORKDIR /workspace
CMD ["/bin/bash"]
