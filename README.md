# README #

Libera Akademio is an authoring tool for creating vector format educational videos, targetted at low-cost systems (such as low-end arm devices like the Raspberry Pi) and very reduced bandwidth connections (as low as 56kbit/s).

### How to compile it: ###

Install the corresponding version of Qt, specified at the .pro file. 

Notice that compilation on Windows requires version 4.8.5, in order to have Wacom support. And therefore will require Qt to be compiled from source (unless you get a precompiled version from a third party website, since Digia does not offer binaries for this version anymore).

Other than that, install the necessary program dependencies to the executable folder: Opus Encoder/Decoder, SoundStretch, and Sox.

And you're DONE! Say whaaaaaaaa~


### Related tools: ###

A parser to create videos in our format from regular video files is coming up soon!