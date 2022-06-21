## Neural strategies for marketsim

To run the neural strategies, you need to install libtorch. Download the C++ version of PyTorch here:
https://pytorch.org/get-started/locally/
. Then, unzip it directly into content root, so you have `./PATH_TO_MARKETSIM/libtorch/`.

For example:
```
wget https://download.pytorch.org/libtorch/cpu/libtorch-cxx11-abi-shared-with-deps-1.11.0%2Bcpu.zip
unzip libtorch-cxx11-abi-shared-with-deps-1.11.0%2Bcpu.zip
```

`cmake` has to be version at least 3.19. If you need to use a lower version of cmake in your system, cmake can also be installed using conda.

Then, build the program:
```
cmake .
make
./testms > run.txt
```