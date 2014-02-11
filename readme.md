SlapQ2
======
This is a source fork of Quake 2, based on version 3.21.
Mostly it's just an excuse for us to play around with some pleasantly low-level code and indulge in Quake2 nostalgia. Some of the more interesting features we've already added as of this writing include:

* Tore out the software renderer and a lot of the platform-specific code, as well as removing a lot of the hacks oriented towards accomodating old GPUs like the Voodoo2 or whatever, that we don't really care about supporting.
* Texture upscaling for modern GPUs, which makes the stock quake2 textures look an awful lot better and less smeary in GL mode.
* A new model loader that attempts to correct the animations on MD2 files to solve the "jittery vertex" problem caused by quantization of the vertexes.
* The particle system has been overhauled completely, enabling more intricate and attractive particle effects.

Thanks go to:
Robert Duffy
John Carmack
Id Software
Robert Luchi
