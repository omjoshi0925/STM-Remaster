# .irr scene files

UTF-16LE XML (Irrlicht scene format). Each level pack has a root
levelnew_NN.irr plus one file per room, levelnew_NN_<k>_Room<k>.irr, and a
.light companion per room.

Each node has an <attributes> block with Name, Position, Rotation, Scale and
an AbsoluteTransformation, plus a user block whose !GameType names what the
node is. Attributes seen in the wild: MeshFile (also #MeshFile), !ScriptFile
on Cinematic nodes, !^Owner^CameraArea on CamCtrlPoint nodes.

World is Z up. Rotations are quaternions in conjugate convention relative to
BDAE, which is why the loader conjugates them. Paths use backslashes and
mixed case; normalise and resolve case-insensitively.

Level 1 census (1049 nodes): CamCtrlPoint 193, DestroyableObject 156, Bonus
93, Cinematic 77, Trigger 49, AnimatedObject 46, CameraArea 44, Effect 41,
enemy spawns 78, Geometry/Collisions/NavMesh/WayPoint 23 each, TriggerRestore
20, RestorePoint 20, Comic 18, CheckPoint 16, plus singletons (SpawnPoint,
BossRush, Hint, SlideCar_bus). Tools/dump_scene.py prints this for any level.
