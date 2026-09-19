# Architecture

    Renderer.mm (Metal, input, HUD, flow glue)
      |-- GameFlow     phases: video, comic, title, playing, dead, complete; checkpoints
      |-- Script       trigger volumes, cinematics, completion rule
      |-- Combat       enemy AI, hero combo, knockback, damage
      |-- Character    hero locomotion on the navmesh
      |-- Level        .irr assembly: rooms, collision, navmesh, props, markers, camera areas
      |-- BDAEModel    meshes, skins, clips, materials, texture binding
      |-- UIKitData    .bsprite and GS screen parsing
      |-- Audio        Vox event table, ADPCM decode
      '-- AudioManager AVAudioEngine playback by event name

Everything under NativePort/Sources except Renderer.mm and AudioManager.mm is
portable C++ and compiles on any host, which is what the host test suites use.
The renderer is the only file that cannot be built off-device; it is checked
structurally (balance, declared types, struct strides) and by CI syntax-only.
