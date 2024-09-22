# PetEngine2

## Introduction
This is my pet project where I explore various language features, technologies and concepts.
Vague final goal is a game engine with an editor capable of creating a simple 3d game.
Currently in the process of exploring low-level systems: 2d and 3d rendering, font processing, ui framework, multithreading.

## Environment and Tools
- Build system: CMake
- Platform: Windows
- Compiler: MSVC
- Third party libs: DirectX12, Doctest, FreeType, HarfBuzz, ImGui, libpng

## Project Structure and Features
- ### Modules
    - **base**: Top module with common classes. I.e. error macros, logging, math utils, ref pointers, etc.
    - **gfx**: High level 2D drawing api for ui. Uses Canvas as an abstraction that can draw rects, images, circles, lines.
    - **gfx_legacy**: Legacy 2D drawing api, DrawList and a Renderer using ImGui backend.
    - **ui**: UI framework for creating widgets in a os window. Uses gfx_legacy as a backend. Has "build"-like api to dynamically build the widget tree from WidgetState user objects.
    - **gpu**: Low lever rendering api. Currently uses only DirectX12. WGPU-like rendering api wrapping d3d12. Simplifies and abstracts some parts of d3d12 like memory management and resource tracking. Uses custom memory allocators for resources.
    - **gpu/shader**: HLSL codegen and compiler wrappers.
    - **task**: Async framework. Uses "event loop" like structures to wrap threads. Supports c++20 coroutine wrappers to simplify async callback management.

- ### Executables
    - **hello_triangle**: Draw a triangle on an OS window using gpu module api
    - **canvas_sample**: Draws some shapes via gfx::Canvas on an OS window using gpu module api
    - **freetype_example**: A cli tool testing the freetype font rasterization. Can rasterize and print ascii
    style glyphs from a provided font.

## TODO
#### UI
- Migrate to gfx::Canvas
#### GPU
- Minimal WGSL parser and hlsl transpiler to abstract backend shader language
- SwapChain resizing
- Work submission on a separate thread. To enable pipelining
