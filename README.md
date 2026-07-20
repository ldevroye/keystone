# Keystone: An Open-Source Secure Enclave Framework for RISC-V Processors

![Documentation Status](https://readthedocs.org/projects/keystone-enclave/badge/)
[![Build Status](https://travis-ci.org/keystone-enclave/keystone.svg?branch=master)](https://travis-ci.org/keystone-enclave/keystone/)
[![OpenSSF Best Practices](https://www.bestpractices.dev/projects/8916/badge)](https://www.bestpractices.dev/projects/8916)

> Visit [Project Website](https://keystone-enclave.org) for more information.

## Introduction

Keystone is an open-source project that builds trusted execution environments (TEEs) for RISC-V systems. Its hardware-enforced and software-defined memory isolation enables trusted computing (a.k.a. confidential computing) with various threat models and functionalities. The implementation is platform-agnostic, making Keystone portable across different RISC-V platforms with minimal engineering efforts.


## Goals

Keystone is a free and open framework for architecting and deploying TEEs on RISC-V hardware platforms. The project's goals are:

* **Enable TEE on (almost) all RISC-V processors**: Keystone aims to support as many RISC-V processor cores that follow RISC-V standard ISA and sub-ISAs as possible. This will help hardware designers and manufacturers to enable TEE with minimal efforts.

* **Make TEE easy to customize depending on needs**: while providing simple TEE features, Keystone also aims to allow various customization that depends on platform-specific features or non-standard sub-ISAs. We borrow the concept from software-defined network, where hardware platform provides *primitives* and the software leverages the primitives to implement specific functionalities or meet security requirements.

* **Reduce the cost of building TEE**: Keystone aims to reduce the cost of building TEE or TEE-based systems. We achieve this by reusing the implementation across multiple different platforms, reducing hardware integration cost, reducing verification cost, and integrating with existing software tools. We hope that anyone can simply extend Keystone to build their own novel TEE design with very low cost.


## Status

Keystone started as an academic project that helps researchers to build and test their ideas. 
Now, Keystone is an **Incubation Stage** open-source project of the Confidential Computing Consortium (CCC) under the Linux Foundation. 

Keystone has helped many researchers focus on their creative ideas instead of building TEE by themselves from scratch.
This resulted in many innovative research projects and publications, which have been pushing the technical advancement of TEEs.

We are currently trying to make Keystone production-ready. You can find the latest general roadmap of Keystone [here](https://docs.google.com/document/d/1AxT0w6NCtfvZcFE1wbZAkAODftqRYhpHaj63mvnQQqA/edit?usp=sharing)

Here are some ongoing and/or planned efforts towards the goal:

* **Technical Improvements**: Make Keystone more usable and on par with existing industry solutions, including memory isolation improvement, better application and hardware support, and additional features.

* **Parity with Industry Standards**: Make Keystone follow the industry standard. This includes standard cryptography, measured boot, and remote attestation protocols. 

* **Hardware Integration**: Partner with RISC-V hardware designer/vendor to fully integrate with the hardware. This includes integration with hardware root-of-trust, memory encryption engine, and crypto accelerators.

## Documentation

See [docs](http://docs.keystone-enclave.org) for getting started.

## Hardware Support

Keystone requires a standard RISC-V platform with a *hardware root of trust* --- including secure key storage and measured boot. Currently, no hardware root of trust has been designed or manufactured specifically for Keystone. If you have a open-source root-of-trust we'd love to integrate with it!

As this project focuses more on the software stack and the toolchain, you can still run the full Keystone software stack on top of a few RISC-V platforms without a real root-of-trust. See https://github.com/keystone-enclave/keystone/tree/master/sm/plat for the supported platforms. In general, `generic` should work with most of the standard RISC-V cores as long as they support:

- RV64 with SV39 addressing mode (or RV32 with SV32)
- M/S/U privilege modes
- More than 4 PMP registers

For full security, platform architect needs to provide the followings

- Entropy source (and ideally a platform specific random number generator)
- Measured boot
- Secure on-chip key storage

Keystone doesn’t provide high-performance hardware-based memory encryption, as it requires a proprietary memory controller. Instead, it provides an example software-based encryption, which uses scratchpad SRAM (if any) to encrypt physical pages.

## Team

Contributors

- Gregor Haas
- Evgeny Pobachienko
- Jakob Sorensen
- David Kholbrenner
- Alex Thomas
- Cathy Lu
- Gui Andrade
- Kevin Chen
- Stephan Kaminsky
- Dayeol Lee (Maintainer)

Advisors

- David Kohlbrenner @ UW
- Shweta Shinde @ ETH Zurich
- Krste Asanovic @ UCB
- Dawn Song @ UCB

## License

Keystone is under BSD-3.

## Contributing

See CONTRIBUTING.md

## Citation

If you want to cite the project, please use the following bibtex:

```
@inproceedings{lee2019keystone,
    title={Keystone: An Open Framework for Architecting Trusted Execution Environments},
    author={Dayeol Lee and David Kohlbrenner and Shweta Shinde and Krste Asanovic and Dawn Song},
    year={2020},
    booktitle = {Proceedings of the Fifteenth European Conference on Computer Systems},
    series = {EuroSys’20}
}
```

## This fork - Rewind

This fork is made for the Master Thesis 2025-26. The goal is to implement a checkpointing algorithm that is sealed from the host and add faults happening following a pattern (mocking real life patterns for different types of faults). Then analysing the results and the impact (time, data, attack vector, etc.)

Most of the work is done in '[examples/rewind/](examples/rewind/)' which is a "fork" of the '[examples/hello/](examples/hello/)' folder.

### Authors

Louis Devroye (loudevroye@gmail.com)

### Additions

- checkpointing mechanism (discrete) '[examples/rewind/eapp/checkpoint.c](examples/rewind/eapp/checkpoint.c)'
- Sealing/cryptographic mechanism (basic) '[examples/rewind/eapp/crypto.c](examples/rewind/eapp/crypto.c)' that uses [runtime/crypto/aes.c](runtime/crypto/aes.c) cryptographic implementation
- Fault model (splitmix64) '[examples/rewind/eapp/fault.c](examples/rewind/eapp/fault.c)' that use basic pseudo-randomness
- Basic logging using OCALLs

### Setup

all compilation and run commands can be found on the official documentation website : [https://docs.keystone-enclave.org/en/latest/Getting-Started/index.html](https://docs.keystone-enclave.org/en/latest/Getting-Started/index.html).

To save some trouble-shooting here are the steps that were followed to setup **this fork**.
- [1.2.1.1 Setup repository](https://docs.keystone-enclave.org/en/latest/Getting-Started/QEMU-Setup-Repository.html)  
  \$```git clone --recurse-submodules https://github.com/keystone-enclave/keystone.git```  
- [1.2.1.2. Install Dependencies](https://docs.keystone-enclave.org/en/latest/Getting-Started/QEMU-Install-Dependencies.html)  
```text  
$sudo apt update
sudo apt install autoconf automake autotools-dev bc \
bison build-essential curl expat jq libexpat1-dev flex gawk gcc git \
gperf libgmp-dev libmpc-dev libmpfr-dev libtool texinfo tmux \
patchutils zlib1g-dev wget bzip2 patch vim-common lbzip2 python3 \
pkg-config libglib2.0-dev libpixman-1-dev libssl-dev screen \
device-tree-compiler expect makeself unzip cpio rsync cmake ninja-build p7zip-full
``` 
- [1.2.1.3. Compile Sources](https://docs.keystone-enclave.org/en/latest/Getting-Started/QEMU-Compile-Sources.html)   
  \$```make buildroot-configure```   
  \$```make linux-configure```  
  These might take some time, it's normal.   
- One should add the keystone path with \$```nano ~/.bashrc```
```text
export PATH=/opt/riscv/bin:$PATH
export KEYSTONE_SDK_DIR=/keystone/sdk/build64/
```   
  Then restarting it using \$```source ~/.bashrc```


### Compilation and Run

All of the following steps are contained in the [usage](#usage) section' scripts. However, they are detailed here to avoid confusion and allow the user to better understand what is their logic.

#### Compilation

To compile the examples we just created, they first need to have a specific architecture:

```text
examples/name/
├── CMakeLists.txt
├── eapp/
│   ├── name.c
└── host/
    └── host.cpp
```

The CMake must be changed according to the new names and dependencies. A good way to create one is to just copy the hello example and tweak it.

Then the real compilation can start.   
- First thing to know (to avoid doing things blindly) is that **Logs** when compiling can be found in the '[build-generic64/build.log](build-generic64/build.log)' (**build-\$PLATFORM$BITS/build.log**) file   
- All the compilation is based on this command (from [1.2.1.3](https://docs.keystone-enclave.org/en/latest/Getting-Started/QEMU-Compile-Sources.html)):
  ```BUILDROOT_TARGET=<target>-dirclean make -j$(nproc)``` where 
  - ```BUILDROOT_TARGET=<target>-dirclean``` is the targeted directory to clean
  - ```make -j$(nproc)``` is the compilation script.
- To recompile (and delete) the examples : 
  \$```BUILDROOT_TARGET=keystone-examples-dirclean make -j$(nproc)```
- To recompile the runtime:
  \$```BUILDROOT_TARGET=keystone-runtime-dirclean make -j$(nproc)```
- To compile (without deleting so a stale is possible) everything :
  \$```make -j$(nproc)```

This is contained in the [make-examples.sh](make-examples.sh) script.

#### Run

[1.2.1.4. Running and Testing Keystone on QEMU](https://docs.keystone-enclave.org/en/latest/Getting-Started/QEMU-Run-Tests.html)   

Running the emulation requires inputs and waiting time for kernel loading.

each step in order is :   
- \$```make run```   
\*wait loading and getting the **login** request\*   
- ```root```   
\***password** request\*   
- ```sifive```   
\*wait for loading\*   
- ```modprobe keystone-driver``` modprobe restarts drivers   
\*wait for driver reloading\*   
- ```/usr/share/keystone/examples/rewind.ke``` Loads the *rewind* example   
\* wait for example to finish its execution\*   
- ```poweroff``` This can be used at any time when logged in (CTRL+C doesn't work inside qemu)   
\*wait for the emulation to fully stop (so that the process is terminated)\*

This is contained in the [run-rewind.exp](run-rewind.exp) script.
In case of a CTRL+C inside the emulation. The process can't be terminated sucessfuly and we can't invoque *make run*. To terminate it use :
- \$```pkill -f qemu-system-riscv64```



### Usage

When everything is done for the [setup](#setup).  
All of the waiting time described in the [compilation & run](#compilation-and-run) part makes it painful to test simple changes and is prone to human errors (forgetting commands, misstype, forgetting to re-compile, human speed limit). This is why it is *recommended* to use the scripts instead of the commands by themselves. 

Use :
- \$```./make-examples.sh``` to rebuild the examples (**compile**)   
- \$```./run-rewind.exp``` to run the automatic qemu emulation, example launching and power off (**run**)   
- \$```./make-run.sh``` to **compile** and **run**   
- \$```pkill -f qemu-system-riscv64``` to kill the process if the emulation is exited early

**Logs**: Little reminder that logs (when compiling) can be found in the '[build-generic64/build.log](build-generic64/build.log)' (**build-\$PLATFORM$BITS/build.log**) file!

### Future Work

- Better checkpointing technique
- Own/Better cryptographic scheme
- More modular fault model (using precise statistical models)
- Improve documentation for easier implementation

### Implementing yourself

[https://docs.keystone-enclave.org/en/latest/Keystone-Applications/SDK-Basics.html#writing-a-simple-application](https://docs.keystone-enclave.org/en/latest/Keystone-Applications/SDK-Basics.html#writing-a-simple-application)

#### Edge calls (OCALLS)

[https://docs.keystone-enclave.org/en/latest/Keystone-Applications/Edge-Calls.html](https://docs.keystone-enclave.org/en/latest/Keystone-Applications/Edge-Calls.html)

#### Data Sealing
[https://docs.keystone-enclave.org/en/latest/Keystone-Applications/Data-Sealing.html](https://docs.keystone-enclave.org/en/latest/Keystone-Applications/Data-Sealing.html)

### Risks - License

This fork is made as is, building on top of it is at the own risks of the users and any negative consequence is not to be put on the authors. 

(Works on my machine ¯\\\_ツ\_/¯)
